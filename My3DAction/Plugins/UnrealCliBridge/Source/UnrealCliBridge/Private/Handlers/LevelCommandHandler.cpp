#include "LevelCommandHandler.h"
#include "Editor.h"
#include "EditorLevelUtils.h"
#include "FileHelpers.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "Engine/StaticMeshActor.h"
#include "GameFramework/Actor.h"
#include "Components/StaticMeshComponent.h"
#include "Components/MeshComponent.h"
#include "Materials/MaterialInterface.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Subsystems/EditorActorSubsystem.h"
#include "LevelEditorSubsystem.h"
#include "Misc/PackageName.h"

DEFINE_LOG_CATEGORY_STATIC(LogUnrealCliBridgeLevel, Log, All);

void FLevelCommandHandler::RegisterAll(FCommandDispatcher& Dispatcher)
{
	Dispatcher.Register(TEXT("level.open"), &HandleOpen);
	Dispatcher.Register(TEXT("level.inspect"), &HandleInspect);
	Dispatcher.Register(TEXT("level.add-actor"), &HandleAddActor);
	Dispatcher.Register(TEXT("level.set-transform"), &HandleSetTransform);
	Dispatcher.Register(TEXT("level.delete-actor"), &HandleDeleteActor);
	Dispatcher.Register(TEXT("level.list-components"), &HandleListComponents);
	Dispatcher.Register(TEXT("level.add-component"), &HandleAddComponent);
	Dispatcher.Register(TEXT("level.remove-component"), &HandleRemoveComponent);
	Dispatcher.Register(TEXT("level.assign-material"), &HandleAssignMaterial);
}

TSharedPtr<FJsonObject> FLevelCommandHandler::HandleOpen(const TSharedPtr<FJsonObject>& Args, bool bForce)
{
	FString Path;
	if (!Args.IsValid() || !Args->TryGetStringField(TEXT("path"), Path) || Path.IsEmpty())
		throw FCommandFailedException(TEXT("CLI_USAGE"), TEXT("--path is required."));

	ULevelEditorSubsystem* LevelSubsystem = GEditor->GetEditorSubsystem<ULevelEditorSubsystem>();
	if (!LevelSubsystem)
		throw FCommandFailedException(TEXT("INTERNAL_ERROR"), TEXT("LevelEditorSubsystem not available."));

	bool bSuccess = LevelSubsystem->LoadLevel(Path);
	if (!bSuccess)
		throw FCommandFailedException(TEXT("NOT_FOUND"), FString::Printf(TEXT("Failed to open level: %s"), *Path));

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("path"), Path);
	Data->SetStringField(TEXT("message"), TEXT("Level opened."));
	return Data;
}

TSharedPtr<FJsonObject> FLevelCommandHandler::HandleInspect(const TSharedPtr<FJsonObject>& Args, bool bForce)
{
	bool bWithValues = false;
	int32 MaxDepth = 10;
	if (Args.IsValid())
	{
		Args->TryGetBoolField(TEXT("withValues"), bWithValues);
		Args->TryGetNumberField(TEXT("maxDepth"), MaxDepth);
	}

	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World)
		throw FCommandFailedException(TEXT("INTERNAL_ERROR"), TEXT("No world loaded."));

	TArray<TSharedPtr<FJsonValue>> Actors;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (!Actor || Actor->IsEditorOnly() == false && !Actor->HasAnyFlags(RF_Transactional))
			continue;
		Actors.Add(MakeShared<FJsonValueObject>(ActorToJson(Actor, bWithValues)));
		if (Actors.Num() >= 500) break;
	}

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("levelName"), World->GetMapName());
	Data->SetArrayField(TEXT("actors"), Actors);
	Data->SetNumberField(TEXT("count"), Actors.Num());
	return Data;
}

