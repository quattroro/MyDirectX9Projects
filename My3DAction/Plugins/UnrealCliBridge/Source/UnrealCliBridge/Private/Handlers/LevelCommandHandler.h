#pragma once
#include "CoreMinimal.h"
#include "CommandDispatcher.h"

class FLevelCommandHandler
{
public:
	static void RegisterAll(FCommandDispatcher& Dispatcher);

private:
	static TSharedPtr<FJsonObject> HandleOpen(const TSharedPtr<FJsonObject>& Args, bool bForce);
	static TSharedPtr<FJsonObject> HandleInspect(const TSharedPtr<FJsonObject>& Args, bool bForce);
	static TSharedPtr<FJsonObject> HandleAddActor(const TSharedPtr<FJsonObject>& Args, bool bForce);
	static TSharedPtr<FJsonObject> HandleSetTransform(const TSharedPtr<FJsonObject>& Args, bool bForce);
	static TSharedPtr<FJsonObject> HandleDeleteActor(const TSharedPtr<FJsonObject>& Args, bool bForce);
	static TSharedPtr<FJsonObject> HandleListComponents(const TSharedPtr<FJsonObject>& Args, bool bForce);
	static TSharedPtr<FJsonObject> HandleAddComponent(const TSharedPtr<FJsonObject>& Args, bool bForce);
	static TSharedPtr<FJsonObject> HandleRemoveComponent(const TSharedPtr<FJsonObject>& Args, bool bForce);
	static TSharedPtr<FJsonObject> HandleAssignMaterial(const TSharedPtr<FJsonObject>& Args, bool bForce);

	static AActor* FindActorByLabel(UWorld* World, const FString& Label);
	static FVector ParseVector3(const FString& Str);
	static FRotator ParseRotator(const FString& Str);
	static TSharedPtr<FJsonObject> ActorToJson(AActor* Actor, bool bWithValues);
};
