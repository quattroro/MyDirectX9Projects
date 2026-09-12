#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "UObject/ReflectedTypeAccessors.h"

class UAIGraph;
class UAIGraphNode;
class UBehaviorTree;
class UBehaviorTreeGraph;
class UBlackboardData;
struct FGraphNodeClassHelper;

// ---------------------------------------------------------------------------------------------
// Linking against BehaviorTreeEditor
//
// Half of that module's UCLASSes are declared NO_API: UBehaviorTreeGraphNode (the base!), _Task,
// _Decorator, _Service, _SubtreeTask, _SimpleParallel, and UBehaviorTreeEditorTypes. Including
// their headers is fine — Classes/ is a public include path — but the usual reflection entry
// points are not exported:
//
//   T::StaticClass()      -> unresolved external. Use StaticClass<T>() instead: UHT emits an
//                            exported free-function specialisation for every reflected class
//                            whether or not the class itself carries an _API macro.
//   NewObject<T>(...)     -> same cause (it calls T::StaticClass()). Write
//                            NewObject<UAIGraphNode>(Outer, BtNodeClasses::Task()).
//   Cast<T>(Object)       -> same cause. Use BtCast<T>(Object).
//   FGraphNodeCreator<T>  -> same cause. Use FAISchemaAction_NewNode::SpawnNodeFromTemplate
//                            instantiated on UAIGraphNode, which is exported.
//   Non-virtual non-inline member functions -> unresolved external, with no workaround. Read the
//                            member variables the function would have read instead.
//
// Member variable access and virtual calls need no symbol, so those are always safe. AIGraph
// (UAIGraph, UAIGraphNode, UAIGraphSchema, FGraphNodeClassHelper, FAISchemaAction_NewNode) is
// fully exported and carries none of these restrictions.
// ---------------------------------------------------------------------------------------------

/** Cast<T> replacement for the NO_API graph node classes. */
template <typename T>
FORCEINLINE T* BtCast(UObject* Object)
{
	return (Object && Object->IsA(StaticClass<T>())) ? static_cast<T*>(Object) : nullptr;
}

/** UClass accessors for the graph node types, routed around the missing dllexport. */
namespace BtNodeClasses
{
	UClass* Base();
	UClass* Root();
	UClass* Composite();
	UClass* SimpleParallel();
	UClass* Task();
	UClass* SubtreeTask();
	UClass* Decorator();
	UClass* Service();
	UClass* CompositeDecorator();
}

/**
 * Pin category names, duplicated from BehaviorTreeEditorTypes.cpp:7-10.
 * UBehaviorTreeEditorTypes::PinCategory_* are NO_API statics, so they cannot be linked against.
 */
namespace BtPinCategory
{
	extern const FName MultipleNodes;
	extern const FName SingleComposite;
	extern const FName SingleTask;
	extern const FName SingleNode;
}

enum class EBtNodeKind : uint8
{
	Composite,
	Task,
	Decorator,
	Service,
	Root,
	Unknown
};

/** "composite" / "task" / "decorator" / "service" / "root" / "unknown". */
const TCHAR* BtNodeKindName(EBtNodeKind Kind);

/** Which of the four families a runtime UBTNode subclass belongs to. */
EBtNodeKind BtKindOfInstanceClass(const UClass* InstanceClass);

/**
 * Maps a runtime UBTNode subclass to the graph node class that has to host it.
 * The choice matters beyond cosmetics: UBehaviorTreeGraphNode::OnSubNodeAdded sorts a new subnode
 * into Decorators or Services by looking at the graph node's C++ type, not at NodeInstance.
 */
UClass* GraphNodeClassFor(const UClass* InstanceClass, EBtNodeKind& OutKind);

/** Human-facing type name: "BTTask_MoveTo" -> "MoveTo". Blueprint classes keep their name. */
FString ShortBtTypeName(const UClass* InstanceClass);

