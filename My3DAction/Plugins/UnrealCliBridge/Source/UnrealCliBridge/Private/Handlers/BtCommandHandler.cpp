#include "BtCommandHandler.h"

#include "BtGraphSupport.h"
#include "HandlerUtils.h"

#include "AIGraphNode.h"
#include "AIGraphTypes.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "BehaviorTreeFactory.h"
#include "BehaviorTreeGraph.h"
#include "BehaviorTreeGraphNode.h"
#include "BehaviorTreeGraphNode_Root.h"

#include "BehaviorTree/BTCompositeNode.h"
#include "BehaviorTree/BTDecorator.h"
#include "BehaviorTree/BTNode.h"
#include "BehaviorTree/BTService.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BehaviorTreeTypes.h"
#include "BehaviorTree/BlackboardData.h"

#include "Misc/PackageName.h"
#include "Modules/ModuleManager.h"
#include "UObject/UnrealType.h"

DEFINE_LOG_CATEGORY_STATIC(LogUnrealCliBridgeBtCommands, Log, All);

// ---------------------------------------------------------------------------------------------
// Serialisation
// ---------------------------------------------------------------------------------------------

namespace
{

/** Guards against a malformed graph sending the recursive walk into a loop. */
constexpr int32 MaxInspectDepth = 64;

FString AssetPathOrEmpty(const UObject* Asset)
{
	return Asset ? Asset->GetPathName() : FString();
}

void WriteEditableValues(const UObject* Instance, const TSharedPtr<FJsonObject>& OutNode)
{
	TSharedPtr<FJsonObject> ValueObj = MakeShared<FJsonObject>();
	for (TFieldIterator<FProperty> PropIt(Instance->GetClass()); PropIt; ++PropIt)
	{
		FProperty* Property = *PropIt;
		if (!Property->HasAnyPropertyFlags(CPF_Edit))
		{
			continue;
		}
		// Transient properties are recomputed on load, and feeding them back through apply-graph
		// would either fail or overwrite something the engine owns. Keep this dump re-appliable.
		if (Property->HasAnyPropertyFlags(CPF_Transient | CPF_EditConst | CPF_DisableEditOnInstance))
		{
			continue;
		}

		// A blackboard key exports as a struct literal that carries AllowedTypes with it, and
		// AllowedTypes is filled in by the node's constructor (AddObjectFilter and friends).
		// Importing the literal back would wipe those filters and the key would then fail to
		// resolve. Report just the key name — which is also the form apply-graph accepts.
		if (const FStructProperty* StructProperty = CastField<FStructProperty>(Property))
		{
			if (StructProperty->Struct && StructProperty->Struct->GetFName() == TEXT("BlackboardKeySelector"))
			{
				const FBlackboardKeySelector* Selector =
					StructProperty->ContainerPtrToValuePtr<FBlackboardKeySelector>(Instance);
				ValueObj->SetStringField(Property->GetName(), Selector->SelectedKeyName.ToString());
				continue;
			}
		}

		FString ValueStr;
		Property->ExportTextItem_Direct(
			ValueStr, Property->ContainerPtrToValuePtr<void>(Instance), nullptr,
			const_cast<UObject*>(Instance), PPF_None);
		ValueObj->SetStringField(Property->GetName(), ValueStr);
	}
	OutNode->SetObjectField(TEXT("values"), ValueObj);
}

TSharedPtr<FJsonObject> SubNodeToJson(UAIGraphNode* Node, bool bWithValues)
{
	TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
	UObject* Instance = Node->NodeInstance;
	const UClass* InstanceClass = Instance ? Instance->GetClass() : nullptr;

	Obj->SetStringField(TEXT("id"), GetBtNodeId(Node));
	Obj->SetStringField(TEXT("type"), ShortBtTypeName(InstanceClass));
	Obj->SetStringField(TEXT("class"), InstanceClass ? InstanceClass->GetName() : FString());
	Obj->SetStringField(TEXT("kind"), BtNodeKindName(BtKindOfInstanceClass(InstanceClass)));
	Obj->SetStringField(TEXT("guid"), Node->NodeGuid.ToString());
	if (!Node->NodeComment.IsEmpty())
	{
		Obj->SetStringField(TEXT("comment"), Node->NodeComment);
	}
	if (bWithValues && Instance)
	{
		WriteEditableValues(Instance, Obj);
	}
	return Obj;
}

void WriteSubNodeArray(const TArray<TObjectPtr<UBehaviorTreeGraphNode>>& SubNodes,
	const TCHAR* Field, bool bWithValues, const TSharedPtr<FJsonObject>& OutNode)
{
	if (SubNodes.Num() == 0)
	{
		return;
	}
	TArray<TSharedPtr<FJsonValue>> Array;
	for (const TObjectPtr<UBehaviorTreeGraphNode>& SubNode : SubNodes)
	{
		if (UAIGraphNode* AsGraphNode = SubNode.Get())
		{
			Array.Add(MakeShared<FJsonValueObject>(SubNodeToJson(AsGraphNode, bWithValues)));
		}
	}
	OutNode->SetArrayField(Field, Array);
}

TSharedPtr<FJsonObject> NodeToJson(UAIGraphNode* Node, bool bWithValues, int32 Depth,
	int32& InOutExecIndex, TSet<UAIGraphNode*>& Visited, TArray<FString>& OutWarnings);

void WriteChildren(UAIGraphNode* Node, int32 PinIndex, const TCHAR* Field, bool bWithValues,
	int32 Depth, int32& InOutExecIndex, TSet<UAIGraphNode*>& Visited,
	TArray<FString>& OutWarnings, const TSharedPtr<FJsonObject>& OutNode)
{
	TArray<UAIGraphNode*> Children;
	GetOrderedChildren(Node, PinIndex, Children);
	if (Children.Num() == 0)
	{
		return;
	}

	TArray<TSharedPtr<FJsonValue>> Array;
	for (UAIGraphNode* Child : Children)
	{
		TSharedPtr<FJsonObject> ChildObj =
			NodeToJson(Child, bWithValues, Depth + 1, InOutExecIndex, Visited, OutWarnings);
		if (ChildObj.IsValid())
		{
			Array.Add(MakeShared<FJsonValueObject>(ChildObj));
		}
	}
	if (Array.Num() > 0)
	{
		OutNode->SetArrayField(Field, Array);
	}
}

TSharedPtr<FJsonObject> NodeToJson(UAIGraphNode* Node, bool bWithValues, int32 Depth,
	int32& InOutExecIndex, TSet<UAIGraphNode*>& Visited, TArray<FString>& OutWarnings)
{
	if (!Node)
	{
		return nullptr;
	}
	if (Depth > MaxInspectDepth)
	{
		OutWarnings.Add(FString::Printf(
			TEXT("Stopped at depth %d under '%s'; the graph may contain a cycle."),
			MaxInspectDepth, *GetBtNodeId(Node)));
		return nullptr;
	}
	if (Visited.Contains(Node))
	{
		OutWarnings.Add(FString::Printf(
			TEXT("Node '%s' is reachable more than once; reported at its first position only."),
			*GetBtNodeId(Node)));
		return nullptr;
	}
	Visited.Add(Node);

	UObject* Instance = Node->NodeInstance;
	const UClass* InstanceClass = Instance ? Instance->GetClass() : nullptr;

	TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
	Obj->SetStringField(TEXT("id"), GetBtNodeId(Node));
	Obj->SetStringField(TEXT("type"), ShortBtTypeName(InstanceClass));
	Obj->SetStringField(TEXT("class"), InstanceClass ? InstanceClass->GetName() : FString());
	Obj->SetStringField(TEXT("kind"), BtNodeKindName(BtKindOfInstanceClass(InstanceClass)));
	Obj->SetStringField(TEXT("guid"), Node->NodeGuid.ToString());
	Obj->SetNumberField(TEXT("execIndex"), InOutExecIndex++);

	TArray<TSharedPtr<FJsonValue>> Pos;
	Pos.Add(MakeShared<FJsonValueNumber>(Node->NodePosX));
	Pos.Add(MakeShared<FJsonValueNumber>(Node->NodePosY));
	Obj->SetArrayField(TEXT("pos"), Pos);

	if (!Node->NodeComment.IsEmpty())
	{
		Obj->SetStringField(TEXT("comment"), Node->NodeComment);
	}
	if (!Node->ErrorMessage.IsEmpty())
	{
		Obj->SetStringField(TEXT("error"), Node->ErrorMessage);
		OutWarnings.Add(FString::Printf(TEXT("Node '%s': %s"), *GetBtNodeId(Node), *Node->ErrorMessage));
	}
	if (bWithValues && Instance)
	{
		WriteEditableValues(Instance, Obj);
	}

	if (UBehaviorTreeGraphNode* AsBtNode = BtCast<UBehaviorTreeGraphNode>(Node))
	{
		WriteSubNodeArray(AsBtNode->Decorators, TEXT("decorators"), bWithValues, Obj);
		WriteSubNodeArray(AsBtNode->Services, TEXT("services"), bWithValues, Obj);
	}

	// A simple parallel node keeps its main task on output pin 0 and the background branch on
	// pin 1; every other node has a single output pin holding all of its children.
	if (Node->GetOutputPin(1) != nullptr)
	{
		WriteChildren(Node, 0, TEXT("mainTask"), bWithValues, Depth, InOutExecIndex, Visited, OutWarnings, Obj);
		WriteChildren(Node, 1, TEXT("children"), bWithValues, Depth, InOutExecIndex, Visited, OutWarnings, Obj);
	}
	else
	{
		WriteChildren(Node, 0, TEXT("children"), bWithValues, Depth, InOutExecIndex, Visited, OutWarnings, Obj);
	}

	return Obj;
}

} // namespace

