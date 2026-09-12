#include "HandlerUtils.h"

#include "CommandDispatcher.h"

#include "Misc/PackageName.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "UObject/UnrealType.h"

namespace HandlerUtils
{

FString NormalizeObjectPath(const FString& InPath)
{
	FString Path = InPath;
	Path.TrimStartAndEndInline();
	if (Path.IsEmpty())
	{
		return Path;
	}

	int32 LastSlash = INDEX_NONE;
	Path.FindLastChar(TEXT('/'), LastSlash);
	const FString LastSegment = (LastSlash == INDEX_NONE) ? Path : Path.Mid(LastSlash + 1);
	if (LastSegment.Contains(TEXT(".")))
	{
		return Path;
	}
	return FString::Printf(TEXT("%s.%s"), *Path, *LastSegment);
}

FString RequireStringArg(const TSharedPtr<FJsonObject>& Args, const TCHAR* Field, const TCHAR* Usage)
{
	FString Value;
	if (!Args.IsValid() || !Args->TryGetStringField(Field, Value) || Value.IsEmpty())
	{
		throw FCommandFailedException(TEXT("CLI_USAGE"), Usage);
	}
	return Value;
}

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

FString JsonToPropertyText(const FProperty* Property, const TSharedPtr<FJsonValue>& Value)
{
	if (!Value.IsValid())
	{
		return FString();
	}

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

bool SaveAssetPackage(UObject* Asset)
{
	if (!Asset)
	{
		return false;
	}

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

static void ApplyValuesInternal(UObject* Target, const TSharedPtr<FJsonObject>& Values, FPropertyOverrideFn* Override)
{
	if (!Target || !Values.IsValid())
	{
		return;
	}

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

		if (Override && (*Override)(*Property, ValuePtr, Pair.Value))
		{
			continue;
		}

		// Object references (textures, behavior trees, ...) are given as content paths.
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

	Target->PostEditChange();
}

void ApplyValues(UObject* Target, const TSharedPtr<FJsonObject>& Values)
{
	ApplyValuesInternal(Target, Values, nullptr);
}

void ApplyValues(UObject* Target, const TSharedPtr<FJsonObject>& Values, FPropertyOverrideFn Override)
{
	ApplyValuesInternal(Target, Values, &Override);
}

} // namespace HandlerUtils
