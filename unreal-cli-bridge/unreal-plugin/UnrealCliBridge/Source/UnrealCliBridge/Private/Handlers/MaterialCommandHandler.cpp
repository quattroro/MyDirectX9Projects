#include "MaterialCommandHandler.h"
#include "MaterialEditingLibrary.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpression.h"
#include "Materials/MaterialExpressionTextureBase.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Factories/MaterialFactoryNew.h"
#include "Factories/MaterialInstanceConstantFactoryNew.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "SceneTypes.h"
#include "MaterialDomain.h"
#include "Misc/PackageName.h"
#include "UObject/SavePackage.h"
#include "UObject/UObjectIterator.h"
#include "UObject/UnrealType.h"
#include "UObject/EnumProperty.h"

DEFINE_LOG_CATEGORY_STATIC(LogUnrealCliBridgeMaterial, Log, All);

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static IAssetRegistry& GetMaterialAssetRegistry()
{
	return FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
}

static IAssetTools& GetMaterialAssetTools()
{
	return FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
}

// "/Game/Foo/M_Bar" -> "/Game/Foo/M_Bar.M_Bar" (already-qualified paths pass through).
static FString NormalizeObjectPath(const FString& InPath)
{
	FString Path = InPath;
	Path.TrimStartAndEndInline();
	if (Path.IsEmpty()) return Path;

	int32 LastSlash = INDEX_NONE;
	Path.FindLastChar(TEXT('/'), LastSlash);
	const FString LastSegment = (LastSlash == INDEX_NONE) ? Path : Path.Mid(LastSlash + 1);
	if (LastSegment.Contains(TEXT(".")))
	{
		return Path;
	}
	return FString::Printf(TEXT("%s.%s"), *Path, *LastSegment);
}

static FString RequireStringArg(const TSharedPtr<FJsonObject>& Args, const TCHAR* Field, const TCHAR* Usage)
{
	FString Value;
	if (!Args.IsValid() || !Args->TryGetStringField(Field, Value) || Value.IsEmpty())
	{
		throw FCommandFailedException(TEXT("CLI_USAGE"), Usage);
	}
	return Value;
}

static UMaterial* LoadMaterialOrThrow(const FString& Path)
{
	UMaterial* Material = LoadObject<UMaterial>(nullptr, *NormalizeObjectPath(Path));
	if (!Material)
	{
		throw FCommandFailedException(TEXT("NOT_FOUND"),
			FString::Printf(TEXT("Material not found: %s"), *Path));
	}
	return Material;
}

// Short, human-facing type name: "MaterialExpressionTextureSample" -> "TextureSample".
static FString ShortExpressionTypeName(const UClass* Class)
{
	FString Name = Class->GetName();
	Name.RemoveFromStart(TEXT("MaterialExpression"), ESearchCase::CaseSensitive);
	return Name.IsEmpty() ? Class->GetName() : Name;
}

// Accepts "TextureSample", "MaterialExpressionTextureSample" or "texturesample".
static UClass* ResolveExpressionClass(const FString& TypeName)
{
	const FString Wanted = TypeName.TrimStartAndEnd();
	if (Wanted.IsEmpty()) return nullptr;

	UClass* Fallback = nullptr;
	for (TObjectIterator<UClass> It; It; ++It)
	{
		UClass* Class = *It;
		if (!Class->IsChildOf(UMaterialExpression::StaticClass())) continue;
		if (Class == UMaterialExpression::StaticClass()) continue;
		if (Class->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists)) continue;

		const FString ClassName = Class->GetName();
		if (ClassName.Equals(Wanted, ESearchCase::IgnoreCase)) return Class;
		if (ShortExpressionTypeName(Class).Equals(Wanted, ESearchCase::IgnoreCase)) return Class;
		// Tolerate a missing/extra prefix in either direction as a last resort.
		if (!Fallback && ClassName.Equals(TEXT("MaterialExpression") + Wanted, ESearchCase::IgnoreCase))
		{
			Fallback = Class;
		}
	}
	return Fallback;
}

// The id we report (and accept) for a node: its Desc if set, otherwise its object name.
static FString GetExpressionId(const UMaterialExpression* Expression)
{
	if (!Expression) return FString();
	return Expression->Desc.IsEmpty() ? Expression->GetName() : Expression->Desc;
}

// Resolves a node reference: Desc, parameter name, object name, GUID, or "#index".
static UMaterialExpression* FindExpression(UMaterial* Material, const FString& Ref)
{
	if (Ref.IsEmpty()) return nullptr;

	TConstArrayView<TObjectPtr<UMaterialExpression>> Expressions = Material->GetExpressions();

	if (Ref.StartsWith(TEXT("#")))
	{
		const int32 Index = FCString::Atoi(*Ref.Mid(1));
		return Expressions.IsValidIndex(Index) ? Expressions[Index].Get() : nullptr;
	}

	// Pass 1: Desc (the id we hand out).
	for (UMaterialExpression* Expression : Expressions)
	{
		if (Expression && Expression->Desc.Equals(Ref, ESearchCase::IgnoreCase)) return Expression;
	}
	// Pass 2: parameter name.
	for (UMaterialExpression* Expression : Expressions)
	{
		if (Expression && Expression->HasAParameterName()
			&& Expression->GetParameterName().ToString().Equals(Ref, ESearchCase::IgnoreCase))
		{
			return Expression;
		}
	}
	// Pass 3: object name / GUID.
	for (UMaterialExpression* Expression : Expressions)
	{
		if (!Expression) continue;
		if (Expression->GetName().Equals(Ref, ESearchCase::IgnoreCase)) return Expression;
		if (Expression->MaterialExpressionGuid.ToString().Equals(Ref, ESearchCase::IgnoreCase)) return Expression;
	}
	return nullptr;
}

static UMaterialExpression* FindExpressionOrThrow(UMaterial* Material, const FString& Ref)
{
	UMaterialExpression* Expression = FindExpression(Material, Ref);
	if (!Expression)
	{
		throw FCommandFailedException(TEXT("NOT_FOUND"),
			FString::Printf(TEXT("Node not found in material: %s. Use `material inspect` to list node ids."), *Ref));
	}
	return Expression;
}

static EMaterialProperty ParseMaterialProperty(const FString& InName)
{
	const UEnum* Enum = StaticEnum<EMaterialProperty>();
	if (Enum)
	{
		// Accept both "MP_BaseColor" and "BaseColor".
		for (int32 i = 0; i < Enum->NumEnums() - 1; ++i)
		{
			const FString EntryName = Enum->GetNameStringByIndex(i);
			if (EntryName.Equals(InName, ESearchCase::IgnoreCase)) return static_cast<EMaterialProperty>(Enum->GetValueByIndex(i));

			FString Trimmed = EntryName;
			Trimmed.RemoveFromStart(TEXT("MP_"), ESearchCase::CaseSensitive);
			if (Trimmed.Equals(InName, ESearchCase::IgnoreCase)) return static_cast<EMaterialProperty>(Enum->GetValueByIndex(i));
		}
	}

	// A couple of names people reach for that don't match the enum spelling.
	if (InName.Equals(TEXT("Emissive"), ESearchCase::IgnoreCase)) return MP_EmissiveColor;
	if (InName.Equals(TEXT("Normal"), ESearchCase::IgnoreCase)) return MP_Normal;

	throw FCommandFailedException(TEXT("CLI_USAGE"),
		FString::Printf(TEXT("Unknown material property: %s. Examples: BaseColor, Metallic, Specular, Roughness, "
			"EmissiveColor, Opacity, OpacityMask, Normal, WorldPositionOffset, AmbientOcclusion, Refraction"), *InName));
}