// ---------------------------------------------------------------------------------------------
// Shared plumbing for the mutating commands
// ---------------------------------------------------------------------------------------------

namespace
{

/**
 * Holds the state every mutating command needs: the asset, its graph, whether an editor window
 * had to be closed, and the warnings collected on the way.
 */
struct FBtEditSession
{
	UBehaviorTree* Tree = nullptr;
	UBehaviorTreeGraph* Graph = nullptr;
	bool bEditorWasOpen = false;
	TArray<FString> Warnings;
	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();

	explicit FBtEditSession(const TSharedPtr<FJsonObject>& Args, const TCHAR* Usage)
	{
		const FString Path = HandlerUtils::RequireStringArg(Args, TEXT("path"), Usage);
		Tree = LoadBehaviorTreeOrThrow(Path);
		bEditorWasOpen = CloseBtEditors(Tree);
		Graph = GetBtGraphForWrite(Tree);
		Data->SetStringField(TEXT("path"), Tree->GetPathName());
	}

	/** Recompile, optionally save, reopen the editor, and attach warnings. */
	TSharedPtr<FJsonObject> Finish(const TSharedPtr<FJsonObject>& Args)
	{
		FinishBtEdit(Tree, Graph, Args, Data, Warnings);
		if (bEditorWasOpen)
		{
			ReopenBtEditors(Tree);
			Data->SetBoolField(TEXT("editorReopened"), true);
		}
		WriteBtWarnings(Warnings, Data);
		return Data;
	}
};

FString OptionalString(const TSharedPtr<FJsonObject>& Args, const TCHAR* Field)
{
	FString Value;
	if (Args.IsValid())
	{
		Args->TryGetStringField(Field, Value);
	}
	return Value;
}

TSharedPtr<FJsonObject> OptionalObject(const TSharedPtr<FJsonObject>& Args, const TCHAR* Field)
{
	const TSharedPtr<FJsonObject>* Object = nullptr;
	if (Args.IsValid() && Args->TryGetObjectField(Field, Object))
	{
		return *Object;
	}
	return nullptr;
}

/** Reads an optional integer, returning Fallback when the field is absent. */
int32 OptionalInt(const TSharedPtr<FJsonObject>& Args, const TCHAR* Field, int32 Fallback)
{
	double Number = 0.0;
	if (Args.IsValid() && Args->TryGetNumberField(Field, Number))
	{
		return static_cast<int32>(Number);
	}
	return Fallback;
}

/** Reads an optional [x, y] position. Returns false when the field is absent. */
bool OptionalPos(const TSharedPtr<FJsonObject>& Args, FVector2D& OutPos)
{
	const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
	if (!Args.IsValid() || !Args->TryGetArrayField(TEXT("pos"), Values) || Values->Num() < 2)
	{
		return false;
	}
	OutPos = FVector2D((*Values)[0]->AsNumber(), (*Values)[1]->AsNumber());
	return true;
}

/** Refuses an id that is already taken, so that later lookups stay unambiguous. */
void EnsureIdIsFree(UBehaviorTreeGraph* Graph, const FString& Id)
{
	if (Id.IsEmpty())
	{
		return;
	}
	TArray<UAIGraphNode*> AllNodes;
	CollectAllBtNodes(Graph, AllNodes);
	for (UAIGraphNode* Node : AllNodes)
	{
		if (GetBtNodeId(Node).Equals(Id, ESearchCase::IgnoreCase))
		{
			throw FCommandFailedException(TEXT("ALREADY_EXISTS"),
				FString::Printf(TEXT("This tree already has a node called '%s'. Ids have to be unique to be usable."), *Id));
		}
	}
}

/** Which output pin a child hangs off: 0 normally, 0/1 for the two halves of a simple parallel. */
int32 ResolveOutputPinIndex(UAIGraphNode* Parent, const FString& AsBranch)
{
	if (AsBranch.IsEmpty())
	{
		return 0;
	}
	if (AsBranch.Equals(TEXT("main-task"), ESearchCase::IgnoreCase))
	{
		return 0;
	}
	if (AsBranch.Equals(TEXT("background"), ESearchCase::IgnoreCase))
	{
		if (!Parent || Parent->GetOutputPin(1) == nullptr)
		{
			throw FCommandFailedException(TEXT("CLI_USAGE"),
				FString::Printf(TEXT("'%s' has no background branch; --as background only applies to a simple parallel."),
					*GetBtNodeId(Parent)));
		}
		return 1;
	}
	throw FCommandFailedException(TEXT("CLI_USAGE"),
		FString::Printf(TEXT("Unknown --as '%s'. Expected main-task or background."), *AsBranch));
}

/** Attaches Child under Parent at the given position in the child order (-1 appends). */
void AttachChild(UBehaviorTreeGraph* Graph, UAIGraphNode* Parent, UAIGraphNode* Child, int32 PinIndex, int32 Index)
{
	ConnectBtNodes(Graph, Parent, Child, PinIndex);

	TArray<UAIGraphNode*> Children;
	GetOrderedChildren(Parent, PinIndex, Children);
	Children.Remove(Child);
	const int32 ClampedIndex = (Index < 0 || Index > Children.Num()) ? Children.Num() : Index;
	Children.Insert(Child, ClampedIndex);

	SpaceChildrenEvenly(Parent, Children);
}

/** Creates a decorator or service and attaches it to Parent. */
TSharedPtr<FJsonObject> AddSubNodeCommand(const TSharedPtr<FJsonObject>& Args, EBtNodeKind Kind, const TCHAR* Usage)
{
	FBtEditSession Session(Args, Usage);

	const FString TypeName = HandlerUtils::RequireStringArg(Args, TEXT("type"),
		Kind == EBtNodeKind::Decorator
			? TEXT("bt add-decorator requires --type.")
			: TEXT("bt add-service requires --type."));
	const FString NodeRef = HandlerUtils::RequireStringArg(Args, TEXT("node"),
		Kind == EBtNodeKind::Decorator
			? TEXT("bt add-decorator requires --node (the node to attach to).")
			: TEXT("bt add-service requires --node (the node to attach to)."));

	const FString Id = OptionalString(Args, TEXT("id"));
	EnsureIdIsFree(Session.Graph, Id);

	UClass* InstanceClass = ResolveBtNodeClass(TypeName, Kind);
	UAIGraphNode* Parent = FindBtNodeOrThrow(Session.Graph, NodeRef);

	EBtNodeKind GraphKind = EBtNodeKind::Unknown;
	UClass* GraphNodeClass = GraphNodeClassFor(InstanceClass, GraphKind);

	{
		FBtGraphUpdateGuard Guard(Session.Graph);
		Session.Graph->Modify();
		Parent->Modify();

		// AddSubNode does the rest: rename into the graph, guid, PostPlacedNewNode (which builds
		// NodeInstance from ClassData), pins, and the Decorators/Services bookkeeping.
		UAIGraphNode* SubNode = NewObject<UAIGraphNode>(Session.Graph, GraphNodeClass);
		if (UBehaviorTreeGraphNode* AsBtNode = BtCast<UBehaviorTreeGraphNode>(SubNode))
		{
			AsBtNode->ClassData = FGraphNodeClassData(InstanceClass, FString());
		}
		Parent->AddSubNode(SubNode, Session.Graph);

		if (!Id.IsEmpty())
		{
			if (UBTNode* Instance = Cast<UBTNode>(SubNode->NodeInstance))
			{
				Instance->NodeName = Id;
			}
		}

		// Order within Decorators / Services is the order they run in.
		const int32 Index = OptionalInt(Args, TEXT("index"), -1);
		if (Index >= 0)
		{
			UBehaviorTreeGraphNode* ParentBtNode = BtCast<UBehaviorTreeGraphNode>(Parent);
			if (ParentBtNode)
			{
				TArray<TObjectPtr<UBehaviorTreeGraphNode>>& Bucket =
					(Kind == EBtNodeKind::Decorator) ? ParentBtNode->Decorators : ParentBtNode->Services;
				UBehaviorTreeGraphNode* Moved = BtCast<UBehaviorTreeGraphNode>(SubNode);
				if (Moved && Bucket.Remove(Moved) > 0)
				{
					Bucket.Insert(Moved, FMath::Clamp(Index, 0, Bucket.Num()));
				}
				// SubNodes is the array AIGraph's shared code walks; keep the two consistent.
				if (Parent->SubNodes.Remove(SubNode) > 0)
				{
					Parent->SubNodes.Insert(SubNode, FMath::Clamp(Index, 0, Parent->SubNodes.Num()));
				}
			}
		}

		ApplyBtValues(Session.Tree, SubNode, OptionalObject(Args, TEXT("values")));
		Session.Data->SetStringField(TEXT("id"), GetBtNodeId(SubNode));
		Session.Data->SetStringField(TEXT("guid"), SubNode->NodeGuid.ToString());
		Session.Data->SetStringField(TEXT("attachedTo"), GetBtNodeId(Parent));
	}

	return Session.Finish(Args);
}

} // namespace
// ---------------------------------------------------------------------------------------------
// Registration
// ---------------------------------------------------------------------------------------------

