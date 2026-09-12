#include "BtGraphSupport.h"

#include "CommandDispatcher.h"
#include "HandlerUtils.h"

#include "AIGraph.h"
#include "AIGraphNode.h"
#include "AIGraphSchema.h"
#include "AIGraphTypes.h"
#include "BehaviorTreeEditorModule.h"
#include "BehaviorTreeEditorTypes.h"
#include "BehaviorTreeGraph.h"
#include "BehaviorTreeGraphNode.h"
#include "BehaviorTreeGraphNode_Composite.h"
#include "BehaviorTreeGraphNode_CompositeDecorator.h"
#include "BehaviorTreeGraphNode_Decorator.h"
#include "BehaviorTreeGraphNode_Root.h"
#include "BehaviorTreeGraphNode_Service.h"
#include "BehaviorTreeGraphNode_SimpleParallel.h"
#include "BehaviorTreeGraphNode_SubtreeTask.h"
#include "BehaviorTreeGraphNode_Task.h"
#include "EdGraphSchema_BehaviorTree.h"

#include "BehaviorTree/BTCompositeNode.h"
#include "BehaviorTree/BTDecorator.h"
#include "BehaviorTree/BTNode.h"
#include "BehaviorTree/BTService.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BehaviorTreeTypes.h"
#include "BehaviorTree/BlackboardData.h"
#include "BehaviorTree/Composites/BTComposite_SimpleParallel.h"
#include "BehaviorTree/Decorators/BTDecorator_BlueprintBase.h"
#include "BehaviorTree/Services/BTService_BlueprintBase.h"
#include "BehaviorTree/Tasks/BTTask_BlueprintBase.h"
#include "BehaviorTree/Tasks/BTTask_RunBehavior.h"

#include "Editor.h"
#include "EdGraph/EdGraphPin.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Modules/ModuleManager.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "UObject/UObjectIterator.h"
#include "UObject/UnrealType.h"

DEFINE_LOG_CATEGORY_STATIC(LogUnrealCliBridgeBt, Log, All);

// ---------------------------------------------------------------------------------------------
// Class plumbing
// ---------------------------------------------------------------------------------------------

namespace BtNodeClasses
{
	UClass* Base()               { return StaticClass<UBehaviorTreeGraphNode>(); }
	UClass* Root()               { return StaticClass<UBehaviorTreeGraphNode_Root>(); }
	UClass* Composite()          { return StaticClass<UBehaviorTreeGraphNode_Composite>(); }
	UClass* SimpleParallel()     { return StaticClass<UBehaviorTreeGraphNode_SimpleParallel>(); }
	UClass* Task()               { return StaticClass<UBehaviorTreeGraphNode_Task>(); }
	UClass* SubtreeTask()        { return StaticClass<UBehaviorTreeGraphNode_SubtreeTask>(); }
	UClass* Decorator()          { return StaticClass<UBehaviorTreeGraphNode_Decorator>(); }
	UClass* Service()            { return StaticClass<UBehaviorTreeGraphNode_Service>(); }
	UClass* CompositeDecorator() { return StaticClass<UBehaviorTreeGraphNode_CompositeDecorator>(); }
}

namespace BtPinCategory
{
	const FName MultipleNodes(TEXT("MultipleNodes"));
	const FName SingleComposite(TEXT("SingleComposite"));
	const FName SingleTask(TEXT("SingleTask"));
	const FName SingleNode(TEXT("SingleNode"));
}

const TCHAR* BtNodeKindName(EBtNodeKind Kind)
{
	switch (Kind)
	{
	case EBtNodeKind::Composite: return TEXT("composite");
	case EBtNodeKind::Task:      return TEXT("task");
	case EBtNodeKind::Decorator: return TEXT("decorator");
	case EBtNodeKind::Service:   return TEXT("service");
	case EBtNodeKind::Root:      return TEXT("root");
	default:                     return TEXT("unknown");
	}
}

UClass* GraphNodeClassFor(const UClass* InstanceClass, EBtNodeKind& OutKind)
{
	OutKind = EBtNodeKind::Unknown;
	if (!InstanceClass)
	{
		return nullptr;
	}

	// Order matters: the specialised cases are subclasses of the general ones.
	if (InstanceClass->IsChildOf(UBTCompositeNode::StaticClass()))
	{
		OutKind = EBtNodeKind::Composite;
		return InstanceClass->IsChildOf(UBTComposite_SimpleParallel::StaticClass())
			? BtNodeClasses::SimpleParallel()
			: BtNodeClasses::Composite();
	}

	if (InstanceClass->IsChildOf(UBTTaskNode::StaticClass()))
	{
		OutKind = EBtNodeKind::Task;
		return InstanceClass->IsChildOf(UBTTask_RunBehavior::StaticClass())
			? BtNodeClasses::SubtreeTask()
			: BtNodeClasses::Task();
	}

	if (InstanceClass->IsChildOf(UBTDecorator::StaticClass()))
	{
		OutKind = EBtNodeKind::Decorator;
		return BtNodeClasses::Decorator();
	}

	if (InstanceClass->IsChildOf(UBTService::StaticClass()))
	{
		OutKind = EBtNodeKind::Service;
		return BtNodeClasses::Service();
	}

	return nullptr;
}