// JSON -> a string ImportText can parse for the given property.
static FString JsonToPropertyText(const FProperty* Property, const TSharedPtr<FJsonValue>& Value);

static FString JsonObjectToStructText(const TSharedPtr<FJsonObject>& Object)
{
	TArray<FString> Parts;
	for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : Object->Values)
	{
		Parts.Add(FString::Printf(TEXT("%s=%s"), *Pair.Key, *JsonToPropertyText(nullptr, Pair.Value)));
	}
	return FString::Printf(TEXT("(%s)"), *FString::Join(Parts, TEXT(",")));
}

// Numeric arrays map onto the usual vector/color struct field names.
static FString JsonArrayToStructText(const FProperty* Property, const TArray<TSharedPtr<FJsonValue>>& Items)
{
	const FStructProperty* StructProp = CastField<const FStructProperty>(Property);
	const FName StructName = (StructProp && StructProp->Struct) ? StructProp->Struct->GetFName() : NAME_None;

	const TCHAR* Fields[4] = { TEXT("X"), TEXT("Y"), TEXT("Z"), TEXT("W") };
	if (StructName == TEXT("LinearColor") || StructName == TEXT("Color"))
	{
		Fields[0] = TEXT("R"); Fields[1] = TEXT("G"); Fields[2] = TEXT("B"); Fields[3] = TEXT("A");
	}

	TArray<FString> Parts;
	for (int32 i = 0; i < Items.Num(); ++i)
	{
		const FString Text = JsonToPropertyText(nullptr, Items[i]);
		Parts.Add(i < 4 ? FString::Printf(TEXT("%s=%s"), Fields[i], *Text) : Text);
	}
	return FString::Printf(TEXT("(%s)"), *FString::Join(Parts, TEXT(",")));
}

static FString JsonToPropertyText(const FProperty* Property, const TSharedPtr<FJsonValue>& Value)
{
	if (!Value.IsValid()) return FString();

	switch (Value->Type)
	{
	case EJson::String:  return Value->AsString();
	case EJson::Boolean: return Value->AsBool() ? TEXT("true") : TEXT("false");
	case EJson::Number:  return FString::Printf(TEXT("%g"), Value->AsNumber());
	case EJson::Object:  return JsonObjectToStructText(Value->AsObject());
	case EJson::Array:   return JsonArrayToStructText(Property, Value->AsArray());
	default:             return FString();
	}
}

// Applies a {"PropertyName": value} object onto any UObject via reflection.
static void ApplyValues(UObject* Target, const TSharedPtr<FJsonObject>& Values)
{
	if (!Target || !Values.IsValid()) return;

	Target->Modify();

	for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : Values->Values)
	{
		FProperty* Property = FindFProperty<FProperty>(Target->GetClass(), *Pair.Key);
		if (!Property)
		{
			throw FCommandFailedException(TEXT("NOT_FOUND"),
				FString::Printf(TEXT("'%s' has no property named '%s'."), *Target->GetClass()->GetName(), *Pair.Key));
		}

		void* ValuePtr = Property->ContainerPtrToValuePtr<void>(Target);

		// Object references (textures, material functions, ...) are given as content paths.
		if (FObjectPropertyBase* ObjectProperty = CastField<FObjectPropertyBase>(Property))
		{
			const FString AssetPath = Pair.Value->AsString();
			if (AssetPath.IsEmpty() || AssetPath.Equals(TEXT("None"), ESearchCase::IgnoreCase))
			{
				ObjectProperty->SetObjectPropertyValue(ValuePtr, nullptr);
				continue;
			}

			UObject* Asset = StaticLoadObject(ObjectProperty->PropertyClass, nullptr, *NormalizeObjectPath(AssetPath));
			if (!Asset)
			{
				throw FCommandFailedException(TEXT("NOT_FOUND"),
					FString::Printf(TEXT("Could not load %s asset for property '%s': %s"),
						*ObjectProperty->PropertyClass->GetName(), *Pair.Key, *AssetPath));
			}
			ObjectProperty->SetObjectPropertyValue(ValuePtr, Asset);
			continue;
		}

		const FString Text = JsonToPropertyText(Property, Pair.Value);
		if (!Property->ImportText_Direct(*Text, ValuePtr, Target, PPF_None))
		{
			throw FCommandFailedException(TEXT("OPERATION_FAILED"),
				FString::Printf(TEXT("Could not assign '%s' to property '%s' (type %s)."),
					*Text, *Pair.Key, *Property->GetCPPType()));
		}
	}

	// Texture nodes derive their sampler type from the assigned texture.
	if (UMaterialExpressionTextureBase* TextureExpression = Cast<UMaterialExpressionTextureBase>(Target))
	{
		TextureExpression->AutoSetSampleType();
	}

	Target->PostEditChange();
}

// Writes the package straight to disk. FEditorFileUtils/UEditorLoadingAndSavingUtils are not an
// option here: they pop a modal source-control dialog, which would hang a CLI call indefinitely.
static bool SaveAssetPackage(UObject* Asset)
{
	UPackage* Package = Asset->GetOutermost();
	Package->SetDirtyFlag(true);

	FString Filename;
	if (!FPackageName::TryConvertLongPackageNameToFilename(
		Package->GetName(), Filename, FPackageName::GetAssetPackageExtension()))
	{
		return false;
	}

	FSavePackageArgs SaveArgs;
	SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
	SaveArgs.SaveFlags = SAVE_NoError;
	SaveArgs.Error = GLog;
	return UPackage::SavePackage(Package, Asset, *Filename, SaveArgs);
}

// Every mutating command ends here: recompile so the change is visible, optionally save.
static void FinishMaterialEdit(UMaterial* Material, const TSharedPtr<FJsonObject>& Args, TSharedPtr<FJsonObject>& OutData)
{
	bool bNoCompile = false;
	bool bSave = false;
	if (Args.IsValid())
	{
		Args->TryGetBoolField(TEXT("noCompile"), bNoCompile);
		Args->TryGetBoolField(TEXT("save"), bSave);
	}

	Material->MarkPackageDirty();
	if (bNoCompile)
	{
		Material->PreEditChange(nullptr);
		Material->PostEditChange();
	}
	else
	{
		UMaterialEditingLibrary::RecompileMaterial(Material);
	}

	if (bSave)
	{
		OutData->SetBoolField(TEXT("saved"), SaveAssetPackage(Material));
	}
	OutData->SetBoolField(TEXT("recompiled"), !bNoCompile);
}

// Unnamed pins come back as "None"; report them as empty, which is also how the connect
// commands spell "the node's only/first pin".
static FString CleanPinName(const FString& Name)
{
	return Name.Equals(TEXT("None"), ESearchCase::IgnoreCase) ? FString() : Name;
}

