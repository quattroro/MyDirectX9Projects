#pragma once
#include "CoreMinimal.h"
#include "CommandDispatcher.h"

class FPluginCommandHandler
{
public:
	static void RegisterAll(FCommandDispatcher& Dispatcher);

private:
	static TSharedPtr<FJsonObject> HandleList(const TSharedPtr<FJsonObject>& Args, bool bForce);
	static TSharedPtr<FJsonObject> HandleEnable(const TSharedPtr<FJsonObject>& Args, bool bForce);
	static TSharedPtr<FJsonObject> HandleDisable(const TSharedPtr<FJsonObject>& Args, bool bForce);
};