void FBtCommandHandler::RegisterAll(FCommandDispatcher& Dispatcher)
{
	Dispatcher.Register(TEXT("bt.create"), &HandleCreate);
	Dispatcher.Register(TEXT("bt.inspect"), &HandleInspect);
	Dispatcher.Register(TEXT("bt.list-node-types"), &HandleListNodeTypes);
	Dispatcher.Register(TEXT("bt.add-node"), &HandleAddNode);
	Dispatcher.Register(TEXT("bt.add-decorator"), &HandleAddDecorator);
	Dispatcher.Register(TEXT("bt.add-service"), &HandleAddService);
	Dispatcher.Register(TEXT("bt.set-node"), &HandleSetNode);
	Dispatcher.Register(TEXT("bt.connect"), &HandleConnect);
	Dispatcher.Register(TEXT("bt.disconnect"), &HandleDisconnect);
	Dispatcher.Register(TEXT("bt.delete-node"), &HandleDeleteNode);
	Dispatcher.Register(TEXT("bt.apply-graph"), &HandleApplyGraph);
	Dispatcher.Register(TEXT("bt.compile"), &HandleCompile);
	Dispatcher.Register(TEXT("bt.set-blackboard"), &HandleSetBlackboard);
}

// ---------------------------------------------------------------------------------------------
// bt.create
// ---------------------------------------------------------------------------------------------

TSharedPtr<FJsonObject> FBtCommandHandler::HandleCreate(const TSharedPtr<FJsonObject>& Args, bool bForce)
{
	const FString Path = HandlerUtils::RequireStringArg(Args, TEXT("path"), TEXT("bt create requires --path."));
	const FString ObjectPath = HandlerUtils::NormalizeObjectPath(Path);

	IAssetRegistry& AssetRegistry =
		FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
	const FAssetData Existing = AssetRegistry.GetAssetByObjectPath(FSoftObjectPath(ObjectPath));
	if (Existing.IsValid() && !bForce)
	{
		throw FCommandFailedException(TEXT("FORCE_REQUIRED"),
			FString::Printf(TEXT("Asset already exists: %s. Use --force to overwrite."), *Path));
	}

	const FString PackageName = FPackageName::ObjectPathToPackageName(ObjectPath);
	const FString AssetName   = FPackageName::GetLongPackageAssetName(PackageName);
	const FString PackagePath = FPackageName::GetLongPackagePath(PackageName);

	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools")).Get();
	UBehaviorTreeFactory* Factory = NewObject<UBehaviorTreeFactory>();
	UBehaviorTree* Tree = Cast<UBehaviorTree>(
		AssetTools.CreateAsset(AssetName, PackagePath, UBehaviorTree::StaticClass(), Factory));
	if (!Tree)
	{
		throw FCommandFailedException(TEXT("OPERATION_FAILED"),
			FString::Printf(TEXT("Could not create behavior tree: %s"), *Path));
	}

	TArray<FString> Warnings;
	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("path"), Tree->GetPathName());
	Data->SetBoolField(TEXT("created"), true);

	// A freshly created asset has no graph until it is opened; build it now so that the very next
	// add-node has somewhere to go.
	UBehaviorTreeGraph* Graph = GetBtGraphForWrite(Tree);

	const FString BlackboardPath = OptionalString(Args, TEXT("blackboard"));
	if (!BlackboardPath.IsEmpty())
	{
		UBlackboardData* Blackboard =
			LoadObject<UBlackboardData>(nullptr, *HandlerUtils::NormalizeObjectPath(BlackboardPath));
		if (!Blackboard)
		{
			throw FCommandFailedException(TEXT("NOT_FOUND"),
				FString::Printf(TEXT("Blackboard not found: %s"), *BlackboardPath));
		}
		SetBtBlackboard(Tree, Graph, Blackboard);
		Data->SetStringField(TEXT("blackboard"), Blackboard->GetPathName());
	}

	FinishBtEdit(Tree, Graph, Args, Data, Warnings);
	WriteBtWarnings(Warnings, Data);
	return Data;
}