static TSharedPtr<FJsonObject> ExpressionToJson(UMaterial* Material, UMaterialExpression* Expression, bool bWithValues)
{
	TSharedPtr<FJsonObject> Node = MakeShared<FJsonObject>();
	Node->SetStringField(TEXT("id"), GetExpressionId(Expression));
	Node->SetStringField(TEXT("type"), ShortExpressionTypeName(Expression->GetClass()));
	Node->SetStringField(TEXT("class"), Expression->GetClass()->GetName());
	Node->SetStringField(TEXT("objectName"), Expression->GetName());
	if (Expression->HasAParameterName())
	{
		Node->SetStringField(TEXT("parameterName"), Expression->GetParameterName().ToString());
	}

	TArray<TSharedPtr<FJsonValue>> Pos;
	Pos.Add(MakeShared<FJsonValueNumber>(Expression->MaterialExpressionEditorX));
	Pos.Add(MakeShared<FJsonValueNumber>(Expression->MaterialExpressionEditorY));
	Node->SetArrayField(TEXT("pos"), Pos);

	// Inputs, with whatever is currently plugged into them.
	const TArray<FString> InputNames = UMaterialEditingLibrary::GetMaterialExpressionInputNames(Expression);
	TArrayView<FExpressionInput*> Inputs = Expression->GetInputsView();
	TArray<TSharedPtr<FJsonValue>> InputArray;
	for (int32 i = 0; i < Inputs.Num(); ++i)
	{
		TSharedPtr<FJsonObject> InputObj = MakeShared<FJsonObject>();
		InputObj->SetStringField(TEXT("name"), InputNames.IsValidIndex(i) ? CleanPinName(InputNames[i]) : FString());
		UMaterialExpression* Source = Inputs[i] ? Inputs[i]->Expression : nullptr;
		if (Source)
		{
			InputObj->SetStringField(TEXT("from"), GetExpressionId(Source));
			if (Source->Outputs.IsValidIndex(Inputs[i]->OutputIndex))
			{
				InputObj->SetStringField(TEXT("fromOutput"), CleanPinName(Source->Outputs[Inputs[i]->OutputIndex].OutputName.ToString()));
			}
		}
		InputArray.Add(MakeShared<FJsonValueObject>(InputObj));
	}
	Node->SetArrayField(TEXT("inputs"), InputArray);

	TArray<TSharedPtr<FJsonValue>> OutputArray;
	for (const FExpressionOutput& Output : Expression->Outputs)
	{
		OutputArray.Add(MakeShared<FJsonValueString>(CleanPinName(Output.OutputName.ToString())));
	}
	Node->SetArrayField(TEXT("outputs"), OutputArray);

	if (bWithValues)
	{
		TSharedPtr<FJsonObject> ValueObj = MakeShared<FJsonObject>();
		for (TFieldIterator<FProperty> PropIt(Expression->GetClass()); PropIt; ++PropIt)
		{
			FProperty* Property = *PropIt;
			if (!Property->HasAnyPropertyFlags(CPF_Edit)) continue;
			if (Property->IsA<FStructProperty>()
				&& CastField<FStructProperty>(Property)->Struct
				&& CastField<FStructProperty>(Property)->Struct->GetFName() == TEXT("ExpressionInput"))
			{
				continue; // connections are already reported above
			}
			FString ValueStr;
			Property->ExportTextItem_Direct(ValueStr, Property->ContainerPtrToValuePtr<void>(Expression), nullptr, Expression, PPF_None);
			ValueObj->SetStringField(Property->GetName(), ValueStr);
		}
		Node->SetObjectField(TEXT("values"), ValueObj);
	}

	return Node;
}

// ---------------------------------------------------------------------------
// Registration
// ---------------------------------------------------------------------------

void FMaterialCommandHandler::RegisterAll(FCommandDispatcher& Dispatcher)
{
	Dispatcher.Register(TEXT("material.create"),             &HandleCreate);
	Dispatcher.Register(TEXT("material.inspect"),            &HandleInspect);
	Dispatcher.Register(TEXT("material.list-node-types"),    &HandleListNodeTypes);
	Dispatcher.Register(TEXT("material.add-node"),           &HandleAddNode);
	Dispatcher.Register(TEXT("material.set-node"),           &HandleSetNode);
	Dispatcher.Register(TEXT("material.connect"),            &HandleConnect);
	Dispatcher.Register(TEXT("material.disconnect"),         &HandleDisconnect);
	Dispatcher.Register(TEXT("material.delete-node"),        &HandleDeleteNode);
	Dispatcher.Register(TEXT("material.set-property"),       &HandleSetProperty);
	Dispatcher.Register(TEXT("material.apply-graph"),        &HandleApplyGraph);
	Dispatcher.Register(TEXT("material.compile"),            &HandleCompile);
	Dispatcher.Register(TEXT("material.create-instance"),    &HandleCreateInstance);
	Dispatcher.Register(TEXT("material.set-instance-param"), &HandleSetInstanceParam);
}

// ---------------------------------------------------------------------------
// material create
// ---------------------------------------------------------------------------

// Friendly aliases so callers don't have to know the engine enum spellings.
static void ApplyMaterialPreset(UMaterial* Material, const TSharedPtr<FJsonObject>& Args)
{
	FString Domain, Blend, Shading;
	bool bTwoSided = false;

	if (Args.IsValid())
	{
		Args->TryGetStringField(TEXT("domain"), Domain);
		Args->TryGetStringField(TEXT("blend"), Blend);
		Args->TryGetStringField(TEXT("shading"), Shading);
		Args->TryGetBoolField(TEXT("twoSided"), bTwoSided);
	}

	if (!Domain.IsEmpty())
	{
		const FString D = Domain.ToLower();
		if      (D == TEXT("surface"))       Material->MaterialDomain = MD_Surface;
		else if (D == TEXT("deferreddecal")) Material->MaterialDomain = MD_DeferredDecal;
		else if (D == TEXT("lightfunction")) Material->MaterialDomain = MD_LightFunction;
		else if (D == TEXT("volume"))        Material->MaterialDomain = MD_Volume;
		else if (D == TEXT("postprocess"))   Material->MaterialDomain = MD_PostProcess;
		else if (D == TEXT("ui"))            Material->MaterialDomain = MD_UI;
		else throw FCommandFailedException(TEXT("CLI_USAGE"),
			FString::Printf(TEXT("Unknown domain '%s'. Supported: surface, deferreddecal, lightfunction, volume, postprocess, ui"), *Domain));
	}

	if (!Blend.IsEmpty())
	{
		const FString B = Blend.ToLower();
		if      (B == TEXT("opaque"))         Material->BlendMode = BLEND_Opaque;
		else if (B == TEXT("masked"))         Material->BlendMode = BLEND_Masked;
		else if (B == TEXT("translucent"))    Material->BlendMode = BLEND_Translucent;
		else if (B == TEXT("additive"))       Material->BlendMode = BLEND_Additive;
		else if (B == TEXT("modulate"))       Material->BlendMode = BLEND_Modulate;
		else if (B == TEXT("alphacomposite")) Material->BlendMode = BLEND_AlphaComposite;
		else if (B == TEXT("alphaholdout"))   Material->BlendMode = BLEND_AlphaHoldout;
		else throw FCommandFailedException(TEXT("CLI_USAGE"),
			FString::Printf(TEXT("Unknown blend mode '%s'. Supported: opaque, masked, translucent, additive, modulate, alphacomposite, alphaholdout"), *Blend));
	}

	if (!Shading.IsEmpty())
	{
		const FString S = Shading.ToLower();
		if      (S == TEXT("unlit"))              Material->SetShadingModel(MSM_Unlit);
		else if (S == TEXT("defaultlit"))         Material->SetShadingModel(MSM_DefaultLit);
		else if (S == TEXT("subsurface"))         Material->SetShadingModel(MSM_Subsurface);
		else if (S == TEXT("preintegratedskin"))  Material->SetShadingModel(MSM_PreintegratedSkin);
		else if (S == TEXT("clearcoat"))          Material->SetShadingModel(MSM_ClearCoat);
		else if (S == TEXT("subsurfaceprofile"))  Material->SetShadingModel(MSM_SubsurfaceProfile);
		else if (S == TEXT("twosidedfoliage"))    Material->SetShadingModel(MSM_TwoSidedFoliage);
		else if (S == TEXT("hair"))               Material->SetShadingModel(MSM_Hair);
		else if (S == TEXT("cloth"))              Material->SetShadingModel(MSM_Cloth);
		else if (S == TEXT("eye"))                Material->SetShadingModel(MSM_Eye);
		else if (S == TEXT("singlelayerwater"))   Material->SetShadingModel(MSM_SingleLayerWater);
		else if (S == TEXT("thintranslucent"))    Material->SetShadingModel(MSM_ThinTranslucent);
		else throw FCommandFailedException(TEXT("CLI_USAGE"),
			FString::Printf(TEXT("Unknown shading model '%s'. Supported: unlit, defaultlit, subsurface, preintegratedskin, clearcoat, "
				"subsurfaceprofile, twosidedfoliage, hair, cloth, eye, singlelayerwater, thintranslucent"), *Shading));
	}

	if (bTwoSided)
	{
		Material->TwoSided = 1;
	}
}