EBtNodeKind BtKindOfInstanceClass(const UClass* InstanceClass)
{
	EBtNodeKind Kind = EBtNodeKind::Unknown;
	GraphNodeClassFor(InstanceClass, Kind);
	return Kind;
}

static void StripBtClassPrefix(FString& Name)
{
	static const TCHAR* Prefixes[] = { TEXT("BTTask_"), TEXT("BTComposite_"), TEXT("BTDecorator_"), TEXT("BTService_") };
	for (const TCHAR* Prefix : Prefixes)
	{
		if (Name.RemoveFromStart(Prefix, ESearchCase::CaseSensitive))
		{
			return;
		}
	}
}

FString ShortBtTypeName(const UClass* InstanceClass)
{
	if (!InstanceClass)
	{
		return FString();
	}

	FString Name = InstanceClass->GetName();

	// Blueprint-generated classes keep their full name; "BTT_Foo_C" is what a user would type.
	if (Name.EndsWith(TEXT("_C"), ESearchCase::CaseSensitive))
	{
		return Name;
	}

	StripBtClassPrefix(Name);
	return Name.IsEmpty() ? InstanceClass->GetName() : Name;
}

TSharedPtr<FGraphNodeClassHelper> GetBtClassCache()
{
	FBehaviorTreeEditorModule& EditorModule =
		FModuleManager::LoadModuleChecked<FBehaviorTreeEditorModule>(TEXT("BehaviorTreeEditor"));
	if (TSharedPtr<FGraphNodeClassHelper> Existing = EditorModule.GetClassCache())
	{
		return Existing;
	}

	// Mirrors what CreateBehaviorTreeEditor() would have done on the first opened tree.
	// AddObservedBlueprintClasses writes into a static registry, so it only needs doing once.
	static bool bObservedClassesRegistered = false;
	if (!bObservedClassesRegistered)
	{
		FGraphNodeClassHelper::AddObservedBlueprintClasses(UBTTask_BlueprintBase::StaticClass());
		FGraphNodeClassHelper::AddObservedBlueprintClasses(UBTDecorator_BlueprintBase::StaticClass());
		FGraphNodeClassHelper::AddObservedBlueprintClasses(UBTService_BlueprintBase::StaticClass());
		bObservedClassesRegistered = true;
	}

	TSharedPtr<FGraphNodeClassHelper> Cache = MakeShareable(new FGraphNodeClassHelper(UBTNode::StaticClass()));
	Cache->UpdateAvailableBlueprintClasses();
	return Cache;
}

static UClass* BaseClassForKind(EBtNodeKind Kind)
{
	switch (Kind)
	{
	case EBtNodeKind::Composite: return UBTCompositeNode::StaticClass();
	case EBtNodeKind::Task:      return UBTTaskNode::StaticClass();
	case EBtNodeKind::Decorator: return UBTDecorator::StaticClass();
	case EBtNodeKind::Service:   return UBTService::StaticClass();
	default:                     return UBTNode::StaticClass();
	}
}

UClass* ResolveBtNodeClass(const FString& TypeName, EBtNodeKind ExpectedKind)
{
	const FString Wanted = TypeName.TrimStartAndEnd();
	if (Wanted.IsEmpty())
	{
		throw FCommandFailedException(TEXT("CLI_USAGE"), TEXT("--type is required."));
	}

	UClass* const BaseClass = BaseClassForKind(ExpectedKind);

	// The class cache is the only source that also sees Blueprint-derived nodes, so try it first.
	TArray<FString> Candidates;
	if (TSharedPtr<FGraphNodeClassHelper> Cache = GetBtClassCache())
	{
		TArray<FGraphNodeClassData> ClassData;
		Cache->GatherClasses(BaseClass, ClassData);
		for (FGraphNodeClassData& Entry : ClassData)
		{
			if (Entry.IsAbstract() || Entry.IsDeprecated())
			{
				continue;
			}

			FString ClassName = Entry.GetClassName();
			FString ShortName = ClassName;
			if (!ShortName.EndsWith(TEXT("_C"), ESearchCase::CaseSensitive))
			{
				StripBtClassPrefix(ShortName);
			}

			if (ClassName.Equals(Wanted, ESearchCase::IgnoreCase)
				|| ShortName.Equals(Wanted, ESearchCase::IgnoreCase))
			{
				// Only now does a Blueprint class have to be loaded.
				if (UClass* Resolved = Entry.GetClass(/*bSilent=*/true))
				{
					return Resolved;
				}
			}
			Candidates.Add(ShortName);
		}
	}

	// Native fallback, for anything the cache has not picked up yet.
	for (TObjectIterator<UClass> It; It; ++It)
	{
		UClass* Class = *It;
		if (!Class->IsChildOf(BaseClass) || Class == BaseClass)
		{
			continue;
		}
		if (Class->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists))
		{
			continue;
		}
		if (Class->GetName().Equals(Wanted, ESearchCase::IgnoreCase)
			|| ShortBtTypeName(Class).Equals(Wanted, ESearchCase::IgnoreCase))
		{
			return Class;
		}
	}

	Candidates.Sort();
	const int32 ShowCount = FMath::Min(Candidates.Num(), 15);
	const FString Shown = FString::Join(TArrayView<FString>(Candidates.GetData(), ShowCount), TEXT(", "));

	const bool bKnownKind = (ExpectedKind != EBtNodeKind::Unknown);
	const FString What = bKnownKind ? FString::Printf(TEXT("%s type"), BtNodeKindName(ExpectedKind)) : TEXT("node type");
	const FString ListHint = bKnownKind
		? FString::Printf(TEXT("`bt list-node-types --kind %s`"), BtNodeKindName(ExpectedKind))
		: TEXT("`bt list-node-types`");

	throw FCommandFailedException(TEXT("NOT_FOUND"),
		FString::Printf(TEXT("Unknown %s '%s'.%s%s%s Use %s for the full list."),
			*What, *Wanted,
			Shown.IsEmpty() ? TEXT("") : TEXT(" Candidates: "), *Shown,
			Shown.IsEmpty() ? TEXT("") : (Candidates.Num() > ShowCount ? TEXT(", ...") : TEXT(".")),
			*ListHint));
}

