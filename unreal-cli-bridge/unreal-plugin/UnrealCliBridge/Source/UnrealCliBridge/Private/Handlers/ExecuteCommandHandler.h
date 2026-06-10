#pragma once
#include "CoreMinimal.h"
#include "CommandDispatcher.h"

class FExecuteCommandHandler
{
public:
	static void RegisterAll(FCommandDispatcher& Dispatcher);

private:
	static TSharedPtr<FJsonObject> HandleExecute(const TSharedPtr<FJsonObject>& Args, bool bForce);
};