TSharedPtr<FJsonObject> FLevelCommandHandler::HandleAddActor(const TSharedPtr<FJsonObject>& Args, bool bForce)
{
	FString ClassName;
	if (!Args.IsValid() || !Args->TryGetStringField(TEXT("class"), ClassName) || ClassName.IsEmpty())
		throw FCommandFailedException(TEXT("CLI_USAGE"), TEXT("--class is required."));

	FString Label;
	FString LocationStr, RotationStr, ScaleStr;
	if (Args.IsValid())
	{
		Args->TryGetStringField(TEXT("name"), Label);
		Args->TryGetStringField(TEXT("location"), LocationStr);
		Args->TryGetStringField(TEXT("rotation"), RotationStr);
		Args->TryGetStringField(TEXT("scale"), ScaleStr);
	}

	UEditorActorSubsystem* ActorSubsystem = GEditor->GetEditorSubsystem<UEditorActorSubsystem>();
	if (!ActorSubsystem)
		throw FCommandFailedException(TEXT("INTERNAL_ERROR"), TEXT("EditorActorSubsystem not available."));

	// Find class by name
	UClass* ActorClass = nullptr;
	// Try common names first
	TMap<FString, UClass*> CommonClasses = {
		{ TEXT("StaticMeshActor"), AStaticMeshActor::StaticClass() },
		{ TEXT("Actor"), AActor::StaticClass() },
	};

	if (CommonClasses.Contains(ClassName))
	{
		ActorClass = CommonClasses[ClassName];
	}
	else
	{
		// Try to find via reflection (ANY_PACKAGE removed in UE 5.3)
		ActorClass = FindFirstObjectSafe<UClass>(*ClassName);
		if (!ActorClass)
		{
			ActorClass = FindFirstObjectSafe<UClass>(*(TEXT("A") + ClassName));
		}
	}

	if (!ActorClass || !ActorClass->IsChildOf(AActor::StaticClass()))
		throw FCommandFailedException(TEXT("NOT_FOUND"), FString::Printf(TEXT("Actor class not found: %s"), *ClassName));

	FVector Location = LocationStr.IsEmpty() ? FVector::ZeroVector : ParseVector3(LocationStr);
	FRotator Rotation = RotationStr.IsEmpty() ? FRotator::ZeroRotator : ParseRotator(RotationStr);
	FVector Scale = ScaleStr.IsEmpty() ? FVector::OneVector : ParseVector3(ScaleStr);

	FTransform SpawnTransform(Rotation, Location, Scale);
	AActor* NewActor = ActorSubsystem->SpawnActorFromClass(ActorClass, Location, Rotation);

	if (!NewActor)
		throw FCommandFailedException(TEXT("OPERATION_FAILED"), TEXT("Failed to spawn actor."));

	NewActor->SetActorScale3D(Scale);

	if (!Label.IsEmpty())
	{
		NewActor->SetActorLabel(Label);
	}

	// Mark level dirty for save
	ULevel* Level = NewActor->GetLevel();
	if (Level) Level->MarkPackageDirty();

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("actorLabel"), NewActor->GetActorLabel());
	Data->SetStringField(TEXT("actorClass"), NewActor->GetClass()->GetName());
	Data->SetStringField(TEXT("message"), TEXT("Actor spawned."));
	return Data;
}