// ---------------------------------------------------------------------------------------------
// bt.add-node / bt.add-decorator / bt.add-service
// ---------------------------------------------------------------------------------------------

TSharedPtr<FJsonObject> FBtCommandHandler::HandleAddNode(const TSharedPtr<FJsonObject>& Args, bool /*bForce*/)
{
	FBtEditSession Session(Args, TEXT("bt add-node requires --path."));

	const FString TypeName = HandlerUtils::RequireStringArg(Args, TEXT("type"), TEXT("bt add-node requires --type."));
	const FString Id = OptionalString(Args, TEXT("id"));
	EnsureIdIsFree(Session.Graph, Id);

	UClass* InstanceClass = ResolveBtNodeClass(TypeName, EBtNodeKind::Unknown);
	const EBtNodeKind Kind = BtKindOfInstanceClass(InstanceClass);
	if (Kind == EBtNodeKind::Decorator || Kind == EBtNodeKind::Service)
	{
		throw FCommandFailedException(TEXT("CLI_USAGE"),
			FString::Printf(TEXT("'%s' is a %s. Attach it with `bt add-%s --node <id> --type %s`."),
				*TypeName, BtNodeKindName(Kind), BtNodeKindName(Kind), *TypeName));
	}

	const FString ParentRef = OptionalString(Args, TEXT("parent"));
	UAIGraphNode* Parent = ParentRef.IsEmpty() ? nullptr : FindBtNodeOrThrow(Session.Graph, ParentRef);

	FVector2D Location(0.f, 0.f);
	const bool bExplicitPos = OptionalPos(Args, Location);
	if (!bExplicitPos && Parent)
	{
		Location = FVector2D(Parent->NodePosX, Parent->NodePosY + 160);
	}

	{
		FBtGraphUpdateGuard Guard(Session.Graph);
		Session.Graph->Modify();

		UAIGraphNode* Node = SpawnBtNode(Session.Graph, InstanceClass, Id, Location);
		ApplyBtValues(Session.Tree, Node, OptionalObject(Args, TEXT("values")));

		const FString Comment = OptionalString(Args, TEXT("comment"));
		if (!Comment.IsEmpty())
		{
			Node->NodeComment = Comment;
			Node->bCommentBubbleVisible = true;
		}

		if (Parent)
		{
			const int32 PinIndex = ResolveOutputPinIndex(Parent, OptionalString(Args, TEXT("as")));
			AttachChild(Session.Graph, Parent, Node, PinIndex, OptionalInt(Args, TEXT("index"), -1));
			Session.Data->SetStringField(TEXT("parent"), GetBtNodeId(Parent));
			Session.Data->SetBoolField(TEXT("attached"), true);
		}
		else
		{
			Session.Data->SetBoolField(TEXT("attached"), false);
			Session.Warnings.Add(FString::Printf(
				TEXT("'%s' has no parent, so it is not part of the compiled tree yet. ")
				TEXT("Attach it with `bt connect --from <parent> --to %s`."),
				*GetBtNodeId(Node), *GetBtNodeId(Node)));
		}

		Session.Data->SetStringField(TEXT("id"), GetBtNodeId(Node));
		Session.Data->SetStringField(TEXT("guid"), Node->NodeGuid.ToString());
		Session.Data->SetStringField(TEXT("kind"), BtNodeKindName(Kind));
	}

	return Session.Finish(Args);
}

TSharedPtr<FJsonObject> FBtCommandHandler::HandleAddDecorator(const TSharedPtr<FJsonObject>& Args, bool /*bForce*/)
{
	return AddSubNodeCommand(Args, EBtNodeKind::Decorator, TEXT("bt add-decorator requires --path."));
}

TSharedPtr<FJsonObject> FBtCommandHandler::HandleAddService(const TSharedPtr<FJsonObject>& Args, bool /*bForce*/)
{
	return AddSubNodeCommand(Args, EBtNodeKind::Service, TEXT("bt add-service requires --path."));
}

// ---------------------------------------------------------------------------------------------
// bt.set-node
// ---------------------------------------------------------------------------------------------

TSharedPtr<FJsonObject> FBtCommandHandler::HandleSetNode(const TSharedPtr<FJsonObject>& Args, bool /*bForce*/)
{
	FBtEditSession Session(Args, TEXT("bt set-node requires --path."));

	const FString NodeRef = HandlerUtils::RequireStringArg(Args, TEXT("node"), TEXT("bt set-node requires --node."));
	UAIGraphNode* Node = FindBtNodeOrThrow(Session.Graph, NodeRef);

	{
		FBtGraphUpdateGuard Guard(Session.Graph);
		Node->Modify();

		ApplyBtValues(Session.Tree, Node, OptionalObject(Args, TEXT("values")));

		const FString NewId = OptionalString(Args, TEXT("id"));
		if (!NewId.IsEmpty() && !NewId.Equals(GetBtNodeId(Node), ESearchCase::CaseSensitive))
		{
			EnsureIdIsFree(Session.Graph, NewId);
			if (UBTNode* Instance = Cast<UBTNode>(Node->NodeInstance))
			{
				Instance->Modify();
				Instance->NodeName = NewId;
			}
		}

		FVector2D Pos;
		if (OptionalPos(Args, Pos))
		{
			Node->NodePosX = static_cast<int32>(Pos.X);
			Node->NodePosY = static_cast<int32>(Pos.Y);
			Session.Warnings.Add(TEXT("Moving a node changes its execution order: siblings run left to right by X."));
		}

		FString Comment;
		if (Args->TryGetStringField(TEXT("comment"), Comment))
		{
			Node->NodeComment = Comment;
			Node->bCommentBubbleVisible = !Comment.IsEmpty();
		}

		Session.Data->SetStringField(TEXT("id"), GetBtNodeId(Node));
		Session.Data->SetStringField(TEXT("guid"), Node->NodeGuid.ToString());
	}

	return Session.Finish(Args);
}

// ---------------------------------------------------------------------------------------------
// bt.connect / bt.disconnect
// ---------------------------------------------------------------------------------------------

TSharedPtr<FJsonObject> FBtCommandHandler::HandleConnect(const TSharedPtr<FJsonObject>& Args, bool /*bForce*/)
{
	FBtEditSession Session(Args, TEXT("bt connect requires --path."));

	const FString FromRef = HandlerUtils::RequireStringArg(Args, TEXT("from"), TEXT("bt connect requires --from (the parent)."));
	const FString ToRef = HandlerUtils::RequireStringArg(Args, TEXT("to"), TEXT("bt connect requires --to (the child)."));

	UAIGraphNode* Parent = FindBtNodeOrThrow(Session.Graph, FromRef);
	UAIGraphNode* Child = FindBtNodeOrThrow(Session.Graph, ToRef);
	if (Parent == Child)
	{
		throw FCommandFailedException(TEXT("CLI_USAGE"), TEXT("A node cannot be its own parent."));
	}

	{
		FBtGraphUpdateGuard Guard(Session.Graph);
		Session.Graph->Modify();

		const int32 PinIndex = ResolveOutputPinIndex(Parent, OptionalString(Args, TEXT("as")));
		AttachChild(Session.Graph, Parent, Child, PinIndex, OptionalInt(Args, TEXT("index"), -1));

		Session.Data->SetStringField(TEXT("from"), GetBtNodeId(Parent));
		Session.Data->SetStringField(TEXT("to"), GetBtNodeId(Child));
	}

	return Session.Finish(Args);
}

