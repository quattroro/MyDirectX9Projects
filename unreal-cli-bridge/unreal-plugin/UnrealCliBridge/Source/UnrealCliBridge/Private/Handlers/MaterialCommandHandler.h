#pragma once
#include "CoreMinimal.h"
#include "CommandDispatcher.h"

class FMaterialCommandHandler
{
public:
	static void RegisterAll(FCommandDispatcher& Dispatcher);

private:
	static TSharedPtr<FJsonObject> HandleCreate(const TSharedPtr<FJsonObject>& Args, bool bForce);
	static TSharedPtr<FJsonObject> HandleInspect(const TSharedPtr<FJsonObject>& Args, bool bForce);
	static TSharedPtr<FJsonObject> HandleListNodeTypes(const TSharedPtr<FJsonObject>& Args, bool bForce);
	static TSharedPtr<FJsonObject> HandleAddNode(const TSharedPtr<FJsonObject>& Args, bool bForce);
	static TSharedPtr<FJsonObject> HandleSetNode(const TSharedPtr<FJsonObject>& Args, bool bForce);
	static TSharedPtr<FJsonObject> HandleConnect(const TSharedPtr<FJsonObject>& Args, bool bForce);
	static TSharedPtr<FJsonObject> HandleDisconnect(const TSharedPtr<FJsonObject>& Args, bool bForce);
	static TSharedPtr<FJsonObject> HandleDeleteNode(const TSharedPtr<FJsonObject>& Args, bool bForce);
	static TSharedPtr<FJsonObject> HandleSetProperty(const TSharedPtr<FJsonObject>& Args, bool bForce);
	static TSharedPtr<FJsonObject> HandleApplyGraph(const TSharedPtr<FJsonObject>& Args, bool bForce);
	static TSharedPtr<FJsonObject> HandleCompile(const TSharedPtr<FJsonObject>& Args, bool bForce);
	static TSharedPtr<FJsonObject> HandleCreateInstance(const TSharedPtr<FJsonObject>& Args, bool bForce);
	static TSharedPtr<FJsonObject> HandleSetInstanceParam(const TSharedPtr<FJsonObject>& Args, bool bForce);
};
