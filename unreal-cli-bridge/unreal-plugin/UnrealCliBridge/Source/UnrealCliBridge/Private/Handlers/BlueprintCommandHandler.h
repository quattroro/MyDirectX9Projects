#pragma once
#include "CoreMinimal.h"
#include "CommandDispatcher.h"

class FBlueprintCommandHandler
{
public:
	static void RegisterAll(FCommandDispatcher& Dispatcher);

private:
	static TSharedPtr<FJsonObject> HandleInspect(const TSharedPtr<FJsonObject>& Args, bool bForce);
	static TSharedPtr<FJsonObject> HandleSetProperty(const TSharedPtr<FJsonObject>& Args, bool bForce);
};