TSharedPtr<FJsonObject> FBtCommandHandler::HandleDisconnect(const TSharedPtr<FJsonObject>& Args, bool bForce)
{
	FBtEditSession Session(Args, TEXT("bt disconnect requires --path."));

	const FString ToRef = HandlerUtils::RequireStringArg(Args, TEXT("to"),
		TEXT("bt disconnect requires --to (the child to detach)."));
	UAIGraphNode* Child = FindBtNodeOrThrow(Session.Graph, ToRef);

	UEdGraphPin* InPin = Child->GetInputPin(0);
	if (!InPin || InPin->LinkedTo.Num() == 0)
	{
		throw FCommandFailedException(TEXT("CLI_USAGE"),
			FString::Printf(TEXT("'%s' has no parent to detach from."), *GetBtNodeId(Child)));
	}

	// Detaching takes the node and everything under it out of the compiled tree.
	TArray<UAIGraphNode*> Subtree;
	{
		TArray<UAIGraphNode*> Stack{ Child };
		while (Stack.Num() > 0)
		{
			UAIGraphNode* Current = Stack.Pop();
			Subtree.Add(Current);
			for (int32 PinIndex = 0; PinIndex < 2; ++PinIndex)
			{
				TArray<UAIGraphNode*> Children;
				GetOrderedChildren(Current, PinIndex, Children);
				Stack.Append(Children);
			}
		}
	}
	if (Subtree.Num() > 1 && !bForce)
	{
		throw FCommandFailedException(TEXT("FORCE_REQUIRED"),
			FString::Printf(TEXT("Detaching '%s' removes %d nodes from the compiled tree. Use --force."),
				*GetBtNodeId(Child), Subtree.Num()));
	}

	{
		FBtGraphUpdateGuard Guard(Session.Graph);
		Session.Graph->Modify();
		Child->Modify();
		InPin->BreakAllPinLinks();
		Child->NodeConnectionListChanged();

		Session.Data->SetStringField(TEXT("to"), GetBtNodeId(Child));
		Session.Data->SetNumberField(TEXT("detachedSubtreeCount"), Subtree.Num());
	}

	return Session.Finish(Args);
}

// ---------------------------------------------------------------------------------------------
// bt.delete-node
// ---------------------------------------------------------------------------------------------

TSharedPtr<FJsonObject> FBtCommandHandler::HandleDeleteNode(const TSharedPtr<FJsonObject>& Args, bool bForce)
{
	if (!bForce)
	{
		throw FCommandFailedException(TEXT("FORCE_REQUIRED"), TEXT("bt delete-node always requires --force."));
	}

	FBtEditSession Session(Args, TEXT("bt delete-node requires --path."));

	const FString NodeRef = HandlerUtils::RequireStringArg(Args, TEXT("node"), TEXT("bt delete-node requires --node."));
	UAIGraphNode* Node = FindBtNodeOrThrow(Session.Graph, NodeRef);

	if (BtCast<UBehaviorTreeGraphNode_Root>(Node))
	{
		throw FCommandFailedException(TEXT("CLI_USAGE"), TEXT("The root node cannot be deleted."));
	}

	bool bKeepChildren = false;
	Args->TryGetBoolField(TEXT("keepChildren"), bKeepChildren);

	const FString DeletedId = GetBtNodeId(Node);
	const bool bIsSubNode = (Node->ParentNode != nullptr);

	{
		FBtGraphUpdateGuard Guard(Session.Graph);
		Session.Graph->Modify();

		if (bIsSubNode)
		{
			UAIGraphNode* Parent = Node->ParentNode;
			Parent->Modify();
			Parent->RemoveSubNode(Node);
			Session.Data->SetStringField(TEXT("detachedFrom"), GetBtNodeId(Parent));
		}
		else
		{
			TArray<UAIGraphNode*> Children;
			GetOrderedChildren(Node, 0, Children);

			UAIGraphNode* Grandparent = nullptr;
			if (UEdGraphPin* InPin = Node->GetInputPin(0))
			{
				if (InPin->LinkedTo.Num() > 0 && InPin->LinkedTo[0])
				{
					Grandparent = BtCast<UBehaviorTreeGraphNode>(InPin->LinkedTo[0]->GetOwningNode());
				}
			}

			Session.Graph->RemoveNode(Node, /*bBreakAllLinks=*/true);

			if (bKeepChildren && Grandparent)
			{
				for (UAIGraphNode* Child : Children)
				{
					ConnectBtNodes(Session.Graph, Grandparent, Child, 0);
				}
				TArray<UAIGraphNode*> Reordered;
				GetOrderedChildren(Grandparent, 0, Reordered);
				SpaceChildrenEvenly(Grandparent, Reordered);
				Session.Data->SetNumberField(TEXT("childrenReattached"), Children.Num());
			}
			else if (Children.Num() > 0)
			{
				Session.Data->SetNumberField(TEXT("childrenDetached"), Children.Num());
				Session.Warnings.Add(FString::Printf(
					TEXT("%d child node(s) of '%s' are now detached from the tree. ")
					TEXT("Pass --keep-children to reattach them to its parent instead."),
					Children.Num(), *DeletedId));
			}
		}

		Session.Data->SetStringField(TEXT("deleted"), DeletedId);
	}

	return Session.Finish(Args);
}

// ---------------------------------------------------------------------------------------------
// bt.compile / bt.set-blackboard
// ---------------------------------------------------------------------------------------------

TSharedPtr<FJsonObject> FBtCommandHandler::HandleCompile(const TSharedPtr<FJsonObject>& Args, bool /*bForce*/)
{
	FBtEditSession Session(Args, TEXT("bt compile requires --path."));
	return Session.Finish(Args);
}

TSharedPtr<FJsonObject> FBtCommandHandler::HandleSetBlackboard(const TSharedPtr<FJsonObject>& Args, bool /*bForce*/)
{
	FBtEditSession Session(Args, TEXT("bt set-blackboard requires --path."));

	const FString BlackboardPath = HandlerUtils::RequireStringArg(Args, TEXT("blackboard"),
		TEXT("bt set-blackboard requires --blackboard."));
	UBlackboardData* Blackboard =
		LoadObject<UBlackboardData>(nullptr, *HandlerUtils::NormalizeObjectPath(BlackboardPath));
	if (!Blackboard)
	{
		throw FCommandFailedException(TEXT("NOT_FOUND"),
			FString::Printf(TEXT("Blackboard not found: %s"), *BlackboardPath));
	}

	{
		FBtGraphUpdateGuard Guard(Session.Graph);
		SetBtBlackboard(Session.Tree, Session.Graph, Blackboard);
		Session.Data->SetStringField(TEXT("blackboard"), Blackboard->GetPathName());
	}

	return Session.Finish(Args);
}
// ---------------------------------------------------------------------------------------------
// bt.inspect
// ---------------------------------------------------------------------------------------------

