#pragma once

#include "CoreMinimal.h"
#include "CommandDispatcher.h"

/**
 * Behavior tree editing.
 *
 * Everything here goes through the editor graph (UBehaviorTreeGraph), never through
 * UBehaviorTree::RootNode. The graph is the source of truth: CreateBTFromGraph() clears RootNode
 * and rebuilds it from scratch every time the asset is opened or recompiled, so a runtime tree
 * edited directly would be thrown away on the next open.
 */
class FBtCommandHandler
{
public:
	static void RegisterAll(FCommandDispatcher& Dispatcher);

private:
	static TSharedPtr<FJsonObject> HandleCreate(const TSharedPtr<FJsonObject>& Args, bool bForce);
	static TSharedPtr<FJsonObject> HandleInspect(const TSharedPtr<FJsonObject>& Args, bool bForce);
	static TSharedPtr<FJsonObject> HandleListNodeTypes(const TSharedPtr<FJsonObject>& Args, bool bForce);
	static TSharedPtr<FJsonObject> HandleAddNode(const TSharedPtr<FJsonObject>& Args, bool bForce);
	static TSharedPtr<FJsonObject> HandleAddDecorator(const TSharedPtr<FJsonObject>& Args, bool bForce);
	static TSharedPtr<FJsonObject> HandleAddService(const TSharedPtr<FJsonObject>& Args, bool bForce);
	static TSharedPtr<FJsonObject> HandleSetNode(const TSharedPtr<FJsonObject>& Args, bool bForce);
	static TSharedPtr<FJsonObject> HandleConnect(const TSharedPtr<FJsonObject>& Args, bool bForce);
	static TSharedPtr<FJsonObject> HandleDisconnect(const TSharedPtr<FJsonObject>& Args, bool bForce);
	static TSharedPtr<FJsonObject> HandleDeleteNode(const TSharedPtr<FJsonObject>& Args, bool bForce);
	static TSharedPtr<FJsonObject> HandleApplyGraph(const TSharedPtr<FJsonObject>& Args, bool bForce);
	static TSharedPtr<FJsonObject> HandleCompile(const TSharedPtr<FJsonObject>& Args, bool bForce);
	static TSharedPtr<FJsonObject> HandleSetBlackboard(const TSharedPtr<FJsonObject>& Args, bool bForce);
};