// ---------------------------------------------------------------------------------------------
// Asset and graph access
// ---------------------------------------------------------------------------------------------

UBehaviorTree* LoadBehaviorTreeOrThrow(const FString& Path)
{
	UBehaviorTree* BehaviorTree = LoadObject<UBehaviorTree>(nullptr, *HandlerUtils::NormalizeObjectPath(Path));
	if (!BehaviorTree)
	{
		throw FCommandFailedException(TEXT("NOT_FOUND"),
			FString::Printf(TEXT("Behavior tree not found: %s"), *Path));
	}
	return BehaviorTree;
}

UBehaviorTreeGraph* GetBtGraphForRead(UBehaviorTree* BehaviorTree)
{
	return BehaviorTree ? Cast<UBehaviorTreeGraph>(BehaviorTree->BTGraph) : nullptr;
}

UBehaviorTreeGraph* GetBtGraphForWrite(UBehaviorTree* BehaviorTree)
{
	if (!BehaviorTree)
	{
		throw FCommandFailedException(TEXT("INTERNAL_ERROR"), TEXT("No behavior tree to edit."));
	}

	UBehaviorTreeGraph* Graph = Cast<UBehaviorTreeGraph>(BehaviorTree->BTGraph);
	if (Graph)
	{
		Graph->OnLoaded();
		Graph->Initialize();
		return Graph;
	}

	if (BehaviorTree->RootNode != nullptr)
	{
		throw FCommandFailedException(TEXT("OPERATION_FAILED"),
			FString::Printf(TEXT("'%s' has a tree but no editor graph, so editing it here would erase the tree. ")
				TEXT("Open it once in the Behavior Tree editor, save, then run this command again."),
				*BehaviorTree->GetName()));
	}

	// Same sequence FBehaviorTreeEditor::RestoreBehaviorTree() runs for a brand new graph.
	BehaviorTree->BTGraph = FBlueprintEditorUtils::CreateNewGraph(
		BehaviorTree, TEXT("Behavior Tree"),
		UBehaviorTreeGraph::StaticClass(), UEdGraphSchema_BehaviorTree::StaticClass());
	Graph = Cast<UBehaviorTreeGraph>(BehaviorTree->BTGraph);
	if (!Graph)
	{
		throw FCommandFailedException(TEXT("OPERATION_FAILED"),
			FString::Printf(TEXT("Could not create an editor graph for '%s'."), *BehaviorTree->GetName()));
	}
	Graph->GetSchema()->CreateDefaultNodesForGraph(*Graph);
	Graph->OnCreated();
	Graph->Initialize();
	return Graph;
}

UAIGraphNode* FindBtRootNode(UBehaviorTreeGraph* Graph)
{
	if (!Graph)
	{
		return nullptr;
	}
	for (UEdGraphNode* Node : Graph->Nodes)
	{
		if (UAIGraphNode* Root = BtCast<UBehaviorTreeGraphNode_Root>(Node))
		{
			return Root;
		}
	}
	return nullptr;
}

FString GetBtNodeId(const UAIGraphNode* Node)
{
	if (!Node)
	{
		return FString();
	}
	if (const UBTNode* Instance = Cast<UBTNode>(Node->NodeInstance))
	{
		return Instance->NodeName.IsEmpty() ? Instance->GetName() : Instance->NodeName;
	}
	return Node->GetName();
}

