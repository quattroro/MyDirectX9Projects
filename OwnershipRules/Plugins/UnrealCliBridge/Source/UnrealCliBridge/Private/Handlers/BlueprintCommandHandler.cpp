#include "BlueprintCommandHandler.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "EdGraph/EdGraph.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "UObject/UnrealType.h"

DEFINE_LOG_CATEGORY_STATIC(LogUnrealCliBridgeBlueprint, Log, All);

void FBlueprintCommandHandler::RegisterAll(FCommandDispatcher& Dispatcher)
{
	Dispatcher.Register(TEXT("blueprint.inspect"), &HandleInspect);
	Dispatcher.Register(TEXT("blueprint.set-property"), &HandleSetProperty);
}

TSharedPtr<FJsonObject> FBlueprintCommandHandler::HandleInspect(const TSharedPtr<FJsonObject>& Args, bool bForce)
{
	FString Path;
	bool bWithValues = false;
	if (!Args.IsValid() || !Args->TryGetStringField(TEXT("path"), Path) || Path.IsEmpty())
		throw FCommandFailedException(TEXT("CLI_USAGE"), TEXT("--path is required."));
	Args->TryGetBoolField(TEXT("withValues"), bWithValues);

	UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *Path);
	if (!Blueprint)
		throw FCommandFailedException(TEXT("NOT_FOUND"), FString::Printf(TEXT("Blueprint not found: %s"), *Path));

	UClass* GeneratedClass = Blueprint->GeneratedClass;
	if (!GeneratedClass)
		throw FCommandFailedException(TEXT("OPERATION_FAILED"), TEXT("Blueprint has no generated class."));

	UObject* CDO = GeneratedClass->GetDefaultObject();

	TArray<TSharedPtr<FJsonValue>> Properties;
	for (TFieldIterator<FProperty> PropIt(GeneratedClass, EFieldIteratorFlags::ExcludeSuper); PropIt; ++PropIt)
	{
		FProperty* Property = *PropIt;
		if (!Property->HasAnyPropertyFlags(CPF_Edit | CPF_BlueprintVisible))
			continue;

		TSharedPtr<FJsonObject> PropObj = MakeShared<FJsonObject>();
		PropObj->SetStringField(TEXT("name"), Property->GetName());
		PropObj->SetStringField(TEXT("type"), Property->GetCPPType());

		if (bWithValues && CDO)
		{
			void* PropPtr = Property->ContainerPtrToValuePtr<void>(CDO);
			FString ValueStr;
			Property->ExportTextItem_Direct(ValueStr, PropPtr, nullptr, nullptr, PPF_None);
			PropObj->SetStringField(TEXT("value"), ValueStr);
		}

		Properties.Add(MakeShared<FJsonValueObject>(PropObj));
	}

	// Components
	TArray<TSharedPtr<FJsonValue>> Components;
	TArray<UActorComponent*> CompTemplates = Blueprint->ComponentTemplates;
	for (UActorComponent* Comp : CompTemplates)
	{
		if (!Comp) continue;
		TSharedPtr<FJsonObject> CompObj = MakeShared<FJsonObject>();
		CompObj->SetStringField(TEXT("name"), Comp->GetName());
		CompObj->SetStringField(TEXT("type"), Comp->GetClass()->GetName());
		Components.Add(MakeShared<FJsonValueObject>(CompObj));
	}

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("path"), Path);
	Data->SetStringField(TEXT("parentClass"), Blueprint->ParentClass ? Blueprint->ParentClass->GetName() : TEXT("None"));
	Data->SetArrayField(TEXT("properties"), Properties);
	Data->SetArrayField(TEXT("components"), Components);
	return Data;
}

TSharedPtr<FJsonObject> FBlueprintCommandHandler::HandleSetProperty(const TSharedPtr<FJsonObject>& Args, bool bForce)
{
	FString Path, PropertyName, Value;
	if (!Args.IsValid()
		|| !Args->TryGetStringField(TEXT("path"), Path)
		|| !Args->TryGetStringField(TEXT("property"), PropertyName)
		|| !Args->TryGetStringField(TEXT("value"), Value))
		throw FCommandFailedException(TEXT("CLI_USAGE"), TEXT("--path, --property, and --value are required."));

	UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *Path);
	if (!Blueprint)
		throw FCommandFailedException(TEXT("NOT_FOUND"), FString::Printf(TEXT("Blueprint not found: %s"), *Path));

	UClass* GeneratedClass = Blueprint->GeneratedClass;
	if (!GeneratedClass)
		throw FCommandFailedException(TEXT("OPERATION_FAILED"), TEXT("Blueprint has no generated class."));

	UObject* CDO = GeneratedClass->GetDefaultObject();
	FProperty* Property = FindFProperty<FProperty>(GeneratedClass, *PropertyName);
	if (!Property)
		throw FCommandFailedException(TEXT("NOT_FOUND"), FString::Printf(TEXT("Property not found: %s"), *PropertyName));

	void* PropPtr = Property->ContainerPtrToValuePtr<void>(CDO);
	Property->ImportText_Direct(*Value, PropPtr, CDO, PPF_None);

	// Mark Blueprint dirty and compile
	Blueprint->MarkPackageDirty();
	FKismetEditorUtilities::CompileBlueprint(Blueprint);

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("path"), Path);
	Data->SetStringField(TEXT("property"), PropertyName);
	Data->SetStringField(TEXT("value"), Value);
	Data->SetStringField(TEXT("message"), TEXT("Property set and Blueprint compiled."));
	return Data;
}