TSharedPtr<FJsonObject> FLevelCommandHandler::HandleSetTransform(const TSharedPtr<FJsonObject>& Args, bool bForce)
{
	FString ActorLabel;
	if (!Args.IsValid() || !Args->TryGetStringField(TEXT("actor"), ActorLabel) || ActorLabel.IsEmpty())
		throw FCommandFailedException(TEXT("CLI_USAGE"), TEXT("--actor is required."));

	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World) throw FCommandFailedException(TEXT("INTERNAL_ERROR"), TEXT("No world loaded."));

	AActor* Actor = FindActorByLabel(World, ActorLabel);
	if (!Actor) throw FCommandFailedException(TEXT("NOT_FOUND"), FString::Printf(TEXT("Actor not found: %s"), *ActorLabel));

	FString LocationStr, RotationStr, ScaleStr;
	Args->TryGetStringField(TEXT("location"), LocationStr);
	Args->TryGetStringField(TEXT("rotation"), RotationStr);
	Args->TryGetStringField(TEXT("scale"), ScaleStr);

	if (!LocationStr.IsEmpty()) Actor->SetActorLocation(ParseVector3(LocationStr));
	if (!RotationStr.IsEmpty()) Actor->SetActorRotation(ParseRotator(RotationStr));
	if (!ScaleStr.IsEmpty()) Actor->SetActorScale3D(ParseVector3(ScaleStr));

	if (ULevel* Level = Actor->GetLevel()) Level->MarkPackageDirty();

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("actor"), ActorLabel);

	TSharedPtr<FJsonObject> Transform = MakeShared<FJsonObject>();
	FVector Loc = Actor->GetActorLocation();
	FRotator Rot = Actor->GetActorRotation();
	FVector Scale = Actor->GetActorScale3D();
	Transform->SetStringField(TEXT("location"), FString::Printf(TEXT("%.3f,%.3f,%.3f"), Loc.X, Loc.Y, Loc.Z));
	Transform->SetStringField(TEXT("rotation"), FString::Printf(TEXT("%.3f,%.3f,%.3f"), Rot.Pitch, Rot.Yaw, Rot.Roll));
	Transform->SetStringField(TEXT("scale"), FString::Printf(TEXT("%.3f,%.3f,%.3f"), Scale.X, Scale.Y, Scale.Z));
	Data->SetObjectField(TEXT("transform"), Transform);
	return Data;
}

TSharedPtr<FJsonObject> FLevelCommandHandler::HandleDeleteActor(const TSharedPtr<FJsonObject>& Args, bool bForce)
{
	if (!bForce) throw FCommandFailedException(TEXT("FORCE_REQUIRED"), TEXT("level delete-actor always requires --force."));

	FString ActorLabel;
	if (!Args.IsValid() || !Args->TryGetStringField(TEXT("actor"), ActorLabel))
		throw FCommandFailedException(TEXT("CLI_USAGE"), TEXT("--actor is required."));

	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World) throw FCommandFailedException(TEXT("INTERNAL_ERROR"), TEXT("No world loaded."));

	AActor* Actor = FindActorByLabel(World, ActorLabel);
	if (!Actor) throw FCommandFailedException(TEXT("NOT_FOUND"), FString::Printf(TEXT("Actor not found: %s"), *ActorLabel));

	UEditorActorSubsystem* ActorSubsystem = GEditor->GetEditorSubsystem<UEditorActorSubsystem>();
	bool bSuccess = ActorSubsystem ? ActorSubsystem->DestroyActor(Actor) : World->DestroyActor(Actor);

	if (!bSuccess) throw FCommandFailedException(TEXT("OPERATION_FAILED"), TEXT("Failed to delete actor."));

	if (ULevel* Level = World->GetCurrentLevel()) Level->MarkPackageDirty();

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("actor"), ActorLabel);
	Data->SetStringField(TEXT("message"), TEXT("Actor deleted."));
	return Data;
}

TSharedPtr<FJsonObject> FLevelCommandHandler::HandleListComponents(const TSharedPtr<FJsonObject>& Args, bool bForce)
{
	FString ActorLabel;
	if (!Args.IsValid() || !Args->TryGetStringField(TEXT("actor"), ActorLabel))
		throw FCommandFailedException(TEXT("CLI_USAGE"), TEXT("--actor is required."));

	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World) throw FCommandFailedException(TEXT("INTERNAL_ERROR"), TEXT("No world loaded."));

	AActor* Actor = FindActorByLabel(World, ActorLabel);
	if (!Actor) throw FCommandFailedException(TEXT("NOT_FOUND"), FString::Printf(TEXT("Actor not found: %s"), *ActorLabel));

	TArray<UActorComponent*> Components;
	Actor->GetComponents(Components);

	TArray<TSharedPtr<FJsonValue>> CompArray;
	for (int32 i = 0; i < Components.Num(); ++i)
	{
		TSharedPtr<FJsonObject> CompObj = MakeShared<FJsonObject>();
		CompObj->SetNumberField(TEXT("index"), i);
		CompObj->SetStringField(TEXT("type"), Components[i]->GetClass()->GetName());
		CompObj->SetStringField(TEXT("name"), Components[i]->GetName());
		CompArray.Add(MakeShared<FJsonValueObject>(CompObj));
	}

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("actor"), ActorLabel);
	Data->SetArrayField(TEXT("components"), CompArray);
	return Data;
}