void CollectAllBtNodes(UBehaviorTreeGraph* Graph, TArray<UAIGraphNode*>& OutNodes)
{
	OutNodes.Reset();
	if (!Graph)
	{
		return;
	}
	for (UEdGraphNode* EdNode : Graph->Nodes)
	{
		UAIGraphNode* Node = Cast<UAIGraphNode>(EdNode);
		if (!Node)
		{
			continue;
		}
		OutNodes.Add(Node);
		for (const TObjectPtr<UAIGraphNode>& SubNode : Node->SubNodes)
		{
			if (UAIGraphNode* Sub = SubNode.Get())
			{
				OutNodes.Add(Sub);
			}
		}
	}
}

void GetOrderedChildren(UAIGraphNode* Node, int32 OutputPinIndex, TArray<UAIGraphNode*>& OutChildren)
{
	OutChildren.Reset();
	if (!Node)
	{
		return;
	}

	UEdGraphPin* Pin = Node->GetOutputPin(OutputPinIndex);
	if (!Pin)
	{
		return;
	}

	TArray<UEdGraphPin*> Linked = Pin->LinkedTo;
	Linked.Sort(FCompareNodeXLocation());

	for (UEdGraphPin* LinkedPin : Linked)
	{
		if (!LinkedPin)
		{
			continue;
		}
		if (UAIGraphNode* Child = BtCast<UBehaviorTreeGraphNode>(LinkedPin->GetOwningNode()))
		{
			OutChildren.Add(Child);
		}
	}
}

static void WalkBtExecutionOrder(UAIGraphNode* Node, TArray<UAIGraphNode*>& OutNodes, TSet<UAIGraphNode*>& Visited, int32 Depth)
{
	if (!Node || Depth > 64 || Visited.Contains(Node))
	{
		return;
	}
	Visited.Add(Node);
	OutNodes.Add(Node);

	const int32 PinCount = (Node->GetOutputPin(1) != nullptr) ? 2 : 1;
	for (int32 PinIndex = 0; PinIndex < PinCount; ++PinIndex)
	{
		TArray<UAIGraphNode*> Children;
		GetOrderedChildren(Node, PinIndex, Children);
		for (UAIGraphNode* Child : Children)
		{
			WalkBtExecutionOrder(Child, OutNodes, Visited, Depth + 1);
		}
	}
}

void BuildBtExecutionOrder(UBehaviorTreeGraph* Graph, TArray<UAIGraphNode*>& OutNodes)
{
	OutNodes.Reset();
	UAIGraphNode* Root = FindBtRootNode(Graph);
	if (!Root)
	{
		return;
	}

	TSet<UAIGraphNode*> Visited;
	Visited.Add(Root);

	TArray<UAIGraphNode*> RootChildren;
	GetOrderedChildren(Root, 0, RootChildren);
	if (RootChildren.Num() > 0)
	{
		WalkBtExecutionOrder(RootChildren[0], OutNodes, Visited, 0);
	}
}

UAIGraphNode* FindBtNodeOrThrow(UBehaviorTreeGraph* Graph, const FString& Ref)
{
	const FString Wanted = Ref.TrimStartAndEnd();
	if (Wanted.IsEmpty())
	{
		throw FCommandFailedException(TEXT("CLI_USAGE"), TEXT("A node reference is required."));
	}

	if (Wanted.Equals(TEXT("Root"), ESearchCase::IgnoreCase))
	{
		if (UAIGraphNode* Root = FindBtRootNode(Graph))
		{
			return Root;
		}
		throw FCommandFailedException(TEXT("NOT_FOUND"), TEXT("This graph has no root node."));
	}

	if (Wanted.StartsWith(TEXT("#")))
	{
		TArray<UAIGraphNode*> Ordered;
		BuildBtExecutionOrder(Graph, Ordered);
		const int32 Index = FCString::Atoi(*Wanted.Mid(1));
		if (!Ordered.IsValidIndex(Index))
		{
			throw FCommandFailedException(TEXT("NOT_FOUND"),
				FString::Printf(TEXT("No node at execution index %d; the tree has %d nodes."), Index, Ordered.Num()));
		}
		return Ordered[Index];
	}

	TArray<UAIGraphNode*> AllNodes;
	CollectAllBtNodes(Graph, AllNodes);

	TArray<UAIGraphNode*> NameMatches;
	for (UAIGraphNode* Node : AllNodes)
	{
		if (GetBtNodeId(Node).Equals(Wanted, ESearchCase::IgnoreCase))
		{
			NameMatches.Add(Node);
		}
	}
	if (NameMatches.Num() == 1)
	{
		return NameMatches[0];
	}
	if (NameMatches.Num() > 1)
	{
		TArray<FString> Guids;
		for (UAIGraphNode* Node : NameMatches)
		{
			Guids.Add(Node->NodeGuid.ToString().Left(8));
		}
		throw FCommandFailedException(TEXT("CLI_USAGE"),
			FString::Printf(TEXT("'%s' matches %d nodes. Use an execution index (#N) or a guid prefix instead: %s"),
				*Wanted, NameMatches.Num(), *FString::Join(Guids, TEXT(", "))));
	}

	if (Wanted.Len() >= 8)
	{
		for (UAIGraphNode* Node : AllNodes)
		{
			if (Node->NodeGuid.ToString().StartsWith(Wanted, ESearchCase::IgnoreCase))
			{
				return Node;
			}
		}
	}

	for (UAIGraphNode* Node : AllNodes)
	{
		if (Node->GetName().Equals(Wanted, ESearchCase::IgnoreCase)
			|| (Node->NodeInstance && Node->NodeInstance->GetName().Equals(Wanted, ESearchCase::IgnoreCase)))
		{
			return Node;
		}
	}

	throw FCommandFailedException(TEXT("NOT_FOUND"),
		FString::Printf(TEXT("No node named '%s'. Run `bt inspect` to see the ids in this tree."), *Wanted));
}