/**
 * The class list behind "add a node" — the only source that also sees Blueprint-derived nodes.
 *
 * FBehaviorTreeEditorModule builds its cache lazily, inside CreateBehaviorTreeEditor(), so it is
 * still empty until someone opens a behavior tree in the editor. When that has not happened this
 * builds an equivalent cache instead of failing. The result is owned by the caller and torn down
 * with it, which keeps its asset-registry delegates from outliving the engine at shutdown.
 */
TSharedPtr<FGraphNodeClassHelper> GetBtClassCache();

/**
 * Resolves a --type argument to a runtime node class.
 * Accepts the short name ("MoveTo"), the class name ("BTTask_MoveTo"), or a Blueprint class
 * ("BTT_Foo_C"). ExpectedKind narrows the search and shapes the error message; pass
 * EBtNodeKind::Unknown to search every family. Throws NOT_FOUND listing candidates.
 */
UClass* ResolveBtNodeClass(const FString& TypeName, EBtNodeKind ExpectedKind);

/** Loads a UBehaviorTree, or throws NOT_FOUND. */
UBehaviorTree* LoadBehaviorTreeOrThrow(const FString& Path);

/**
 * Returns the editor graph if the asset already has one, without creating it.
 *
 * A behavior tree that has never been opened in the editor has BTGraph == nullptr. Read commands
 * tolerate that and report what they can; see GetBtGraphForWrite for why they must not create one.
 */
UBehaviorTreeGraph* GetBtGraphForRead(UBehaviorTree* BehaviorTree);

/**
 * Returns the editor graph, creating it when the asset has neither a graph nor a tree.
 *
 * Refuses the one dangerous combination: no graph but a non-empty RootNode. Creating a graph there
 * would compile an empty tree over the existing one — CreateBTFromGraph clears RootNode before it
 * rebuilds — and the tree would be gone with no undo. The editor does the same thing on open, but
 * a person watching it happen can close without saving; a CLI call cannot.
 */
UBehaviorTreeGraph* GetBtGraphForWrite(UBehaviorTree* BehaviorTree);

/** The graph's root node, or nullptr if the graph somehow has none. */
UAIGraphNode* FindBtRootNode(UBehaviorTreeGraph* Graph);

/** Every graph node including decorators and services, in no particular order. */
void CollectAllBtNodes(UBehaviorTreeGraph* Graph, TArray<UAIGraphNode*>& OutNodes);

/** Nodes reachable from the root, in execution order (the same order bt inspect reports). */
void BuildBtExecutionOrder(UBehaviorTreeGraph* Graph, TArray<UAIGraphNode*>& OutNodes);

/**
 * Resolves a node reference, trying in turn:
 *   1. "Root"
 *   2. "#N"           execution-order index
 *   3. NodeName       the id bt inspect hands out
 *   4. NodeGuid       full, or a prefix of at least 8 characters
 *   5. object name    e.g. "BTTask_MoveTo_0"
 * Throws NOT_FOUND when nothing matches, and CLI_USAGE listing guids when a name is ambiguous.
 */
UAIGraphNode* FindBtNodeOrThrow(UBehaviorTreeGraph* Graph, const FString& Ref);

/** The id a node answers to: its NodeName, falling back to the object name. */
FString GetBtNodeId(const UAIGraphNode* Node);

/**
 * Children of a node in execution order.
 *
 * Execution order is decided by NodePosX, not by the order of UEdGraphPin::LinkedTo — see
 * CreateChildren() in BehaviorTreeGraph.cpp, which sorts with FCompareNodeXLocation before
 * building FBTCompositeChild entries. This sorts a copy, so it does not disturb the graph.
 */
void GetOrderedChildren(UAIGraphNode* Node, int32 OutputPinIndex, TArray<UAIGraphNode*>& OutChildren);

/** Creates a node of InstanceClass in the graph and gives it Id as its NodeName. */
UAIGraphNode* SpawnBtNode(UBehaviorTreeGraph* Graph, UClass* InstanceClass, const FString& Id, FVector2D Location);