TSharedPtr<FJsonObject> FBtCommandHandler::HandleInspect(const TSharedPtr<FJsonObject>& Args, bool /*bForce*/)
{
	const FString Path = HandlerUtils::RequireStringArg(Args, TEXT("path"), TEXT("bt inspect requires --path."));
	bool bWithValues = false;
	Args->TryGetBoolField(TEXT("withValues"), bWithValues);

	UBehaviorTree* BehaviorTree = LoadBehaviorTreeOrThrow(Path);

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("path"), BehaviorTree->GetPathName());
	Data->SetStringField(TEXT("blackboard"), AssetPathOrEmpty(BehaviorTree->BlackboardAsset));

	TArray<FString> Warnings;

	UBehaviorTreeGraph* Graph = GetBtGraphForRead(BehaviorTree);
	if (!Graph)
	{
		// Never open this asset in the editor and it has no graph yet. Report what the runtime
		// tree says rather than creating a graph, which would destroy that tree.
		Data->SetBoolField(TEXT("hasGraph"), false);
		Data->SetNumberField(TEXT("nodeCount"), 0);
		Warnings.Add(BehaviorTree->RootNode
			? TEXT("This asset has no editor graph yet, so its structure cannot be reported. "
			       "Open it once in the Behavior Tree editor and save, then inspect again.")
			: TEXT("This asset has no editor graph and no tree; it is empty."));

		TArray<TSharedPtr<FJsonValue>> WarningValues;
		for (const FString& Warning : Warnings)
		{
			WarningValues.Add(MakeShared<FJsonValueString>(Warning));
		}
		Data->SetArrayField(TEXT("warnings"), WarningValues);
		return Data;
	}

	Data->SetBoolField(TEXT("hasGraph"), true);

	UAIGraphNode* RootGraphNode = FindBtRootNode(Graph);
	if (!RootGraphNode)
	{
		throw FCommandFailedException(TEXT("OPERATION_FAILED"),
			FString::Printf(TEXT("Behavior tree graph has no root node: %s"), *Path));
	}

	if (UBehaviorTreeGraphNode* AsBtNode = BtCast<UBehaviorTreeGraphNode>(RootGraphNode))
	{
		WriteSubNodeArray(AsBtNode->Decorators, TEXT("rootDecorators"), bWithValues, Data);
	}

	int32 ExecIndex = 0;
	TSet<UAIGraphNode*> Visited;
	Visited.Add(RootGraphNode);

	TArray<UAIGraphNode*> RootChildren;
	GetOrderedChildren(RootGraphNode, 0, RootChildren);
	if (RootChildren.Num() > 0)
	{
		TSharedPtr<FJsonObject> RootObj =
			NodeToJson(RootChildren[0], bWithValues, 0, ExecIndex, Visited, Warnings);
		if (RootObj.IsValid())
		{
			Data->SetObjectField(TEXT("root"), RootObj);
		}
		if (RootChildren.Num() > 1)
		{
			Warnings.Add(TEXT("The root node has more than one child; only the first is part of the tree."));
		}
	}

	// Anything the walk never reached is present in the graph but excluded from the compiled tree.
	TArray<TSharedPtr<FJsonValue>> Detached;
	for (UEdGraphNode* EdNode : Graph->Nodes)
	{
		UAIGraphNode* Node = BtCast<UBehaviorTreeGraphNode>(EdNode);
		if (!Node || Visited.Contains(Node) || Node->ParentNode != nullptr)
		{
			continue;
		}
		Detached.Add(MakeShared<FJsonValueObject>(SubNodeToJson(Node, bWithValues)));
	}
	if (Detached.Num() > 0)
	{
		Data->SetArrayField(TEXT("detached"), Detached);
		Warnings.Add(FString::Printf(
			TEXT("%d node(s) are not connected to the root and are excluded from the compiled tree."),
			Detached.Num()));
	}

	Data->SetNumberField(TEXT("nodeCount"), ExecIndex);

	TArray<TSharedPtr<FJsonValue>> WarningValues;
	for (const FString& Warning : Warnings)
	{
		WarningValues.Add(MakeShared<FJsonValueString>(Warning));
	}
	Data->SetArrayField(TEXT("warnings"), WarningValues);

	return Data;
}

// ---------------------------------------------------------------------------------------------
// bt.list-node-types
// ---------------------------------------------------------------------------------------------

TSharedPtr<FJsonObject> FBtCommandHandler::HandleListNodeTypes(const TSharedPtr<FJsonObject>& Args, bool /*bForce*/)
{
	FString KindFilter;
	FString Filter;
	int32 Limit = 60;
	if (Args.IsValid())
	{
		Args->TryGetStringField(TEXT("kind"), KindFilter);
		Args->TryGetStringField(TEXT("filter"), Filter);
		double LimitNumber = 0.0;
		if (Args->TryGetNumberField(TEXT("limit"), LimitNumber) && LimitNumber > 0.0)
		{
			Limit = static_cast<int32>(LimitNumber);
		}
	}

	// This cache is the only source that also sees Blueprint-derived nodes.
	TSharedPtr<FGraphNodeClassHelper> ClassCache = GetBtClassCache();
	if (!ClassCache.IsValid())
	{
		throw FCommandFailedException(TEXT("OPERATION_FAILED"),
			TEXT("Could not build the behavior tree node class list."));
	}

	struct FKindEntry
	{
		const TCHAR* Name;
		UClass* Base;
	};
	const FKindEntry Kinds[] = {
		{ TEXT("composite"), UBTCompositeNode::StaticClass() },
		{ TEXT("task"),      UBTTaskNode::StaticClass()      },
		{ TEXT("decorator"), UBTDecorator::StaticClass()     },
		{ TEXT("service"),   UBTService::StaticClass()       },
	};

	if (!KindFilter.IsEmpty())
	{
		bool bKnown = false;
		for (const FKindEntry& Kind : Kinds)
		{
			bKnown = bKnown || KindFilter.Equals(Kind.Name, ESearchCase::IgnoreCase);
		}
		if (!bKnown)
		{
			throw FCommandFailedException(TEXT("CLI_USAGE"),
				FString::Printf(TEXT("Unknown --kind '%s'. Expected one of: composite, task, decorator, service."),
					*KindFilter));
		}
	}

	TArray<TSharedPtr<FJsonValue>> Types;
	int32 TotalMatches = 0;

	for (const FKindEntry& Kind : Kinds)
	{
		if (!KindFilter.IsEmpty() && !KindFilter.Equals(Kind.Name, ESearchCase::IgnoreCase))
		{
			continue;
		}

		TArray<FGraphNodeClassData> ClassData;
		ClassCache->GatherClasses(Kind.Base, ClassData);

		for (FGraphNodeClassData& Entry : ClassData)
		{
			if (Entry.IsAbstract() || Entry.IsDeprecated())
			{
				continue;
			}

			const FString ClassName = Entry.GetClassName();
			if (!Filter.IsEmpty() && !ClassName.Contains(Filter, ESearchCase::IgnoreCase))
			{
				continue;
			}

			++TotalMatches;
			if (Types.Num() >= Limit)
			{
				continue;
			}

			TSharedPtr<FJsonObject> TypeObj = MakeShared<FJsonObject>();
			// Report the name a user would pass to --type. GetClass() would force every
			// Blueprint node to load, so stay with the cached name and strip the prefix here.
			FString ShortName = ClassName;
			if (!ShortName.EndsWith(TEXT("_C"), ESearchCase::CaseSensitive))
			{
				static const TCHAR* Prefixes[] = {
					TEXT("BTTask_"), TEXT("BTComposite_"), TEXT("BTDecorator_"), TEXT("BTService_") };
				for (const TCHAR* Prefix : Prefixes)
				{
					if (ShortName.RemoveFromStart(Prefix, ESearchCase::CaseSensitive))
					{
						break;
					}
				}
			}
			TypeObj->SetStringField(TEXT("type"), ShortName);
			TypeObj->SetStringField(TEXT("class"), ClassName);
			TypeObj->SetStringField(TEXT("kind"), Kind.Name);
			TypeObj->SetBoolField(TEXT("blueprint"), Entry.IsBlueprint());

			// Category and tooltip come from class metadata and are empty for most nodes; a
			// Blueprint class that has not been loaded has neither. Omit rather than send "".
			const FString Category = Entry.GetCategory().ToString();
			if (!Category.IsEmpty())
			{
				TypeObj->SetStringField(TEXT("category"), Category);
			}
			const FString Tooltip = Entry.GetTooltip().ToString();
			if (!Tooltip.IsEmpty())
			{
				TypeObj->SetStringField(TEXT("tooltip"), Tooltip);
			}
			Types.Add(MakeShared<FJsonValueObject>(TypeObj));
		}
	}

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetArrayField(TEXT("types"), Types);
	Data->SetNumberField(TEXT("count"), Types.Num());
	Data->SetNumberField(TEXT("totalMatches"), TotalMatches);

	// Blueprint nodes are found through the asset registry, so a scan still in flight means this
	// list is incomplete. Say so rather than letting a missing type look like a missing class.
	IAssetRegistry& AssetRegistry =
		FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
	if (AssetRegistry.IsLoadingAssets())
	{
		Data->SetBoolField(TEXT("assetScanInProgress"), true);
	}

	if (TotalMatches > Types.Num())
	{
		Data->SetStringField(TEXT("message"),
			FString::Printf(TEXT("Showing %d of %d matches. Narrow with --filter or raise --limit."),
				Types.Num(), TotalMatches));
	}
	return Data;
}
// ---------------------------------------------------------------------------------------------
// bt.apply-graph
// ---------------------------------------------------------------------------------------------