// ---------------------------------------------------------------------------------------------
// Editing
// ---------------------------------------------------------------------------------------------

UAIGraphNode* SpawnBtNode(UBehaviorTreeGraph* Graph, UClass* InstanceClass, const FString& Id, FVector2D Location)
{
	EBtNodeKind Kind = EBtNodeKind::Unknown;
	UClass* GraphNodeClass = GraphNodeClassFor(InstanceClass, Kind);
	if (!GraphNodeClass)
	{
		throw FCommandFailedException(TEXT("OPERATION_FAILED"),
			FString::Printf(TEXT("'%s' is not a behavior tree node class."), *GetNameSafe(InstanceClass)));
	}

	// The template is the node: FAISchemaAction_NewNode::PerformAction renames it into the graph
	// rather than duplicating it, so the NO_API subclass survives. It also runs PostPlacedNewNode,
	// which is what creates NodeInstance from ClassData.
	UAIGraphNode* Template = NewObject<UAIGraphNode>(Graph, GraphNodeClass);
	if (UBehaviorTreeGraphNode* AsBtNode = BtCast<UBehaviorTreeGraphNode>(Template))
	{
		AsBtNode->ClassData = FGraphNodeClassData(InstanceClass, FString());
	}

	UAIGraphNode* Node = FAISchemaAction_NewNode::SpawnNodeFromTemplate<UAIGraphNode>(Graph, Template, Location);
	if (!Node)
	{
		throw FCommandFailedException(TEXT("OPERATION_FAILED"),
			FString::Printf(TEXT("Could not create a node of type '%s'."), *ShortBtTypeName(InstanceClass)));
	}

	if (!Id.IsEmpty())
	{
		if (UBTNode* Instance = Cast<UBTNode>(Node->NodeInstance))
		{
			Instance->NodeName = Id;
		}
	}
	return Node;
}

void ConnectBtNodes(UBehaviorTreeGraph* Graph, UAIGraphNode* Parent, UAIGraphNode* Child, int32 OutputPinIndex)
{
	UEdGraphPin* OutPin = Parent ? Parent->GetOutputPin(OutputPinIndex) : nullptr;
	UEdGraphPin* InPin = Child ? Child->GetInputPin(0) : nullptr;

	if (!OutPin)
	{
		throw FCommandFailedException(TEXT("OPERATION_FAILED"),
			FString::Printf(TEXT("'%s' has no output pin %d, so it cannot take children."),
				*GetBtNodeId(Parent), OutputPinIndex));
	}
	if (!InPin)
	{
		throw FCommandFailedException(TEXT("OPERATION_FAILED"),
			FString::Printf(TEXT("'%s' has no input pin; decorators and services attach with ")
				TEXT("`bt add-decorator` / `bt add-service` instead of `bt connect`."),
				*GetBtNodeId(Child)));
	}

	// The schema breaks any existing link into the child's input pin, so this is also a re-parent.
	if (!Graph->GetSchema()->TryCreateConnection(OutPin, InPin))
	{
		throw FCommandFailedException(TEXT("OPERATION_FAILED"),
			FString::Printf(TEXT("'%s' cannot be a child of '%s' (pin type %s vs %s). ")
				TEXT("A task cannot take children, and a composite pin only accepts what it is typed for."),
				*GetBtNodeId(Child), *GetBtNodeId(Parent),
				*InPin->PinType.PinCategory.ToString(), *OutPin->PinType.PinCategory.ToString()));
	}
	Child->NodeConnectionListChanged();
}

void SpaceChildrenEvenly(UAIGraphNode* Parent, const TArray<UAIGraphNode*>& OrderedChildren)
{
	// Execution order is read off NodePosX, so the positions are the ordering — see
	// FCompareNodeXLocation in CreateChildren().
	constexpr int32 ColStep = 300;
	const int32 Count = OrderedChildren.Num();
	if (Count == 0 || !Parent)
	{
		return;
	}

	const int32 StartX = Parent->NodePosX - ((Count - 1) * ColStep) / 2;
	for (int32 Index = 0; Index < Count; ++Index)
	{
		UAIGraphNode* Child = OrderedChildren[Index];
		if (!Child)
		{
			continue;
		}
		Child->Modify();
		Child->NodePosX = StartX + Index * ColStep;
		if (Child->NodePosY <= Parent->NodePosY)
		{
			Child->NodePosY = Parent->NodePosY + 160;
		}
	}
}