/**
 * Attaches Parent -> Child, replacing whatever parent the child had.
 * The schema breaks the child's existing input link on its own, so this doubles as a re-parent.
 * OutputPinIndex picks the branch on a simple parallel node (0 = main task, 1 = background).
 */
void ConnectBtNodes(UBehaviorTreeGraph* Graph, UAIGraphNode* Parent, UAIGraphNode* Child, int32 OutputPinIndex);

/** Spaces a node's children evenly so that NodePosX encodes the order they are listed in. */
void SpaceChildrenEvenly(UAIGraphNode* Parent, const TArray<UAIGraphNode*>& OrderedChildren);

/**
 * Applies a {"Property": value} object onto a node instance.
 *
 * Adds two things on top of HandlerUtils::ApplyValues: a FBlackboardKeySelector given as a plain
 * string sets SelectedKeyName, and afterwards InitializeFromAsset re-runs so the selector resolves
 * against the tree's blackboard. Without that the key looks set in the editor but reads nothing at
 * runtime.
 */
void ApplyBtValues(UBehaviorTree* BehaviorTree, UAIGraphNode* Node, const TSharedPtr<FJsonObject>& Values);

/** Points the tree and its root node at a blackboard and re-resolves every key selector. */
void SetBtBlackboard(UBehaviorTree* BehaviorTree, UBehaviorTreeGraph* Graph, UBlackboardData* Blackboard);

/**
 * Positions every node: depth on Y, execution order on X.
 *
 * AutoArrange() cannot be used here — it measures Slate widgets, which only exist while the graph
 * is open in an editor window. This is a Reingold-Tilford sketch: leaves take successive columns,
 * a parent centres over its children, and a final pass re-separates siblings that grid snapping
 * pushed onto the same X, because equal X makes their execution order arbitrary.
 */
void LayoutBtGraph(UBehaviorTreeGraph* Graph, TArray<FString>& OutWarnings);

/** Closes any open editor for the asset, returning whether one was open. */
bool CloseBtEditors(UBehaviorTree* BehaviorTree);

/** Reopens the asset's editor (pair with CloseBtEditors). */
void ReopenBtEditors(UBehaviorTree* BehaviorTree);

/**
 * The tail every mutating command runs: recompile the runtime tree, optionally lay out and save.
 * Reads "noUpdate", "layout" and "save" from Args and reports what it did in OutData.
 */
void FinishBtEdit(UBehaviorTree* BehaviorTree, UBehaviorTreeGraph* Graph,
	const TSharedPtr<FJsonObject>& Args, const TSharedPtr<FJsonObject>& OutData,
	TArray<FString>& OutWarnings);

/** Copies collected warnings into OutData as a "warnings" array (always present). */
void WriteBtWarnings(const TArray<FString>& Warnings, const TSharedPtr<FJsonObject>& OutData);

/**
 * Suppresses UBehaviorTreeGraph::UpdateAsset for the duration of a batch of edits and runs it
 * once on scope exit. Without it, every AddSubNode() and every pin change recompiles the entire
 * runtime tree.
 *
 * bLockUpdates is a transient bit rather than a UPROPERTY, so leaving it set would turn every
 * later edit in this editor session into a silent no-op. Unlocking therefore happens in a
 * destructor, and that destructor swallows exceptions: throwing while the stack is already
 * unwinding from FCommandFailedException would call std::terminate and take the editor with it.
 */
struct FBtGraphUpdateGuard
{
	explicit FBtGraphUpdateGuard(UAIGraph* InGraph);
	~FBtGraphUpdateGuard();

	FBtGraphUpdateGuard(const FBtGraphUpdateGuard&) = delete;
	FBtGraphUpdateGuard& operator=(const FBtGraphUpdateGuard&) = delete;

private:
	TWeakObjectPtr<UAIGraph> Graph;
};
