#pragma once
#include "CoreMinimal.h"
#include "CommandDispatcher.h"

class FEditorCommandHandler
{
public:
	static void RegisterAll(FCommandDispatcher& Dispatcher);

private:
	static TSharedPtr<FJsonObject> HandleStatus(const TSharedPtr<FJsonObject>& Args, bool bForce);
	static TSharedPtr<FJsonObject> HandlePlay(const TSharedPtr<FJsonObject>& Args, bool bForce);
	static TSharedPtr<FJsonObject> HandlePause(const TSharedPtr<FJsonObject>& Args, bool bForce);
	static TSharedPtr<FJsonObject> HandleStop(const TSharedPtr<FJsonObject>& Args, bool bForce);
	static TSharedPtr<FJsonObject> HandleCompile(const TSharedPtr<FJsonObject>& Args, bool bForce);
	static TSharedPtr<FJsonObject> HandleRefresh(const TSharedPtr<FJsonObject>& Args, bool bForce);
	static TSharedPtr<FJsonObject> HandleReadLog(const TSharedPtr<FJsonObject>& Args, bool bForce);
	static TSharedPtr<FJsonObject> HandleScreenshot(const TSharedPtr<FJsonObject>& Args, bool bForce);
	static TSharedPtr<FJsonObject> HandleExecuteMenu(const TSharedPtr<FJsonObject>& Args, bool bForce);
};