TSharedPtr<FJsonObject> FMaterialCommandHandler::HandleCreate(const TSharedPtr<FJsonObject>& Args, bool bForce)
{
	const FString Path = RequireStringArg(Args, TEXT("path"), TEXT("--path is required."));

	const FString ObjectPath = NormalizeObjectPath(Path);
	const FAssetData Existing = GetMaterialAssetRegistry().GetAssetByObjectPath(FSoftObjectPath(ObjectPath));
	if (Existing.IsValid() && !bForce)
	{
		throw FCommandFailedException(TEXT("FORCE_REQUIRED"),
			FString::Printf(TEXT("Asset already exists: %s. Use --force to overwrite."), *Path));
	}

	const FString PackageName = FPackageName::ObjectPathToPackageName(ObjectPath);
	const FString AssetName   = FPackageName::GetLongPackageAssetName(PackageName);
	const FString PackagePath = FPackageName::GetLongPackagePath(PackageName);

	UMaterialFactoryNew* Factory = NewObject<UMaterialFactoryNew>();
	UMaterial* Material = Cast<UMaterial>(
		GetMaterialAssetTools().CreateAsset(AssetName, PackagePath, UMaterial::StaticClass(), Factory));
	if (!Material)
	{
		throw FCommandFailedException(TEXT("OPERATION_FAILED"), TEXT("Failed to create material."));
	}

	ApplyMaterialPreset(Material, Args);

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("path"), ObjectPath);
	Data->SetStringField(TEXT("message"), TEXT("Material created."));
	FinishMaterialEdit(Material, Args, Data);
	return Data;
}

// ---------------------------------------------------------------------------
// material inspect
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> FMaterialCommandHandler::HandleInspect(const TSharedPtr<FJsonObject>& Args, bool bForce)
{
	const FString Path = RequireStringArg(Args, TEXT("path"), TEXT("--path is required."));
	bool bWithValues = false;
	Args->TryGetBoolField(TEXT("withValues"), bWithValues);

	UMaterial* Material = LoadMaterialOrThrow(Path);

	TArray<TSharedPtr<FJsonValue>> Nodes;
	for (UMaterialExpression* Expression : Material->GetExpressions())
	{
		if (!Expression) continue;
		Nodes.Add(MakeShared<FJsonValueObject>(ExpressionToJson(Material, Expression, bWithValues)));
	}

	// Which node feeds each material output pin.
	TArray<TSharedPtr<FJsonValue>> Outputs;
	const UEnum* PropertyEnum = StaticEnum<EMaterialProperty>();
	for (int32 i = 0; i < MP_MAX; ++i)
	{
		const EMaterialProperty Property = static_cast<EMaterialProperty>(i);
		// Deliberately not UMaterialEditingLibrary::GetMaterialPropertyInputNode(): it dereferences
		// the input without a null check, and hidden properties have none, so it crashes the editor.
		FExpressionInput* Input = Material->GetExpressionInputForProperty(Property);
		if (!Input || !Input->Expression) continue;

		TSharedPtr<FJsonObject> OutObj = MakeShared<FJsonObject>();
		FString PropertyName = PropertyEnum ? PropertyEnum->GetNameStringByValue(i) : FString::FromInt(i);
		PropertyName.RemoveFromStart(TEXT("MP_"), ESearchCase::CaseSensitive);
		OutObj->SetStringField(TEXT("property"), PropertyName);
		OutObj->SetStringField(TEXT("from"), GetExpressionId(Input->Expression));
		if (Input->Expression->Outputs.IsValidIndex(Input->OutputIndex))
		{
			OutObj->SetStringField(TEXT("fromOutput"),
				CleanPinName(Input->Expression->Outputs[Input->OutputIndex].OutputName.ToString()));
		}
		Outputs.Add(MakeShared<FJsonValueObject>(OutObj));
	}

	TSharedPtr<FJsonObject> Settings = MakeShared<FJsonObject>();
	Settings->SetStringField(TEXT("domain"), StaticEnum<EMaterialDomain>()
		? StaticEnum<EMaterialDomain>()->GetNameStringByValue(Material->MaterialDomain) : TEXT(""));
	Settings->SetStringField(TEXT("blendMode"), StaticEnum<EBlendMode>()
		? StaticEnum<EBlendMode>()->GetNameStringByValue(Material->BlendMode) : TEXT(""));
	const FMaterialShadingModelField ShadingModels = Material->GetShadingModels();
	Settings->SetStringField(TEXT("shadingModel"), (ShadingModels.IsValid() && StaticEnum<EMaterialShadingModel>())
		? StaticEnum<EMaterialShadingModel>()->GetNameStringByValue(ShadingModels.GetFirstShadingModel()) : TEXT(""));
	Settings->SetBoolField(TEXT("twoSided"), Material->TwoSided != 0);

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("path"), NormalizeObjectPath(Path));
	Data->SetObjectField(TEXT("settings"), Settings);
	Data->SetArrayField(TEXT("nodes"), Nodes);
	Data->SetArrayField(TEXT("outputs"), Outputs);
	Data->SetNumberField(TEXT("nodeCount"), Nodes.Num());
	return Data;
}

// ---------------------------------------------------------------------------
// material list-node-types
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> FMaterialCommandHandler::HandleListNodeTypes(const TSharedPtr<FJsonObject>& Args, bool bForce)
{
	FString Filter;
	int32 Limit = 200;
	if (Args.IsValid())
	{
		Args->TryGetStringField(TEXT("filter"), Filter);
		Args->TryGetNumberField(TEXT("limit"), Limit);
	}

	TArray<TSharedPtr<FJsonValue>> Types;
	int32 TotalMatches = 0;
	for (TObjectIterator<UClass> It; It; ++It)
	{
		UClass* Class = *It;
		if (!Class->IsChildOf(UMaterialExpression::StaticClass())) continue;
		if (Class == UMaterialExpression::StaticClass()) continue;
		if (Class->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists)) continue;

		const FString ShortName = ShortExpressionTypeName(Class);
		if (!Filter.IsEmpty() && !ShortName.Contains(Filter, ESearchCase::IgnoreCase)) continue;

		++TotalMatches;
		if (Types.Num() >= Limit) continue;

		TSharedPtr<FJsonObject> TypeObj = MakeShared<FJsonObject>();
		TypeObj->SetStringField(TEXT("type"), ShortName);
		TypeObj->SetStringField(TEXT("class"), Class->GetName());

		if (const UMaterialExpression* CDO = Class->GetDefaultObject<UMaterialExpression>())
		{
			TArray<TSharedPtr<FJsonValue>> Categories;
			for (const FText& Category : CDO->MenuCategories)
			{
				Categories.Add(MakeShared<FJsonValueString>(Category.ToString()));
			}
			TypeObj->SetArrayField(TEXT("categories"), Categories);
		}

		Types.Add(MakeShared<FJsonValueObject>(TypeObj));
	}

	Types.Sort([](const TSharedPtr<FJsonValue>& A, const TSharedPtr<FJsonValue>& B)
	{
		return A->AsObject()->GetStringField(TEXT("type")).Compare(
			B->AsObject()->GetStringField(TEXT("type")), ESearchCase::IgnoreCase) < 0;
	});

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetArrayField(TEXT("types"), Types);
	Data->SetNumberField(TEXT("count"), Types.Num());
	Data->SetNumberField(TEXT("totalMatches"), TotalMatches);
	return Data;
}