TSharedPtr<FJsonObject> FLevelCommandHandler::HandleAddComponent(const TSharedPtr<FJsonObject>& Args, bool bForce)
{
	FString ActorLabel, ComponentType;
	if (!Args.IsValid() || !Args->TryGetStringField(TEXT("actor"), ActorLabel) || !Args->TryGetStringField(TEXT("type"), ComponentType))
		throw FCommandFailedException(TEXT("CLI_USAGE"), TEXT("--actor and --type are required."));

	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	AActor* Actor = FindActorByLabel(World, ActorLabel);
	if (!Actor) throw FCommandFailedException(TEXT("NOT_FOUND"), FString::Printf(TEXT("Actor not found: %s"), *ActorLabel));

	UClass* CompClass = FindFirstObjectSafe<UClass>(*ComponentType);
	if (!CompClass) CompClass = FindFirstObjectSafe<UClass>(*(TEXT("U") + ComponentType));
	if (!CompClass || !CompClass->IsChildOf(UActorComponent::StaticClass()))
		throw FCommandFailedException(TEXT("NOT_FOUND"), FString::Printf(TEXT("Component class not found: %s"), *ComponentType));

	UActorComponent* NewComp = NewObject<UActorComponent>(Actor, CompClass, NAME_None, RF_Transactional);
	Actor->AddInstanceComponent(NewComp);
	NewComp->RegisterComponent();

	if (ULevel* Level = Actor->GetLevel()) Level->MarkPackageDirty();

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("actor"), ActorLabel);
	Data->SetStringField(TEXT("componentType"), ComponentType);
	Data->SetStringField(TEXT("componentName"), NewComp->GetName());
	Data->SetStringField(TEXT("message"), TEXT("Component added."));
	return Data;
}

TSharedPtr<FJsonObject> FLevelCommandHandler::HandleRemoveComponent(const TSharedPtr<FJsonObject>& Args, bool bForce)
{
	if (!bForce) throw FCommandFailedException(TEXT("FORCE_REQUIRED"), TEXT("level remove-component always requires --force."));

	FString ActorLabel, ComponentType;
	int32 Index = 0;
	if (!Args.IsValid() || !Args->TryGetStringField(TEXT("actor"), ActorLabel) || !Args->TryGetStringField(TEXT("type"), ComponentType))
		throw FCommandFailedException(TEXT("CLI_USAGE"), TEXT("--actor and --type are required."));
	Args->TryGetNumberField(TEXT("index"), Index);

	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	AActor* Actor = FindActorByLabel(World, ActorLabel);
	if (!Actor) throw FCommandFailedException(TEXT("NOT_FOUND"), FString::Printf(TEXT("Actor not found: %s"), *ActorLabel));

	TArray<UActorComponent*> Components;
	Actor->GetComponents(Components);

	int32 MatchIndex = 0;
	for (UActorComponent* Comp : Components)
	{
		if (Comp->GetClass()->GetName() == ComponentType || Comp->GetClass()->GetName() == TEXT("U") + ComponentType)
		{
			if (MatchIndex == Index)
			{
				Comp->DestroyComponent();
				if (ULevel* Level = Actor->GetLevel()) Level->MarkPackageDirty();

				TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
				Data->SetStringField(TEXT("actor"), ActorLabel);
				Data->SetStringField(TEXT("componentType"), ComponentType);
				Data->SetStringField(TEXT("message"), TEXT("Component removed."));
				return Data;
			}
			MatchIndex++;
		}
	}

	throw FCommandFailedException(TEXT("NOT_FOUND"),
		FString::Printf(TEXT("Component '%s'[%d] not found on actor '%s'."), *ComponentType, Index, *ActorLabel));
}