void ApplyBtValues(UBehaviorTree* BehaviorTree, UAIGraphNode* Node, const TSharedPtr<FJsonObject>& Values)
{
	UObject* Instance = Node ? Node->NodeInstance : nullptr;
	if (!Instance || !Values.IsValid() || Values->Values.Num() == 0)
	{
		return;
	}

	TArray<FString> SelectorProperties;

	// A blackboard key is usually written as a bare name ("TargetActor"), which ImportText would
	// reject because the property is a struct. Fill in SelectedKeyName directly and let
	// InitializeFromAsset resolve the rest below.
	//
	// bt inspect --with-values reports the same property as a struct literal instead, so a value
	// starting with '(' goes down the normal ImportText path and only joins the resolve pass —
	// that is what makes an inspect dump re-appliable.
	auto HandleBlackboardSelector =
		[&SelectorProperties](FProperty& Property, void* ValuePtr, const TSharedPtr<FJsonValue>& Value) -> bool
	{
		FStructProperty* StructProperty = CastField<FStructProperty>(&Property);
		if (!StructProperty || !StructProperty->Struct
			|| StructProperty->Struct->GetFName() != TEXT("BlackboardKeySelector")
			|| !Value.IsValid() || Value->Type != EJson::String)
		{
			return false;
		}

		SelectorProperties.Add(Property.GetName());

		const FString Text = Value->AsString().TrimStartAndEnd();
		if (Text.StartsWith(TEXT("(")))
		{
			return false;
		}

		FBlackboardKeySelector* Selector = static_cast<FBlackboardKeySelector*>(ValuePtr);
		Selector->SelectedKeyName = FName(*Text);
		Selector->InvalidateResolvedKey();
		return true;
	};

	HandlerUtils::ApplyValues(Instance, Values, HandlerUtils::FPropertyOverrideFn(HandleBlackboardSelector));

	if (SelectorProperties.Num() == 0)
	{
		return;
	}

	// Nodes that care about SelectedKeyID resolve it here — UBTDecorator_Blackboard and the other
	// UBTNode subclasses that override InitializeFromAsset to call ResolveSelectedKey.
	UBTNode* BtNode = Cast<UBTNode>(Instance);
	if (BtNode && BehaviorTree)
	{
		BtNode->InitializeFromAsset(*BehaviorTree);
	}

	// Do not check IsSet() here. Resolution is opt-in per node class: a node that reads its key
	// with GetValueAsObject(Selector.SelectedKeyName) never resolves an ID and would look broken.
	// What is worth catching is a name the blackboard does not have at all — that is a typo, and
	// it fails silently at runtime otherwise.
	UBlackboardData* Blackboard = BehaviorTree ? BehaviorTree->BlackboardAsset : nullptr;
	for (const FString& PropertyName : SelectorProperties)
	{
		FStructProperty* StructProperty = FindFProperty<FStructProperty>(Instance->GetClass(), *PropertyName);
		if (!StructProperty)
		{
			continue;
		}
		const FBlackboardKeySelector* Selector =
			StructProperty->ContainerPtrToValuePtr<FBlackboardKeySelector>(Instance);
		const FName KeyName = Selector->SelectedKeyName;
		if (KeyName.IsNone())
		{
			continue;
		}
		if (Blackboard && Blackboard->GetKeyID(KeyName) != FBlackboard::InvalidKey)
		{
			continue;
		}

		TArray<FString> KeyNames;
		for (UBlackboardData* Data = Blackboard; Data; Data = Data->Parent)
		{
			for (const FBlackboardEntry& Entry : Data->Keys)
			{
				KeyNames.Add(Entry.EntryName.ToString());
			}
		}
		throw FCommandFailedException(TEXT("NOT_FOUND"),
			FString::Printf(TEXT("Blackboard key '%s' for property '%s' is not in %s. Available keys: %s"),
				*KeyName.ToString(), *PropertyName,
				Blackboard ? *Blackboard->GetName() : TEXT("(no blackboard set on this tree)"),
				KeyNames.Num() > 0 ? *FString::Join(KeyNames, TEXT(", ")) : TEXT("(none)")));
	}
}

void SetBtBlackboard(UBehaviorTree* BehaviorTree, UBehaviorTreeGraph* Graph, UBlackboardData* Blackboard)
{
	if (!BehaviorTree)
	{
		return;
	}

	BehaviorTree->Modify();
	BehaviorTree->BlackboardAsset = Blackboard;

	// The root node carries its own copy, and it is the one the editor's details panel shows.
	if (UBehaviorTreeGraphNode_Root* Root = BtCast<UBehaviorTreeGraphNode_Root>(FindBtRootNode(Graph)))
	{
		Root->Modify();
		Root->BlackboardAsset = Blackboard;
	}

	if (Graph)
	{
		Graph->UpdateBlackboardChange();
	}
}

// ---------------------------------------------------------------------------------------------
// Layout
// ---------------------------------------------------------------------------------------------