namespace
{

/** Running totals and id bookkeeping for one apply-graph call. */
struct FApplyGraphState
{
	FBtEditSession* Session = nullptr;
	TSet<FString> UsedIds;
	int32 NodesCreated = 0;
	int32 DecoratorsCreated = 0;
	int32 ServicesCreated = 0;
};

/**
 * Node names default to the class's own label ("Selector", "Wait"), so several nodes of the same
 * type naturally want the same id — including in a bt inspect dump, which reports those defaults
 * verbatim. An id's job is to address exactly one node, so keep the readable name where it is free
 * and suffix it where it is not.
 *
 * A duplicate the caller wrote out explicitly is more likely to be a mistake, so that one is
 * reported as a warning rather than passing silently.
 */
FString ClaimId(FApplyGraphState& State, const FString& Requested, const FString& Fallback)
{
	const FString Base = Requested.IsEmpty() ? Fallback : Requested;
	if (!State.UsedIds.Contains(Base))
	{
		State.UsedIds.Add(Base);
		return Base;
	}

	for (int32 Suffix = 2; Suffix < 1000; ++Suffix)
	{
		const FString Candidate = FString::Printf(TEXT("%s_%d"), *Base, Suffix);
		if (State.UsedIds.Contains(Candidate))
		{
			continue;
		}
		State.UsedIds.Add(Candidate);
		if (!Requested.IsEmpty())
		{
			State.Session->Warnings.Add(FString::Printf(
				TEXT("Id '%s' was used more than once; this node became '%s'."), *Requested, *Candidate));
		}
		return Candidate;
	}
	return Base;
}

UAIGraphNode* BuildNodeFromJson(FApplyGraphState& State, const TSharedPtr<FJsonObject>& NodeJson, int32 Depth);

/** Creates the decorators or services listed on a node, in the order given. */
void BuildSubNodesFromJson(FApplyGraphState& State, UAIGraphNode* Parent,
	const TSharedPtr<FJsonObject>& NodeJson, const TCHAR* Field, EBtNodeKind Kind)
{
	const TArray<TSharedPtr<FJsonValue>>* Entries = nullptr;
	if (!NodeJson->TryGetArrayField(Field, Entries))
	{
		return;
	}

	UBehaviorTreeGraph* Graph = State.Session->Graph;
	for (const TSharedPtr<FJsonValue>& Entry : *Entries)
	{
		const TSharedPtr<FJsonObject> SubJson = Entry->AsObject();
		if (!SubJson.IsValid())
		{
			continue;
		}

		FString TypeName;
		if (!SubJson->TryGetStringField(TEXT("type"), TypeName) || TypeName.IsEmpty())
		{
			throw FCommandFailedException(TEXT("CLI_USAGE"),
				FString::Printf(TEXT("Every entry in '%s' needs a \"type\"."), Field));
		}

		UClass* InstanceClass = ResolveBtNodeClass(TypeName, Kind);
		EBtNodeKind ResolvedKind = EBtNodeKind::Unknown;
		UClass* GraphNodeClass = GraphNodeClassFor(InstanceClass, ResolvedKind);
		if (ResolvedKind != Kind)
		{
			throw FCommandFailedException(TEXT("CLI_USAGE"),
				FString::Printf(TEXT("'%s' is a %s, but it is listed under '%s'."),
					*TypeName, BtNodeKindName(ResolvedKind), Field));
		}

		UAIGraphNode* SubNode = NewObject<UAIGraphNode>(Graph, GraphNodeClass);
		if (UBehaviorTreeGraphNode* AsBtNode = BtCast<UBehaviorTreeGraphNode>(SubNode))
		{
			AsBtNode->ClassData = FGraphNodeClassData(InstanceClass, FString());
		}
		Parent->AddSubNode(SubNode, Graph);

		FString RequestedId;
		SubJson->TryGetStringField(TEXT("id"), RequestedId);
		const FString Id = ClaimId(State, RequestedId, GetBtNodeId(SubNode));
		if (UBTNode* Instance = Cast<UBTNode>(SubNode->NodeInstance))
		{
			Instance->NodeName = Id;
		}

		ApplyBtValues(State.Session->Tree, SubNode, OptionalObject(SubJson, TEXT("values")));

		FString Comment;
		if (SubJson->TryGetStringField(TEXT("comment"), Comment) && !Comment.IsEmpty())
		{
			SubNode->NodeComment = Comment;
			SubNode->bCommentBubbleVisible = true;
		}

		(Kind == EBtNodeKind::Decorator ? State.DecoratorsCreated : State.ServicesCreated)++;
	}
}

/** Creates the children listed under Field and hangs them off the given output pin, in order. */
void BuildChildrenFromJson(FApplyGraphState& State, UAIGraphNode* Parent,
	const TSharedPtr<FJsonObject>& NodeJson, const TCHAR* Field, int32 PinIndex, int32 Depth)
{
	const TArray<TSharedPtr<FJsonValue>>* Entries = nullptr;
	if (!NodeJson->TryGetArrayField(Field, Entries) || Entries->Num() == 0)
	{
		return;
	}

	TArray<UAIGraphNode*> Created;
	for (const TSharedPtr<FJsonValue>& Entry : *Entries)
	{
		const TSharedPtr<FJsonObject> ChildJson = Entry->AsObject();
		if (!ChildJson.IsValid())
		{
			continue;
		}
		UAIGraphNode* Child = BuildNodeFromJson(State, ChildJson, Depth + 1);
		ConnectBtNodes(State.Session->Graph, Parent, Child, PinIndex);
		Created.Add(Child);
	}

	// Array order is the intended execution order, and execution order lives in NodePosX.
	SpaceChildrenEvenly(Parent, Created);
}

UAIGraphNode* BuildNodeFromJson(FApplyGraphState& State, const TSharedPtr<FJsonObject>& NodeJson, int32 Depth)
{
	if (Depth > 64)
	{
		throw FCommandFailedException(TEXT("CLI_USAGE"),
			TEXT("The graph description nests more than 64 levels deep; check for a repeated fragment."));
	}

	FString TypeName;
	if (!NodeJson->TryGetStringField(TEXT("type"), TypeName) || TypeName.IsEmpty())
	{
		throw FCommandFailedException(TEXT("CLI_USAGE"), TEXT("Every node needs a \"type\"."));
	}

	UClass* InstanceClass = ResolveBtNodeClass(TypeName, EBtNodeKind::Unknown);
	const EBtNodeKind Kind = BtKindOfInstanceClass(InstanceClass);
	if (Kind == EBtNodeKind::Decorator || Kind == EBtNodeKind::Service)
	{
		throw FCommandFailedException(TEXT("CLI_USAGE"),
			FString::Printf(TEXT("'%s' is a %s; list it under \"%ss\" on its owning node, not under \"children\"."),
				*TypeName, BtNodeKindName(Kind), BtNodeKindName(Kind)));
	}

	UAIGraphNode* Node = SpawnBtNode(State.Session->Graph, InstanceClass, FString(), FVector2D::ZeroVector);
	State.NodesCreated++;

	FString RequestedId;
	NodeJson->TryGetStringField(TEXT("id"), RequestedId);
	const FString Id = ClaimId(State, RequestedId, GetBtNodeId(Node));
	if (UBTNode* Instance = Cast<UBTNode>(Node->NodeInstance))
	{
		Instance->NodeName = Id;
	}

	ApplyBtValues(State.Session->Tree, Node, OptionalObject(NodeJson, TEXT("values")));

	FString Comment;
	if (NodeJson->TryGetStringField(TEXT("comment"), Comment) && !Comment.IsEmpty())
	{
		Node->NodeComment = Comment;
		Node->bCommentBubbleVisible = true;
	}

	FVector2D Pos;
	if (OptionalPos(NodeJson, Pos))
	{
		Node->NodePosX = static_cast<int32>(Pos.X);
		Node->NodePosY = static_cast<int32>(Pos.Y);
	}

	BuildSubNodesFromJson(State, Node, NodeJson, TEXT("decorators"), EBtNodeKind::Decorator);
	BuildSubNodesFromJson(State, Node, NodeJson, TEXT("services"), EBtNodeKind::Service);

	// A simple parallel splits its children across two pins; everything else uses one.
	if (Node->GetOutputPin(1) != nullptr)
	{
		BuildChildrenFromJson(State, Node, NodeJson, TEXT("mainTask"), 0, Depth);
		BuildChildrenFromJson(State, Node, NodeJson, TEXT("children"), 1, Depth);
	}
	else
	{
		BuildChildrenFromJson(State, Node, NodeJson, TEXT("children"), 0, Depth);
	}

	return Node;
}

} // namespace