// ---------------------------------------------------------------------------
// material add-node / set-node / delete-node
// ---------------------------------------------------------------------------

// Shared by add-node and apply-graph.
static UMaterialExpression* CreateNode(
	UMaterial* Material,
	const FString& TypeName,
	const FString& NodeId,
	int32 PosX,
	int32 PosY,
	const TSharedPtr<FJsonObject>& Values)
{
	UClass* ExpressionClass = ResolveExpressionClass(TypeName);
	if (!ExpressionClass)
	{
		throw FCommandFailedException(TEXT("NOT_FOUND"),
			FString::Printf(TEXT("Unknown material node type: %s. Use `material list-node-types --filter <term>` to search."), *TypeName));
	}

	UMaterialExpression* Expression = UMaterialEditingLibrary::CreateMaterialExpression(Material, ExpressionClass, PosX, PosY);
	if (!Expression)
	{
		throw FCommandFailedException(TEXT("OPERATION_FAILED"),
			FString::Printf(TEXT("Failed to create node of type %s."), *TypeName));
	}

	// Desc doubles as the node's stable, human-readable id (shown in the graph as a comment).
	if (!NodeId.IsEmpty())
	{
		Expression->Desc = NodeId;
	}

	if (Values.IsValid() && Values->Values.Num() > 0)
	{
		ApplyValues(Expression, Values);
	}

	return Expression;
}

static void ParsePos(const TSharedPtr<FJsonObject>& Args, const TCHAR* Field, int32& OutX, int32& OutY, bool& bOutHasPos)
{
	bOutHasPos = false;
	const TArray<TSharedPtr<FJsonValue>>* PosArray = nullptr;
	if (Args.IsValid() && Args->TryGetArrayField(Field, PosArray) && PosArray->Num() >= 2)
	{
		OutX = static_cast<int32>((*PosArray)[0]->AsNumber());
		OutY = static_cast<int32>((*PosArray)[1]->AsNumber());
		bOutHasPos = true;
	}
}

TSharedPtr<FJsonObject> FMaterialCommandHandler::HandleAddNode(const TSharedPtr<FJsonObject>& Args, bool bForce)
{
	const FString Path = RequireStringArg(Args, TEXT("path"), TEXT("--path is required."));
	const FString Type = RequireStringArg(Args, TEXT("type"), TEXT("--type is required."));
	FString NodeId;
	Args->TryGetStringField(TEXT("name"), NodeId);

	UMaterial* Material = LoadMaterialOrThrow(Path);

	if (!NodeId.IsEmpty() && FindExpression(Material, NodeId) && !bForce)
	{
		throw FCommandFailedException(TEXT("FORCE_REQUIRED"),
			FString::Printf(TEXT("A node named '%s' already exists in this material. Use --force to add a duplicate."), *NodeId));
	}

	int32 PosX = 0, PosY = 0;
	bool bHasPos = false;
	ParsePos(Args, TEXT("pos"), PosX, PosY, bHasPos);

	const TSharedPtr<FJsonObject>* ValuesPtr = nullptr;
	Args->TryGetObjectField(TEXT("values"), ValuesPtr);

	Material->Modify();
	UMaterialExpression* Expression = CreateNode(Material, Type, NodeId, PosX, PosY,
		ValuesPtr ? *ValuesPtr : nullptr);

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("path"), NormalizeObjectPath(Path));
	Data->SetObjectField(TEXT("node"), ExpressionToJson(Material, Expression, false));
	Data->SetStringField(TEXT("message"), TEXT("Node added."));
	FinishMaterialEdit(Material, Args, Data);
	return Data;
}

TSharedPtr<FJsonObject> FMaterialCommandHandler::HandleSetNode(const TSharedPtr<FJsonObject>& Args, bool bForce)
{
	const FString Path   = RequireStringArg(Args, TEXT("path"), TEXT("--path is required."));
	const FString NodeId = RequireStringArg(Args, TEXT("node"), TEXT("--node is required."));

	UMaterial* Material = LoadMaterialOrThrow(Path);
	UMaterialExpression* Expression = FindExpressionOrThrow(Material, NodeId);

	const TSharedPtr<FJsonObject>* ValuesPtr = nullptr;
	if (!Args->TryGetObjectField(TEXT("values"), ValuesPtr) || !ValuesPtr)
	{
		throw FCommandFailedException(TEXT("CLI_USAGE"), TEXT("--values <json> is required."));
	}

	Material->Modify();
	ApplyValues(Expression, *ValuesPtr);

	int32 PosX = 0, PosY = 0;
	bool bHasPos = false;
	ParsePos(Args, TEXT("pos"), PosX, PosY, bHasPos);
	if (bHasPos)
	{
		Expression->MaterialExpressionEditorX = PosX;
		Expression->MaterialExpressionEditorY = PosY;
	}

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("path"), NormalizeObjectPath(Path));
	Data->SetObjectField(TEXT("node"), ExpressionToJson(Material, Expression, true));
	Data->SetStringField(TEXT("message"), TEXT("Node updated."));
	FinishMaterialEdit(Material, Args, Data);
	return Data;
}

TSharedPtr<FJsonObject> FMaterialCommandHandler::HandleDeleteNode(const TSharedPtr<FJsonObject>& Args, bool bForce)
{
	if (!bForce)
	{
		throw FCommandFailedException(TEXT("FORCE_REQUIRED"), TEXT("material delete-node always requires --force."));
	}

	const FString Path   = RequireStringArg(Args, TEXT("path"), TEXT("--path is required."));
	const FString NodeId = RequireStringArg(Args, TEXT("node"), TEXT("--node is required."));

	UMaterial* Material = LoadMaterialOrThrow(Path);
	UMaterialExpression* Expression = FindExpressionOrThrow(Material, NodeId);
	const FString ResolvedId = GetExpressionId(Expression);

	Material->Modify();
	UMaterialEditingLibrary::DeleteMaterialExpression(Material, Expression);

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("path"), NormalizeObjectPath(Path));
	Data->SetStringField(TEXT("node"), ResolvedId);
	Data->SetStringField(TEXT("message"), TEXT("Node deleted."));
	FinishMaterialEdit(Material, Args, Data);
	return Data;
}

// ---------------------------------------------------------------------------
// material connect / disconnect
// ---------------------------------------------------------------------------