namespace
{

constexpr int32 LayoutColStep = 300;
constexpr int32 LayoutRowBase = 140;
constexpr int32 LayoutSubNodeHeight = 34;
constexpr int32 LayoutGridSnap = 16;

int32 SubNodeCount(UAIGraphNode* Node)
{
	return Node ? Node->SubNodes.Num() : 0;
}

void MeasureRows(UAIGraphNode* Node, int32 Depth, TArray<int32>& InOutMaxSubNodes, TSet<UAIGraphNode*>& Visited)
{
	if (!Node || Visited.Contains(Node))
	{
		return;
	}
	Visited.Add(Node);

	while (InOutMaxSubNodes.Num() <= Depth)
	{
		InOutMaxSubNodes.Add(0);
	}
	InOutMaxSubNodes[Depth] = FMath::Max(InOutMaxSubNodes[Depth], SubNodeCount(Node));

	const int32 PinCount = (Node->GetOutputPin(1) != nullptr) ? 2 : 1;
	for (int32 PinIndex = 0; PinIndex < PinCount; ++PinIndex)
	{
		TArray<UAIGraphNode*> Children;
		GetOrderedChildren(Node, PinIndex, Children);
		for (UAIGraphNode* Child : Children)
		{
			MeasureRows(Child, Depth + 1, InOutMaxSubNodes, Visited);
		}
	}
}

int32 PlaceNode(UAIGraphNode* Node, int32 Depth, int32& InOutLeafCursor,
	const TArray<int32>& RowY, TSet<UAIGraphNode*>& Visited)
{
	if (!Node || Visited.Contains(Node))
	{
		return InOutLeafCursor;
	}
	Visited.Add(Node);

	Node->Modify();
	Node->NodePosY = RowY.IsValidIndex(Depth) ? RowY[Depth] : Depth * (LayoutRowBase + LayoutSubNodeHeight);

	TArray<UAIGraphNode*> AllChildren;
	const int32 PinCount = (Node->GetOutputPin(1) != nullptr) ? 2 : 1;
	for (int32 PinIndex = 0; PinIndex < PinCount; ++PinIndex)
	{
		TArray<UAIGraphNode*> Children;
		GetOrderedChildren(Node, PinIndex, Children);
		AllChildren.Append(Children);
	}

	if (AllChildren.Num() == 0)
	{
		Node->NodePosX = InOutLeafCursor;
		InOutLeafCursor += LayoutColStep;
		return Node->NodePosX;
	}

	int32 First = MAX_int32;
	int32 Last = MIN_int32;
	for (UAIGraphNode* Child : AllChildren)
	{
		if (Visited.Contains(Child))
		{
			continue;
		}
		const int32 ChildX = PlaceNode(Child, Depth + 1, InOutLeafCursor, RowY, Visited);
		First = FMath::Min(First, ChildX);
		Last = FMath::Max(Last, ChildX);
	}

	Node->NodePosX = (First == MAX_int32) ? InOutLeafCursor : (First + Last) / 2;
	return Node->NodePosX;
}

/**
 * Grid snapping can round two siblings onto the same X, and equal X makes their execution order
 * arbitrary (FCompareNodeXLocation falls back to Y, which layout also makes equal). Walk each
 * sibling list and push duplicates apart.
 */
void EnforceStrictChildOrder(UAIGraphNode* Node, TSet<UAIGraphNode*>& Visited, TArray<FString>& OutWarnings)
{
	if (!Node || Visited.Contains(Node))
	{
		return;
	}
	Visited.Add(Node);

	const int32 PinCount = (Node->GetOutputPin(1) != nullptr) ? 2 : 1;
	for (int32 PinIndex = 0; PinIndex < PinCount; ++PinIndex)
	{
		TArray<UAIGraphNode*> Children;
		GetOrderedChildren(Node, PinIndex, Children);
		for (int32 Index = 1; Index < Children.Num(); ++Index)
		{
			if (Children[Index]->NodePosX <= Children[Index - 1]->NodePosX)
			{
				Children[Index]->Modify();
				Children[Index]->NodePosX = Children[Index - 1]->NodePosX + LayoutGridSnap;
				OutWarnings.Add(FString::Printf(
					TEXT("Nudged '%s' to keep it ordered after '%s'; siblings sharing an X have no defined order."),
					*GetBtNodeId(Children[Index]), *GetBtNodeId(Children[Index - 1])));
			}
		}
		for (UAIGraphNode* Child : Children)
		{
			EnforceStrictChildOrder(Child, Visited, OutWarnings);
		}
	}
}

} // namespace