TSharedPtr<FJsonObject> FLevelCommandHandler::HandleAssignMaterial(const TSharedPtr<FJsonObject>& Args, bool bForce)
{
	FString ActorLabel, MaterialPath;
	int32 Slot = 0;
	if (!Args.IsValid() || !Args->TryGetStringField(TEXT("actor"), ActorLabel) || !Args->TryGetStringField(TEXT("material"), MaterialPath))
		throw FCommandFailedException(TEXT("CLI_USAGE"), TEXT("--actor and --material are required."));
	Args->TryGetNumberField(TEXT("slot"), Slot);

	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	AActor* Actor = FindActorByLabel(World, ActorLabel);
	if (!Actor) throw FCommandFailedException(TEXT("NOT_FOUND"), FString::Printf(TEXT("Actor not found: %s"), *ActorLabel));

	UMaterialInterface* Material = Cast<UMaterialInterface>(
		StaticLoadObject(UMaterialInterface::StaticClass(), nullptr, *MaterialPath));
	if (!Material)
		throw FCommandFailedException(TEXT("NOT_FOUND"), FString::Printf(TEXT("Material not found: %s"), *MaterialPath));

	UMeshComponent* MeshComp = Actor->FindComponentByClass<UMeshComponent>();
	if (!MeshComp)
		throw FCommandFailedException(TEXT("NOT_FOUND"), TEXT("Actor has no MeshComponent."));

	MeshComp->SetMaterial(Slot, Material);
	if (ULevel* Level = Actor->GetLevel()) Level->MarkPackageDirty();

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("actor"), ActorLabel);
	Data->SetStringField(TEXT("material"), MaterialPath);
	Data->SetNumberField(TEXT("slot"), Slot);
	return Data;
}

// --- Helpers ---

AActor* FLevelCommandHandler::FindActorByLabel(UWorld* World, const FString& Label)
{
	if (!World) return nullptr;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		if ((*It)->GetActorLabel() == Label)
			return *It;
	}
	return nullptr;
}

FVector FLevelCommandHandler::ParseVector3(const FString& Str)
{
	TArray<FString> Parts;
	Str.ParseIntoArray(Parts, TEXT(","));
	if (Parts.Num() < 3)
		throw FCommandFailedException(TEXT("CLI_USAGE"), FString::Printf(TEXT("Invalid vector format: '%s'. Expected x,y,z"), *Str));
	return FVector(FCString::Atof(*Parts[0]), FCString::Atof(*Parts[1]), FCString::Atof(*Parts[2]));
}

FRotator FLevelCommandHandler::ParseRotator(const FString& Str)
{
	TArray<FString> Parts;
	Str.ParseIntoArray(Parts, TEXT(","));
	if (Parts.Num() < 3)
		throw FCommandFailedException(TEXT("CLI_USAGE"), FString::Printf(TEXT("Invalid rotator format: '%s'. Expected pitch,yaw,roll"), *Str));
	return FRotator(FCString::Atof(*Parts[0]), FCString::Atof(*Parts[1]), FCString::Atof(*Parts[2]));
}

TSharedPtr<FJsonObject> FLevelCommandHandler::ActorToJson(AActor* Actor, bool bWithValues)
{
	TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
	Obj->SetStringField(TEXT("label"), Actor->GetActorLabel());
	Obj->SetStringField(TEXT("class"), Actor->GetClass()->GetName());

	if (bWithValues)
	{
		FVector Loc = Actor->GetActorLocation();
		FRotator Rot = Actor->GetActorRotation();
		FVector Scale = Actor->GetActorScale3D();
		TSharedPtr<FJsonObject> Transform = MakeShared<FJsonObject>();
		Transform->SetStringField(TEXT("location"), FString::Printf(TEXT("%.3f,%.3f,%.3f"), Loc.X, Loc.Y, Loc.Z));
		Transform->SetStringField(TEXT("rotation"), FString::Printf(TEXT("%.3f,%.3f,%.3f"), Rot.Pitch, Rot.Yaw, Rot.Roll));
		Transform->SetStringField(TEXT("scale"), FString::Printf(TEXT("%.3f,%.3f,%.3f"), Scale.X, Scale.Y, Scale.Z));
		Obj->SetObjectField(TEXT("transform"), Transform);
	}

	return Obj;
}