// One connection: node -> node input, or node -> material output property.
static void ConnectOne(
	UMaterial* Material,
	const FString& FromRef,
	const FString& FromOutput,
	const FString& ToRef,
	const FString& ToInput,
	const FString& PropertyName)
{
	UMaterialExpression* From = FindExpressionOrThrow(Material, FromRef);

	if (!PropertyName.IsEmpty())
	{
		const EMaterialProperty Property = ParseMaterialProperty(PropertyName);
		if (!UMaterialEditingLibrary::ConnectMaterialProperty(From, FromOutput, Property))
		{
			throw FCommandFailedException(TEXT("OPERATION_FAILED"),
				FString::Printf(TEXT("Could not connect '%s'%s to material property '%s'. "
					"Check the output name with `material inspect`."),
					*FromRef,
					FromOutput.IsEmpty() ? TEXT("") : *FString::Printf(TEXT(" output '%s'"), *FromOutput),
					*PropertyName));
		}
		return;
	}

	if (ToRef.IsEmpty())
	{
		throw FCommandFailedException(TEXT("CLI_USAGE"), TEXT("Either --to (with --to-input) or --property is required."));
	}

	UMaterialExpression* To = FindExpressionOrThrow(Material, ToRef);
	if (!UMaterialEditingLibrary::ConnectMaterialExpressions(From, FromOutput, To, ToInput))
	{
		// Spell out the target's pins: unnamed ones are the usual reason a --to-input guess fails.
		TArray<FString> InputNames = UMaterialEditingLibrary::GetMaterialExpressionInputNames(To);
		for (FString& Name : InputNames)
		{
			Name = CleanPinName(Name);
			if (Name.IsEmpty()) Name = TEXT("<unnamed: omit --to-input>");
		}

		throw FCommandFailedException(TEXT("OPERATION_FAILED"),
			FString::Printf(TEXT("Could not connect '%s' -> '%s'%s. Inputs of '%s': %s"),
				*FromRef, *ToRef,
				ToInput.IsEmpty() ? TEXT("") : *FString::Printf(TEXT(" input '%s'"), *ToInput),
				*ToRef,
				InputNames.Num() > 0 ? *FString::Join(InputNames, TEXT(", ")) : TEXT("(none)")));
	}
}

TSharedPtr<FJsonObject> FMaterialCommandHandler::HandleConnect(const TSharedPtr<FJsonObject>& Args, bool bForce)
{
	const FString Path     = RequireStringArg(Args, TEXT("path"), TEXT("--path is required."));
	const FString FromRef  = RequireStringArg(Args, TEXT("from"), TEXT("--from is required."));
	FString FromOutput, ToRef, ToInput, PropertyName;
	Args->TryGetStringField(TEXT("fromOutput"), FromOutput);
	Args->TryGetStringField(TEXT("to"), ToRef);
	Args->TryGetStringField(TEXT("toInput"), ToInput);
	Args->TryGetStringField(TEXT("property"), PropertyName);

	UMaterial* Material = LoadMaterialOrThrow(Path);
	Material->Modify();
	ConnectOne(Material, FromRef, FromOutput, ToRef, ToInput, PropertyName);

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("path"), NormalizeObjectPath(Path));
	Data->SetStringField(TEXT("from"), FromRef);
	if (!PropertyName.IsEmpty()) Data->SetStringField(TEXT("property"), PropertyName);
	else                         Data->SetStringField(TEXT("to"), ToRef);
	Data->SetStringField(TEXT("message"), TEXT("Connected."));
	FinishMaterialEdit(Material, Args, Data);
	return Data;
}

TSharedPtr<FJsonObject> FMaterialCommandHandler::HandleDisconnect(const TSharedPtr<FJsonObject>& Args, bool bForce)
{
	const FString Path = RequireStringArg(Args, TEXT("path"), TEXT("--path is required."));
	FString ToRef, ToInput, PropertyName;
	Args->TryGetStringField(TEXT("to"), ToRef);
	Args->TryGetStringField(TEXT("toInput"), ToInput);
	Args->TryGetStringField(TEXT("property"), PropertyName);

	UMaterial* Material = LoadMaterialOrThrow(Path);
	Material->Modify();

	FExpressionInput* Input = nullptr;
	if (!PropertyName.IsEmpty())
	{
		Input = Material->GetExpressionInputForProperty(ParseMaterialProperty(PropertyName));
	}
	else if (!ToRef.IsEmpty())
	{
		UMaterialExpression* To = FindExpressionOrThrow(Material, ToRef);
		TArrayView<FExpressionInput*> Inputs = To->GetInputsView();
		const TArray<FString> InputNames = UMaterialEditingLibrary::GetMaterialExpressionInputNames(To);
		if (ToInput.IsEmpty())
		{
			Input = Inputs.Num() > 0 ? Inputs[0] : nullptr;
		}
		else
		{
			for (int32 i = 0; i < Inputs.Num(); ++i)
			{
				if (InputNames.IsValidIndex(i) && InputNames[i].Equals(ToInput, ESearchCase::IgnoreCase))
				{
					Input = Inputs[i];
					break;
				}
			}
		}
	}
	else
	{
		throw FCommandFailedException(TEXT("CLI_USAGE"), TEXT("Either --to or --property is required."));
	}

	if (!Input)
	{
		throw FCommandFailedException(TEXT("NOT_FOUND"), TEXT("Could not resolve the input pin to disconnect."));
	}

	Input->Expression = nullptr;
	Input->OutputIndex = 0;

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("path"), NormalizeObjectPath(Path));
	Data->SetStringField(TEXT("message"), TEXT("Disconnected."));
	FinishMaterialEdit(Material, Args, Data);
	return Data;
}

// ---------------------------------------------------------------------------
// material set-property
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> FMaterialCommandHandler::HandleSetProperty(const TSharedPtr<FJsonObject>& Args, bool bForce)
{
	const FString Path = RequireStringArg(Args, TEXT("path"), TEXT("--path is required."));
	UMaterial* Material = LoadMaterialOrThrow(Path);

	FString PropertyName, Value;
	const bool bHasSingle = Args->TryGetStringField(TEXT("property"), PropertyName)
		&& Args->TryGetStringField(TEXT("value"), Value);

	const TSharedPtr<FJsonObject>* ValuesPtr = nullptr;
	const bool bHasValues = Args->TryGetObjectField(TEXT("values"), ValuesPtr) && ValuesPtr;

	// The friendly --domain/--blend/--shading/--two-sided flags work here too.
	const bool bHasPreset = Args->HasField(TEXT("domain")) || Args->HasField(TEXT("blend"))
		|| Args->HasField(TEXT("shading")) || Args->HasField(TEXT("twoSided"));

	if (!bHasSingle && !bHasValues && !bHasPreset)
	{
		throw FCommandFailedException(TEXT("CLI_USAGE"),
			TEXT("Provide --property with --value, or --values <json>, or one of --domain/--blend/--shading/--two-sided."));
	}

	Material->Modify();
	ApplyMaterialPreset(Material, Args);

	if (bHasSingle)
	{
		TSharedPtr<FJsonObject> Single = MakeShared<FJsonObject>();
		Single->SetStringField(PropertyName, Value);
		ApplyValues(Material, Single);
	}
	if (bHasValues)
	{
		ApplyValues(Material, *ValuesPtr);
	}

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("path"), NormalizeObjectPath(Path));
	Data->SetStringField(TEXT("message"), TEXT("Material settings updated."));
	FinishMaterialEdit(Material, Args, Data);
	return Data;
}