void LayoutBtGraph(UBehaviorTreeGraph* Graph, TArray<FString>& OutWarnings)
{
	UAIGraphNode* Root = FindBtRootNode(Graph);
	if (!Root)
	{
		return;
	}

	// Pass 1: how tall each row has to be, given the tallest stack of subnodes in it.
	TArray<int32> MaxSubNodes;
	{
		TSet<UAIGraphNode*> Visited;
		MeasureRows(Root, 0, MaxSubNodes, Visited);
	}

	TArray<int32> RowY;
	RowY.Reserve(MaxSubNodes.Num());
	int32 Y = 0;
	for (int32 Depth = 0; Depth < MaxSubNodes.Num(); ++Depth)
	{
		RowY.Add(Y);
		Y += LayoutRowBase + LayoutSubNodeHeight * MaxSubNodes[Depth];
	}

	// Pass 2: leaves take successive columns, parents centre over their children.
	{
		int32 LeafCursor = 0;
		TSet<UAIGraphNode*> Visited;
		PlaceNode(Root, 0, LeafCursor, RowY, Visited);
	}

	// Snap to the grid the editor uses, then repair any ordering the snapping broke.
	for (UEdGraphNode* EdNode : Graph->Nodes)
	{
		if (UAIGraphNode* Node = Cast<UAIGraphNode>(EdNode))
		{
			Node->NodePosX = (Node->NodePosX / LayoutGridSnap) * LayoutGridSnap;
			Node->NodePosY = (Node->NodePosY / LayoutGridSnap) * LayoutGridSnap;
		}
	}
	{
		TSet<UAIGraphNode*> Visited;
		EnforceStrictChildOrder(Root, Visited, OutWarnings);
	}
}

// ---------------------------------------------------------------------------------------------
// Editor windows, finishing, warnings
// ---------------------------------------------------------------------------------------------

bool CloseBtEditors(UBehaviorTree* BehaviorTree)
{
	UAssetEditorSubsystem* AssetEditors = GEditor ? GEditor->GetEditorSubsystem<UAssetEditorSubsystem>() : nullptr;
	if (!AssetEditors || !BehaviorTree)
	{
		return false;
	}
	if (AssetEditors->FindEditorsForAsset(BehaviorTree).Num() == 0)
	{
		return false;
	}
	// An open editor holds Slate widgets pointing at the graph nodes; deleting one underneath it
	// crashes, and a Ctrl+S there would overwrite whatever we do here.
	AssetEditors->CloseAllEditorsForAsset(BehaviorTree);
	return true;
}

void ReopenBtEditors(UBehaviorTree* BehaviorTree)
{
	UAssetEditorSubsystem* AssetEditors = GEditor ? GEditor->GetEditorSubsystem<UAssetEditorSubsystem>() : nullptr;
	if (AssetEditors && BehaviorTree)
	{
		AssetEditors->OpenEditorForAsset(BehaviorTree);
	}
}

void FinishBtEdit(UBehaviorTree* BehaviorTree, UBehaviorTreeGraph* Graph,
	const TSharedPtr<FJsonObject>& Args, const TSharedPtr<FJsonObject>& OutData,
	TArray<FString>& OutWarnings)
{
	bool bNoUpdate = false;
	bool bLayout = false;
	bool bSave = false;
	if (Args.IsValid())
	{
		Args->TryGetBoolField(TEXT("noUpdate"), bNoUpdate);
		Args->TryGetBoolField(TEXT("layout"), bLayout);
		Args->TryGetBoolField(TEXT("save"), bSave);
	}

	if (bLayout)
	{
		LayoutBtGraph(Graph, OutWarnings);
	}

	if (!bNoUpdate)
	{
		// OnSave() is what the editor's Save runs: it fills in the nodes a simple parallel needs
		// and then recompiles the runtime tree. Nothing calls it for us — UBehaviorTree has no
		// PreSave hook — so skipping it would leave RootNode stale on disk.
		Graph->OnSave();
	}
	Graph->NotifyGraphChanged();
	Graph->Modify();
	BehaviorTree->Modify();
	BehaviorTree->MarkPackageDirty();

	OutData->SetBoolField(TEXT("recompiled"), !bNoUpdate);
	if (bSave)
	{
		OutData->SetBoolField(TEXT("saved"), HandlerUtils::SaveAssetPackage(BehaviorTree));
	}
}

void WriteBtWarnings(const TArray<FString>& Warnings, const TSharedPtr<FJsonObject>& OutData)
{
	TArray<TSharedPtr<FJsonValue>> Values;
	for (const FString& Warning : Warnings)
	{
		Values.Add(MakeShared<FJsonValueString>(Warning));
	}
	OutData->SetArrayField(TEXT("warnings"), Values);
}

FBtGraphUpdateGuard::FBtGraphUpdateGuard(UAIGraph* InGraph)
	: Graph(InGraph)
{
	if (InGraph)
	{
		InGraph->LockUpdates();
	}
}

FBtGraphUpdateGuard::~FBtGraphUpdateGuard()
{
	UAIGraph* Locked = Graph.Get();
	if (!Locked)
	{
		return;
	}

	// UnlockUpdates() runs UpdateAsset(), which recompiles the runtime tree and can touch a lot of
	// code. If it throws while we are already unwinding from a FCommandFailedException, the process
	// calls std::terminate. Swallowing here costs a stale tree; not swallowing costs the editor.
	try
	{
		Locked->UnlockUpdates();
	}
	catch (...)
	{
		UE_LOG(LogUnrealCliBridgeBt, Error,
			TEXT("UnlockUpdates() threw while unwinding; the behavior tree may be left stale."));
	}
}
