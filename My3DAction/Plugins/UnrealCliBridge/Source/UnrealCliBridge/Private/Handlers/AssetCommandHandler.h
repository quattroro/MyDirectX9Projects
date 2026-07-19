#pragma once
#include "CoreMinimal.h"
#include "CommandDispatcher.h"

class FAssetCommandHandler
{
public:
	static void RegisterAll(FCommandDispatcher& Dispatcher);

private:
	static TSharedPtr<FJsonObject> HandleFind(const TSharedPtr<FJsonObject>& Args, bool bForce);
	static TSharedPtr<FJsonObject> HandleInfo(const TSharedPtr<FJsonObject>& Args, bool bForce);
	static TSharedPtr<FJsonObject> HandleMove(const TSharedPtr<FJsonObject>& Args, bool bForce);
	static TSharedPtr<FJsonObject> HandleRename(const TSharedPtr<FJsonObject>& Args, bool bForce);
	static TSharedPtr<FJsonObject> HandleDelete(const TSharedPtr<FJsonObject>& Args, bool bForce);
	static TSharedPtr<FJsonObject> HandleCreate(const TSharedPtr<FJsonObject>& Args, bool bForce);
	static TSharedPtr<FJsonObject> HandleMkdir(const TSharedPtr<FJsonObject>& Args, bool bForce);
};