TSharedPtr<FJsonObject> FBtCommandHandler::HandleApplyGraph(const TSharedPtr<FJsonObject>& Args, bool bForce)
{
	const TSharedPtr<FJsonObject> Graph = OptionalObject(Args, TEXT("graph"));
	if (!Graph.IsValid())
	{
		throw FCommandFailedException(TEXT("CLI_USAGE"),
			TEXT("bt apply-graph requires --graph or --graph-file."));
	}

	FBtEditSession Session(Args, TEXT("bt apply-graph requires --path."));

	bool bClear = false;
	Args->TryGetBoolField(TEXT("clear"), bClear);
	Graph->TryGetBoolField(TEXT("clear"), bClear);

	UAIGraphNode* RootNode = FindBtRootNode(Session.Graph);
	if (!RootNode)
	{
		throw FCommandFailedException(TEXT("OPERATION_FAILED"), TEXT("This graph has no root node."));
	}

	// Everything except the root counts as existing content.
	int32 PreviousNodeCount = 0;
	for (UEdGraphNode* EdNode : Session.Graph->Nodes)
	{
		if (Cast<UAIGraphNode>(EdNode) && EdNode != RootNode)
		{
			++PreviousNodeCount;
		}
	}
	if (PreviousNodeCount > 0 && !bClear && !bForce)
	{
		throw FCommandFailedException(TEXT("FORCE_REQUIRED"),
			FString::Printf(TEXT("'%s' already has %d node(s). Pass --clear to replace the tree, or --force to build ")
				TEXT("alongside what is there. Run `bt inspect --json` first if you want a backup."),
				*Session.Tree->GetName(), PreviousNodeCount));
	}
	Session.Data->SetNumberField(TEXT("previousNodeCount"), PreviousNodeCount);

	FApplyGraphState State;
	State.Session = &Session;

	{
		FBtGraphUpdateGuard Guard(Session.Graph);
		Session.Graph->Modify();
		Session.Tree->Modify();

		// The blackboard has to be in place before any node is built: a node resolves its key
		// selectors in InitializeFromAsset, against whatever blackboard the tree points at.
		FString BlackboardPath;
		if (Graph->TryGetStringField(TEXT("blackboard"), BlackboardPath) && !BlackboardPath.IsEmpty())
		{
			UBlackboardData* Blackboard =
				LoadObject<UBlackboardData>(nullptr, *HandlerUtils::NormalizeObjectPath(BlackboardPath));
			if (!Blackboard)
			{
				throw FCommandFailedException(TEXT("NOT_FOUND"),
					FString::Printf(TEXT("Blackboard not found: %s"), *BlackboardPath));
			}
			SetBtBlackboard(Session.Tree, Session.Graph, Blackboard);
			Session.Data->SetStringField(TEXT("blackboard"), Blackboard->GetPathName());
		}

		if (bClear)
		{
			TArray<UEdGraphNode*> Doomed;
			for (UEdGraphNode* EdNode : Session.Graph->Nodes)
			{
				if (EdNode != RootNode && Cast<UAIGraphNode>(EdNode))
				{
					Doomed.Add(EdNode);
				}
			}
			for (UEdGraphNode* EdNode : Doomed)
			{
				Session.Graph->RemoveNode(EdNode, /*bBreakAllLinks=*/true);
			}
			if (UBehaviorTreeGraphNode* RootBtNode = BtCast<UBehaviorTreeGraphNode>(RootNode))
			{
				RootBtNode->Modify();
				RootNode->RemoveAllSubNodes();
				RootBtNode->Decorators.Reset();
				RootBtNode->Services.Reset();
			}
			Session.Data->SetBoolField(TEXT("cleared"), true);
		}
		else
		{
			// Ids already in the graph are taken, so a new node cannot shadow one of them.
			TArray<UAIGraphNode*> Existing;
			CollectAllBtNodes(Session.Graph, Existing);
			for (UAIGraphNode* Node : Existing)
			{
				State.UsedIds.Add(GetBtNodeId(Node));
			}
		}

		BuildSubNodesFromJson(State, RootNode, Graph, TEXT("rootDecorators"), EBtNodeKind::Decorator);

		const TSharedPtr<FJsonObject> RootJson = OptionalObject(Graph, TEXT("root"));
		if (RootJson.IsValid())
		{
			UAIGraphNode* TreeRoot = BuildNodeFromJson(State, RootJson, 0);
			ConnectBtNodes(Session.Graph, RootNode, TreeRoot, 0);
			TreeRoot->NodePosX = RootNode->NodePosX;
			TreeRoot->NodePosY = RootNode->NodePosY + 160;
		}
		else
		{
			Session.Warnings.Add(TEXT("The graph description has no \"root\", so the tree is empty."));
		}

		// Positions only matter relative to each other, and only X carries meaning, so unless the
		// description pinned nodes down it is always worth laying the whole thing out.
		bool bLayout = true;
		Args->TryGetBoolField(TEXT("layout"), bLayout);
		Graph->TryGetBoolField(TEXT("layout"), bLayout);
		if (bLayout)
		{
			LayoutBtGraph(Session.Graph, Session.Warnings);
		}

		Session.Data->SetNumberField(TEXT("nodesCreated"), State.NodesCreated);
		Session.Data->SetNumberField(TEXT("decorators"), State.DecoratorsCreated);
		Session.Data->SetNumberField(TEXT("services"), State.ServicesCreated);
	}

	return Session.Finish(Args);
}