// ---------------------------------------------------------------------------
// material apply-graph
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> FMaterialCommandHandler::HandleApplyGraph(const TSharedPtr<FJsonObject>& Args, bool bForce)
{
	const FString Path = RequireStringArg(Args, TEXT("path"), TEXT("--path is required."));

	const TSharedPtr<FJsonObject>* GraphPtr = nullptr;
	if (!Args->TryGetObjectField(TEXT("graph"), GraphPtr) || !GraphPtr)
	{
		throw FCommandFailedException(TEXT("CLI_USAGE"), TEXT("--graph <json> (or --graph-file <path>) is required."));
	}
	const TSharedPtr<FJsonObject>& Graph = *GraphPtr;

	UMaterial* Material = LoadMaterialOrThrow(Path);

	bool bClear = false;
	Args->TryGetBoolField(TEXT("clear"), bClear);
	Graph->TryGetBoolField(TEXT("clear"), bClear);

	if (Material->GetExpressions().Num() > 0 && !bClear && !bForce)
	{
		throw FCommandFailedException(TEXT("FORCE_REQUIRED"),
			FString::Printf(TEXT("Material '%s' already has %d node(s). Pass --clear to rebuild it from scratch, "
				"or --force to add on top of the existing graph."),
				*Path, Material->GetExpressions().Num()));
	}

	Material->Modify();

	if (bClear)
	{
		UMaterialEditingLibrary::DeleteAllMaterialExpressions(Material);
	}

	// Material-level settings first: some node validation depends on domain/blend mode.
	const TSharedPtr<FJsonObject>* SettingsPtr = nullptr;
	if (Graph->TryGetObjectField(TEXT("settings"), SettingsPtr) && SettingsPtr)
	{
		ApplyMaterialPreset(Material, *SettingsPtr);

		// Anything that isn't one of the friendly aliases is treated as a raw UMaterial property.
		TSharedPtr<FJsonObject> Raw = MakeShared<FJsonObject>();
		for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : (*SettingsPtr)->Values)
		{
			if (Pair.Key.Equals(TEXT("domain"), ESearchCase::IgnoreCase)) continue;
			if (Pair.Key.Equals(TEXT("blend"), ESearchCase::IgnoreCase)) continue;
			if (Pair.Key.Equals(TEXT("shading"), ESearchCase::IgnoreCase)) continue;
			if (Pair.Key.Equals(TEXT("twoSided"), ESearchCase::IgnoreCase)) continue;
			Raw->SetField(Pair.Key, Pair.Value);
		}
		if (Raw->Values.Num() > 0)
		{
			ApplyValues(Material, Raw);
		}
	}

	// Nodes.
	TMap<FString, UMaterialExpression*> NodesById;
	bool bAnyExplicitPos = false;
	const TArray<TSharedPtr<FJsonValue>>* NodeArray = nullptr;
	if (Graph->TryGetArrayField(TEXT("nodes"), NodeArray))
	{
		for (const TSharedPtr<FJsonValue>& NodeValue : *NodeArray)
		{
			const TSharedPtr<FJsonObject> NodeObj = NodeValue->AsObject();
			if (!NodeObj.IsValid()) continue;

			FString Id, Type;
			NodeObj->TryGetStringField(TEXT("id"), Id);
			if (!NodeObj->TryGetStringField(TEXT("type"), Type) || Type.IsEmpty())
			{
				throw FCommandFailedException(TEXT("CLI_USAGE"),
					FString::Printf(TEXT("Graph node '%s' is missing a \"type\"."), *Id));
			}
			if (Id.IsEmpty())
			{
				throw FCommandFailedException(TEXT("CLI_USAGE"),
					FString::Printf(TEXT("Every graph node needs an \"id\" (node of type '%s' has none)."), *Type));
			}
			if (NodesById.Contains(Id))
			{
				throw FCommandFailedException(TEXT("CLI_USAGE"),
					FString::Printf(TEXT("Duplicate node id in graph: '%s'."), *Id));
			}

			int32 PosX = 0, PosY = 0;
			bool bHasPos = false;
			ParsePos(NodeObj, TEXT("pos"), PosX, PosY, bHasPos);
			bAnyExplicitPos |= bHasPos;

			const TSharedPtr<FJsonObject>* ValuesPtr = nullptr;
			NodeObj->TryGetObjectField(TEXT("values"), ValuesPtr);

			NodesById.Add(Id, CreateNode(Material, Type, Id, PosX, PosY, ValuesPtr ? *ValuesPtr : nullptr));
		}
	}

	// Connections between nodes.
	int32 ConnectionCount = 0;
	const TArray<TSharedPtr<FJsonValue>>* ConnectionArray = nullptr;
	if (Graph->TryGetArrayField(TEXT("connections"), ConnectionArray))
	{
		for (const TSharedPtr<FJsonValue>& ConnValue : *ConnectionArray)
		{
			const TSharedPtr<FJsonObject> ConnObj = ConnValue->AsObject();
			if (!ConnObj.IsValid()) continue;

			FString From, FromOutput, To, ToInput, Property;
			ConnObj->TryGetStringField(TEXT("from"), From);
			ConnObj->TryGetStringField(TEXT("fromOutput"), FromOutput);
			ConnObj->TryGetStringField(TEXT("to"), To);
			ConnObj->TryGetStringField(TEXT("toInput"), ToInput);
			ConnObj->TryGetStringField(TEXT("property"), Property);

			ConnectOne(Material, From, FromOutput, To, ToInput, Property);
			++ConnectionCount;
		}
	}

	// Material output pins.
	int32 OutputCount = 0;
	const TArray<TSharedPtr<FJsonValue>>* OutputArray = nullptr;
	if (Graph->TryGetArrayField(TEXT("outputs"), OutputArray))
	{
		for (const TSharedPtr<FJsonValue>& OutValue : *OutputArray)
		{
			const TSharedPtr<FJsonObject> OutObj = OutValue->AsObject();
			if (!OutObj.IsValid()) continue;

			FString From, FromOutput, Property;
			OutObj->TryGetStringField(TEXT("from"), From);
			OutObj->TryGetStringField(TEXT("fromOutput"), FromOutput);
			if (!OutObj->TryGetStringField(TEXT("property"), Property) || Property.IsEmpty())
			{
				throw FCommandFailedException(TEXT("CLI_USAGE"), TEXT("Every graph output needs a \"property\" (e.g. BaseColor)."));
			}

			ConnectOne(Material, From, FromOutput, FString(), FString(), Property);
			++OutputCount;
		}
	}

	// Without hand-placed coordinates the nodes would all pile up at the origin.
	bool bLayout = !bAnyExplicitPos;
	Args->TryGetBoolField(TEXT("layout"), bLayout);
	Graph->TryGetBoolField(TEXT("layout"), bLayout);
	if (bLayout)
	{
		UMaterialEditingLibrary::LayoutMaterialExpressions(Material);
	}

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("path"), NormalizeObjectPath(Path));
	Data->SetNumberField(TEXT("nodesCreated"), NodesById.Num());
	Data->SetNumberField(TEXT("connections"), ConnectionCount);
	Data->SetNumberField(TEXT("outputs"), OutputCount);
	Data->SetBoolField(TEXT("cleared"), bClear);
	Data->SetStringField(TEXT("message"), TEXT("Graph applied."));
	FinishMaterialEdit(Material, Args, Data);
	return Data;
}

// ---------------------------------------------------------------------------
// material compile
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> FMaterialCommandHandler::HandleCompile(const TSharedPtr<FJsonObject>& Args, bool bForce)
{
	const FString Path = RequireStringArg(Args, TEXT("path"), TEXT("--path is required."));
	UMaterial* Material = LoadMaterialOrThrow(Path);

	bool bLayout = false;
	Args->TryGetBoolField(TEXT("layout"), bLayout);
	if (bLayout)
	{
		Material->Modify();
		UMaterialEditingLibrary::LayoutMaterialExpressions(Material);
	}

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("path"), NormalizeObjectPath(Path));
	Data->SetNumberField(TEXT("nodeCount"), UMaterialEditingLibrary::GetNumMaterialExpressions(Material));
	Data->SetStringField(TEXT("message"), TEXT("Material recompiled."));
	FinishMaterialEdit(Material, Args, Data);
	return Data;
}

// ---------------------------------------------------------------------------
// material create-instance / set-instance-param
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> FMaterialCommandHandler::HandleCreateInstance(const TSharedPtr<FJsonObject>& Args, bool bForce)
{
	const FString Path   = RequireStringArg(Args, TEXT("path"),   TEXT("--path is required."));
	const FString Parent = RequireStringArg(Args, TEXT("parent"), TEXT("--parent is required."));

	UMaterialInterface* ParentMaterial = LoadObject<UMaterialInterface>(nullptr, *NormalizeObjectPath(Parent));
	if (!ParentMaterial)
	{
		throw FCommandFailedException(TEXT("NOT_FOUND"),
			FString::Printf(TEXT("Parent material not found: %s"), *Parent));
	}

	const FString ObjectPath = NormalizeObjectPath(Path);
	const FAssetData Existing = GetMaterialAssetRegistry().GetAssetByObjectPath(FSoftObjectPath(ObjectPath));
	if (Existing.IsValid() && !bForce)
	{
		throw FCommandFailedException(TEXT("FORCE_REQUIRED"),
			FString::Printf(TEXT("Asset already exists: %s. Use --force to overwrite."), *Path));
	}

	const FString PackageName = FPackageName::ObjectPathToPackageName(ObjectPath);
	const FString AssetName   = FPackageName::GetLongPackageAssetName(PackageName);
	const FString PackagePath = FPackageName::GetLongPackagePath(PackageName);

	UMaterialInstanceConstantFactoryNew* Factory = NewObject<UMaterialInstanceConstantFactoryNew>();
	Factory->InitialParent = ParentMaterial;

	UMaterialInstanceConstant* Instance = Cast<UMaterialInstanceConstant>(
		GetMaterialAssetTools().CreateAsset(AssetName, PackagePath, UMaterialInstanceConstant::StaticClass(), Factory));
	if (!Instance)
	{
		throw FCommandFailedException(TEXT("OPERATION_FAILED"), TEXT("Failed to create material instance."));
	}

	bool bSave = false;
	Args->TryGetBoolField(TEXT("save"), bSave);

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("path"), ObjectPath);
	Data->SetStringField(TEXT("parent"), NormalizeObjectPath(Parent));
	Data->SetStringField(TEXT("message"), TEXT("Material instance created."));
	if (bSave)
	{
		Data->SetBoolField(TEXT("saved"), SaveAssetPackage(Instance));
	}
	return Data;
}

TSharedPtr<FJsonObject> FMaterialCommandHandler::HandleSetInstanceParam(const TSharedPtr<FJsonObject>& Args, bool bForce)
{
	const FString Path      = RequireStringArg(Args, TEXT("path"), TEXT("--path is required."));
	const FString ParamName = RequireStringArg(Args, TEXT("name"), TEXT("--name is required."));
	const FString Type      = RequireStringArg(Args, TEXT("type"), TEXT("--type is required (scalar, vector, texture, switch)."));

	UMaterialInstanceConstant* Instance = LoadObject<UMaterialInstanceConstant>(nullptr, *NormalizeObjectPath(Path));
	if (!Instance)
	{
		throw FCommandFailedException(TEXT("NOT_FOUND"),
			FString::Printf(TEXT("Material instance not found: %s"), *Path));
	}

	const TSharedPtr<FJsonValue> Value = Args->TryGetField(TEXT("value"));
	if (!Value.IsValid())
	{
		throw FCommandFailedException(TEXT("CLI_USAGE"), TEXT("--value is required."));
	}

	const FName Param(*ParamName);
	const FString LowerType = Type.ToLower();

	// The engine's SetMaterialInstance*ParameterValue helpers always return false (they never assign
	// their result), so the parameter has to be validated against the parent up front instead.
	TArray<FName> KnownNames;
	if      (LowerType == TEXT("scalar"))                                  UMaterialEditingLibrary::GetScalarParameterNames(Instance, KnownNames);
	else if (LowerType == TEXT("vector") || LowerType == TEXT("color"))    UMaterialEditingLibrary::GetVectorParameterNames(Instance, KnownNames);
	else if (LowerType == TEXT("texture"))                                 UMaterialEditingLibrary::GetTextureParameterNames(Instance, KnownNames);
	else if (LowerType == TEXT("switch") || LowerType == TEXT("bool"))     UMaterialEditingLibrary::GetStaticSwitchParameterNames(Instance, KnownNames);
	else
	{
		throw FCommandFailedException(TEXT("CLI_USAGE"),
			FString::Printf(TEXT("Unknown parameter type '%s'. Supported: scalar, vector, texture, switch"), *Type));
	}

	if (!KnownNames.Contains(Param))
	{
		TArray<FString> NameStrings;
		for (const FName& Known : KnownNames) NameStrings.Add(Known.ToString());
		throw FCommandFailedException(TEXT("NOT_FOUND"),
			FString::Printf(TEXT("The parent material exposes no %s parameter named '%s'. Available: %s"),
				*LowerType, *ParamName,
				NameStrings.Num() > 0 ? *FString::Join(NameStrings, TEXT(", ")) : TEXT("(none)")));
	}

	if (LowerType == TEXT("scalar"))
	{
		UMaterialEditingLibrary::SetMaterialInstanceScalarParameterValue(
			Instance, Param, static_cast<float>(Value->AsNumber()));
	}
	else if (LowerType == TEXT("vector") || LowerType == TEXT("color"))
	{
		FLinearColor Color = FLinearColor::Black;
		if (Value->Type == EJson::Array)
		{
			const TArray<TSharedPtr<FJsonValue>>& Items = Value->AsArray();
			if (Items.Num() < 3)
			{
				throw FCommandFailedException(TEXT("CLI_USAGE"), TEXT("A vector parameter needs at least [r,g,b]."));
			}
			Color = FLinearColor(
				static_cast<float>(Items[0]->AsNumber()),
				static_cast<float>(Items[1]->AsNumber()),
				static_cast<float>(Items[2]->AsNumber()),
				Items.Num() > 3 ? static_cast<float>(Items[3]->AsNumber()) : 1.0f);
		}
		else if (!Color.InitFromString(Value->AsString()))
		{
			throw FCommandFailedException(TEXT("CLI_USAGE"),
				FString::Printf(TEXT("Could not read a color from '%s'. Use [r,g,b,a] or \"(R=..,G=..,B=..,A=..)\"."),
					*Value->AsString()));
		}
		UMaterialEditingLibrary::SetMaterialInstanceVectorParameterValue(Instance, Param, Color);
	}
	else if (LowerType == TEXT("texture"))
	{
		UTexture* Texture = LoadObject<UTexture>(nullptr, *NormalizeObjectPath(Value->AsString()));
		if (!Texture)
		{
			throw FCommandFailedException(TEXT("NOT_FOUND"),
				FString::Printf(TEXT("Texture not found: %s"), *Value->AsString()));
		}
		UMaterialEditingLibrary::SetMaterialInstanceTextureParameterValue(Instance, Param, Texture);
	}
	else // switch / bool
	{
		UMaterialEditingLibrary::SetMaterialInstanceStaticSwitchParameterValue(Instance, Param, Value->AsBool());
	}

	UMaterialEditingLibrary::UpdateMaterialInstance(Instance);
	Instance->MarkPackageDirty();

	bool bSave = false;
	Args->TryGetBoolField(TEXT("save"), bSave);

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("path"), NormalizeObjectPath(Path));
	Data->SetStringField(TEXT("name"), ParamName);
	Data->SetStringField(TEXT("type"), LowerType);
	Data->SetStringField(TEXT("message"), TEXT("Instance parameter set."));
	if (bSave)
	{
		Data->SetBoolField(TEXT("saved"), SaveAssetPackage(Instance));
	}
	return Data;
}
