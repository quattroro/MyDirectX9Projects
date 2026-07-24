#include "AnimCommandHandler.h"
#include "Animation/AnimBlueprint.h"
#include "Animation/AnimBlueprintGeneratedClass.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimInstance.h"
#include "Animation/Skeleton.h"
#include "Animation/AnimSequence.h"
#include "Factories/AnimBlueprintFactory.h"
#include "Components/SkeletalMeshComponent.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "EdGraphSchema_K2.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "K2Node_VariableGet.h"
#include "K2Node_CallFunction.h"
#include "Kismet/KismetMathLibrary.h"
#include "AnimationGraph.h"
#include "AnimationGraphSchema.h"
#include "AnimationStateMachineGraph.h"
#include "AnimationStateMachineSchema.h"
#include "AnimationStateGraph.h"
#include "AnimGraphNode_StateMachine.h"
#include "AnimGraphNode_SequencePlayer.h"
#include "AnimGraphNode_StateResult.h"
#include "AnimGraphNode_Root.h"
#include "AnimGraphNode_TransitionResult.h"
#include "AnimStateNode.h"
#include "AnimStateTransitionNode.h"
#include "AnimStateEntryNode.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "Misc/PackageName.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"

DEFINE_LOG_CATEGORY_STATIC(LogUnrealCliBridgeAnim, Log, All);

static IAssetTools& GetAnimAssetTools()
{
	return FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
}

void FAnimCommandHandler::RegisterAll(FCommandDispatcher& Dispatcher)
{
	Dispatcher.Register(TEXT("anim.create-abp"),         &HandleCreateAbp);
	Dispatcher.Register(TEXT("anim.assign-abp"),         &HandleAssignAbp);
	Dispatcher.Register(TEXT("anim.list-states"),        &HandleListStates);
	Dispatcher.Register(TEXT("anim.add-variable"),       &HandleAddVariable);
	Dispatcher.Register(TEXT("anim.play-montage"),       &HandlePlayMontage);
	Dispatcher.Register(TEXT("anim.setup-statemachine"), &HandleSetupStateMachine);
}

TSharedPtr<FJsonObject> FAnimCommandHandler::HandleCreateAbp(const TSharedPtr<FJsonObject>& Args, bool bForce)
{
	FString SkeletonPath, AssetPath;
	if (!Args.IsValid()
		|| !Args->TryGetStringField(TEXT("skeleton"), SkeletonPath)
		|| !Args->TryGetStringField(TEXT("path"), AssetPath))
		throw FCommandFailedException(TEXT("CLI_USAGE"), TEXT("--skeleton and --path are required."));

	USkeleton* Skeleton = LoadObject<USkeleton>(nullptr, *SkeletonPath);
	if (!Skeleton)
		throw FCommandFailedException(TEXT("NOT_FOUND"),
			FString::Printf(TEXT("Skeleton not found: %s"), *SkeletonPath));

	FString PackageName = FPackageName::ObjectPathToPackageName(AssetPath);
	FString AssetName   = FPackageName::GetLongPackageAssetName(PackageName);
	FString PackagePath = FPackageName::GetLongPackagePath(PackageName);

	UAnimBlueprintFactory* Factory = NewObject<UAnimBlueprintFactory>();
	Factory->TargetSkeleton = Skeleton;
	Factory->BlueprintType  = EBlueprintType::BPTYPE_Normal;

	UObject* NewAsset = GetAnimAssetTools().CreateAsset(
		AssetName, PackagePath, UAnimBlueprint::StaticClass(), Factory);
	if (!NewAsset)
		throw FCommandFailedException(TEXT("OPERATION_FAILED"),
			TEXT("Failed to create Animation Blueprint."));

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("path"),     AssetPath);
	Data->SetStringField(TEXT("skeleton"), SkeletonPath);
	Data->SetStringField(TEXT("message"),  TEXT("Animation Blueprint created."));
	return Data;
}

TSharedPtr<FJsonObject> FAnimCommandHandler::HandleAssignAbp(const TSharedPtr<FJsonObject>& Args, bool bForce)
{
	FString BpPath, AnimBpPath, ComponentName;
	if (!Args.IsValid()
		|| !Args->TryGetStringField(TEXT("bp"),     BpPath)
		|| !Args->TryGetStringField(TEXT("animBp"), AnimBpPath))
		throw FCommandFailedException(TEXT("CLI_USAGE"), TEXT("--bp and --anim-bp are required."));
	Args->TryGetStringField(TEXT("component"), ComponentName);

	UBlueprint* BP = LoadObject<UBlueprint>(nullptr, *BpPath);
	if (!BP)
		throw FCommandFailedException(TEXT("NOT_FOUND"),
			FString::Printf(TEXT("Blueprint not found: %s"), *BpPath));

	UAnimBlueprint* AnimBP = LoadObject<UAnimBlueprint>(nullptr, *AnimBpPath);
	if (!AnimBP)
		throw FCommandFailedException(TEXT("NOT_FOUND"),
			FString::Printf(TEXT("Animation Blueprint not found: %s"), *AnimBpPath));

	USkeletalMeshComponent* MeshComp = nullptr;
	for (UActorComponent* Template : BP->ComponentTemplates)
	{
		USkeletalMeshComponent* Candidate = Cast<USkeletalMeshComponent>(Template);
		if (!Candidate) continue;
		if (ComponentName.IsEmpty()
			|| Candidate->GetName().Contains(ComponentName, ESearchCase::IgnoreCase))
		{
			MeshComp = Candidate;
			break;
		}
	}

	if (!MeshComp)
		throw FCommandFailedException(TEXT("NOT_FOUND"),
			TEXT("No SkeletalMeshComponent found in the Blueprint. "
			     "Use --component to specify one by name."));

	MeshComp->AnimClass = AnimBP->GeneratedClass;
	BP->MarkPackageDirty();
	FKismetEditorUtilities::CompileBlueprint(BP);

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("bp"),        BpPath);
	Data->SetStringField(TEXT("animBp"),    AnimBpPath);
	Data->SetStringField(TEXT("component"), MeshComp->GetName());
	Data->SetStringField(TEXT("message"),   TEXT("Animation Blueprint assigned and Blueprint compiled."));
	return Data;
}

TSharedPtr<FJsonObject> FAnimCommandHandler::HandleListStates(const TSharedPtr<FJsonObject>& Args, bool bForce)
{
	FString Path;
	if (!Args.IsValid() || !Args->TryGetStringField(TEXT("path"), Path) || Path.IsEmpty())
		throw FCommandFailedException(TEXT("CLI_USAGE"), TEXT("--path is required."));

	UAnimBlueprint* AnimBP = LoadObject<UAnimBlueprint>(nullptr, *Path);
	if (!AnimBP)
		throw FCommandFailedException(TEXT("NOT_FOUND"),
			FString::Printf(TEXT("Animation Blueprint not found: %s"), *Path));

	TArray<TSharedPtr<FJsonValue>> StateMachines;
	if (UAnimBlueprintGeneratedClass* AnimClass =
		Cast<UAnimBlueprintGeneratedClass>(AnimBP->GeneratedClass))
	{
		for (const FBakedAnimationStateMachine& SM : AnimClass->BakedStateMachines)
		{
			TSharedPtr<FJsonObject> SMObj = MakeShared<FJsonObject>();
			SMObj->SetStringField(TEXT("name"), SM.MachineName.ToString());

			TArray<TSharedPtr<FJsonValue>> States;
			for (const FBakedAnimationState& State : SM.States)
				States.Add(MakeShared<FJsonValueString>(State.StateName.ToString()));
			SMObj->SetArrayField(TEXT("states"), States);
			StateMachines.Add(MakeShared<FJsonValueObject>(SMObj));
		}
	}

	TArray<TSharedPtr<FJsonValue>> Variables;
	for (const FBPVariableDescription& Var : AnimBP->NewVariables)
	{
		TSharedPtr<FJsonObject> VarObj = MakeShared<FJsonObject>();
		VarObj->SetStringField(TEXT("name"), Var.VarName.ToString());
		VarObj->SetStringField(TEXT("type"), Var.VarType.PinCategory.ToString());
		Variables.Add(MakeShared<FJsonValueObject>(VarObj));
	}

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("path"),          Path);
	Data->SetArrayField(TEXT("stateMachines"),  StateMachines);
	Data->SetArrayField(TEXT("variables"),      Variables);
	return Data;
}

TSharedPtr<FJsonObject> FAnimCommandHandler::HandleAddVariable(const TSharedPtr<FJsonObject>& Args, bool bForce)
{
	FString Path, VarName, TypeStr;
	if (!Args.IsValid()
		|| !Args->TryGetStringField(TEXT("path"), Path)
		|| !Args->TryGetStringField(TEXT("name"), VarName)
		|| !Args->TryGetStringField(TEXT("type"), TypeStr))
		throw FCommandFailedException(TEXT("CLI_USAGE"),
			TEXT("--path, --name, and --type are required."));

	UAnimBlueprint* AnimBP = LoadObject<UAnimBlueprint>(nullptr, *Path);
	if (!AnimBP)
		throw FCommandFailedException(TEXT("NOT_FOUND"),
			FString::Printf(TEXT("Animation Blueprint not found: %s"), *Path));

	FEdGraphPinType PinType;
	TypeStr = TypeStr.ToLower();
	if (TypeStr == TEXT("float"))
	{
		PinType.PinCategory    = UEdGraphSchema_K2::PC_Real;
		PinType.PinSubCategory = UEdGraphSchema_K2::PC_Float;
	}
	else if (TypeStr == TEXT("bool"))
	{
		PinType.PinCategory = UEdGraphSchema_K2::PC_Boolean;
	}
	else if (TypeStr == TEXT("int"))
	{
		PinType.PinCategory = UEdGraphSchema_K2::PC_Int;
	}
	else
	{
		throw FCommandFailedException(TEXT("CLI_USAGE"),
			FString::Printf(TEXT("Unknown type '%s'. Supported: float, bool, int"), *TypeStr));
	}

	bool bAdded = FBlueprintEditorUtils::AddMemberVariable(AnimBP, FName(*VarName), PinType);
	if (!bAdded)
		throw FCommandFailedException(TEXT("OPERATION_FAILED"),
			FString::Printf(TEXT("Failed to add variable '%s'. It may already exist."), *VarName));

	AnimBP->MarkPackageDirty();
	FKismetEditorUtilities::CompileBlueprint(AnimBP);

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("path"),    Path);
	Data->SetStringField(TEXT("name"),    VarName);
	Data->SetStringField(TEXT("type"),    TypeStr);
	Data->SetStringField(TEXT("message"), TEXT("Variable added and Animation Blueprint compiled."));
	return Data;
}

TSharedPtr<FJsonObject> FAnimCommandHandler::HandlePlayMontage(const TSharedPtr<FJsonObject>& Args, bool bForce)
{
	FString ActorLabel, MontagePath;
	double PlayRate = 1.0;
	if (!Args.IsValid()
		|| !Args->TryGetStringField(TEXT("actor"),   ActorLabel)
		|| !Args->TryGetStringField(TEXT("montage"), MontagePath))
		throw FCommandFailedException(TEXT("CLI_USAGE"),
			TEXT("--actor and --montage are required."));
	Args->TryGetNumberField(TEXT("rate"), PlayRate);

	UWorld* PIEWorld = nullptr;
	for (const FWorldContext& Context : GEngine->GetWorldContexts())
	{
		if (Context.WorldType == EWorldType::PIE)
		{
			PIEWorld = Context.World();
			break;
		}
	}
	if (!PIEWorld)
		throw FCommandFailedException(TEXT("PLAYMODE_NOT_ACTIVE"),
			TEXT("No active PIE session. Start Play-In-Editor first."));

	AActor* TargetActor = nullptr;
	for (TActorIterator<AActor> It(PIEWorld); It; ++It)
	{
		if (It->GetActorLabel() == ActorLabel)
		{
			TargetActor = *It;
			break;
		}
	}
	if (!TargetActor)
		throw FCommandFailedException(TEXT("NOT_FOUND"),
			FString::Printf(TEXT("Actor not found in PIE: %s"), *ActorLabel));

	USkeletalMeshComponent* MeshComp =
		TargetActor->FindComponentByClass<USkeletalMeshComponent>();
	if (!MeshComp)
		throw FCommandFailedException(TEXT("NOT_FOUND"),
			TEXT("Actor has no SkeletalMeshComponent."));

	UAnimInstance* AnimInst = MeshComp->GetAnimInstance();
	if (!AnimInst)
		throw FCommandFailedException(TEXT("OPERATION_FAILED"),
			TEXT("No AnimInstance on the SkeletalMeshComponent."));

	UAnimMontage* Montage = LoadObject<UAnimMontage>(nullptr, *MontagePath);
	if (!Montage)
		throw FCommandFailedException(TEXT("NOT_FOUND"),
			FString::Printf(TEXT("Montage not found: %s"), *MontagePath));

	float Duration = AnimInst->Montage_Play(Montage, static_cast<float>(PlayRate));

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("actor"),    ActorLabel);
	Data->SetStringField(TEXT("montage"),  MontagePath);
	Data->SetNumberField(TEXT("duration"), Duration);
	Data->SetNumberField(TEXT("rate"),     PlayRate);
	Data->SetStringField(TEXT("message"),  TEXT("Montage playing."));
	return Data;
}

// Helper: wire a VariableGet->Greater->TransitionResult chain in a K2 transition graph
static void SetupSpeedTransitionCondition(UEdGraph* TransGraph, UBlueprint* Blueprint, float Threshold, bool bGreater)
{
	if (!TransGraph || !Blueprint) return;

	// Find the TransitionResult node
	UAnimGraphNode_TransitionResult* ResultNode = nullptr;
	for (UEdGraphNode* Node : TransGraph->Nodes)
	{
		ResultNode = Cast<UAnimGraphNode_TransitionResult>(Node);
		if (ResultNode) break;
	}
	if (!ResultNode) return;

	// Variable Get for "Speed"
	UK2Node_VariableGet* VarGet = NewObject<UK2Node_VariableGet>(TransGraph);
	TransGraph->AddNode(VarGet, false, false);
	VarGet->CreateNewGuid();
	VarGet->PostPlacedNewNode();
	VarGet->VariableReference.SetSelfMember(FName(TEXT("Speed")));
	VarGet->AllocateDefaultPins();
	VarGet->NodePosX = ResultNode->NodePosX - 400;
	VarGet->NodePosY = ResultNode->NodePosY;

	// Greater_FloatFloat (or Less)
	UK2Node_CallFunction* CmpNode = NewObject<UK2Node_CallFunction>(TransGraph);
	TransGraph->AddNode(CmpNode, false, false);
	CmpNode->CreateNewGuid();
	CmpNode->PostPlacedNewNode();
	FName FuncName = bGreater ? FName(TEXT("GreaterEqual_FloatFloat")) : FName(TEXT("Less_FloatFloat"));
	CmpNode->FunctionReference.SetExternalMember(FuncName, UKismetMathLibrary::StaticClass());
	CmpNode->AllocateDefaultPins();
	CmpNode->NodePosX = ResultNode->NodePosX - 200;
	CmpNode->NodePosY = ResultNode->NodePosY;

	// Connect Speed → A
	UEdGraphPin* SpeedOut = VarGet->FindPin(FName(TEXT("Speed")), EGPD_Output);
	UEdGraphPin* APin     = CmpNode->FindPin(FName(TEXT("A")),     EGPD_Input);
	if (SpeedOut && APin) SpeedOut->MakeLinkTo(APin);

	// Set B default = Threshold
	UEdGraphPin* BPin = CmpNode->FindPin(FName(TEXT("B")), EGPD_Input);
	if (BPin) BPin->DefaultValue = FString::Printf(TEXT("%g"), Threshold);

	// Connect ReturnValue → bCanEnterTransition
	UEdGraphPin* RetVal   = CmpNode->FindPin(FName(TEXT("ReturnValue")),        EGPD_Output);
	UEdGraphPin* CondPin  = ResultNode->FindPin(FName(TEXT("bCanEnterTransition")), EGPD_Input);
	if (RetVal && CondPin) RetVal->MakeLinkTo(CondPin);
}

TSharedPtr<FJsonObject> FAnimCommandHandler::HandleSetupStateMachine(const TSharedPtr<FJsonObject>& Args, bool bForce)
{
	// --- Parse arguments ---
	FString AnimBpPath, IdleAnimPath, WalkAnimPath;
	double  WalkThreshold = 10.0;
	if (!Args.IsValid()
		|| !Args->TryGetStringField(TEXT("path"),      AnimBpPath)
		|| !Args->TryGetStringField(TEXT("idleAnim"),  IdleAnimPath)
		|| !Args->TryGetStringField(TEXT("walkAnim"),  WalkAnimPath))
		throw FCommandFailedException(TEXT("CLI_USAGE"),
			TEXT("--path, --idle-anim, and --walk-anim are required."));
	Args->TryGetNumberField(TEXT("walkThreshold"), WalkThreshold);

	// --- Load assets ---
	UAnimBlueprint* AnimBP = LoadObject<UAnimBlueprint>(nullptr, *AnimBpPath);
	if (!AnimBP)
		throw FCommandFailedException(TEXT("NOT_FOUND"),
			FString::Printf(TEXT("Animation Blueprint not found: %s"), *AnimBpPath));

	UAnimSequence* IdleAnim = LoadObject<UAnimSequence>(nullptr, *IdleAnimPath);
	if (!IdleAnim)
		throw FCommandFailedException(TEXT("NOT_FOUND"),
			FString::Printf(TEXT("Idle animation not found: %s"), *IdleAnimPath));

	UAnimSequence* WalkAnim = LoadObject<UAnimSequence>(nullptr, *WalkAnimPath);
	if (!WalkAnim)
		throw FCommandFailedException(TEXT("NOT_FOUND"),
			FString::Printf(TEXT("Walk animation not found: %s"), *WalkAnimPath));

	// --- Ensure Speed variable exists ---
	bool bHasSpeed = false;
	for (const FBPVariableDescription& Var : AnimBP->NewVariables)
		if (Var.VarName == FName(TEXT("Speed"))) { bHasSpeed = true; break; }

	if (!bHasSpeed)
	{
		FEdGraphPinType FloatPin;
		FloatPin.PinCategory    = UEdGraphSchema_K2::PC_Real;
		FloatPin.PinSubCategory = UEdGraphSchema_K2::PC_Float;
		FBlueprintEditorUtils::AddMemberVariable(AnimBP, FName(TEXT("Speed")), FloatPin);
	}

	// --- Find the UAnimationGraph ---
	UAnimationGraph* AnimGraph = nullptr;
	TArray<UEdGraph*> AllGraphs;
	AnimBP->GetAllGraphs(AllGraphs);
	for (UEdGraph* G : AllGraphs)
	{
		if (UAnimationGraph* AG = Cast<UAnimationGraph>(G)) { AnimGraph = AG; break; }
	}
	if (!AnimGraph)
		throw FCommandFailedException(TEXT("OPERATION_FAILED"),
			TEXT("Could not find AnimationGraph in the Animation Blueprint."));

	// --- Guard: don't create a second state machine if already present ---
	TArray<UAnimGraphNode_Base*> ExistingSMs;
	AnimGraph->GetGraphNodesOfClass(UAnimGraphNode_StateMachine::StaticClass(), ExistingSMs);
	if (ExistingSMs.Num() > 0 && !bForce)
		throw FCommandFailedException(TEXT("ALREADY_EXISTS"),
			TEXT("State machine already exists. Use --force to recreate."));

	AnimBP->Modify();
	AnimGraph->Modify();

	// --- Create StateMachine node in AnimGraph ---
	FGraphNodeCreator<UAnimGraphNode_StateMachine> SMCreator(*AnimGraph);
	UAnimGraphNode_StateMachine* SMNode = SMCreator.CreateNode();
	SMCreator.Finalize();  // Finalize() already calls PostPlacedNewNode() which creates EditorStateMachineGraph

	UAnimationStateMachineGraph* SMGraph = SMNode->EditorStateMachineGraph;
	if (!SMGraph)
		throw FCommandFailedException(TEXT("OPERATION_FAILED"),
			TEXT("Failed to create state machine graph."));
	SMGraph->Modify();

	// --- Create Idle state ---
	UAnimStateNode* IdleState = FEdGraphSchemaAction_NewStateNode::SpawnNodeFromTemplate<UAnimStateNode>(
		SMGraph, NewObject<UAnimStateNode>(), FVector2D(200.f, 0.f), false);
	IdleState->Modify();
	// BoundGraph is auto-created by PostPlacedNewNode inside SpawnNodeFromTemplate
	{
		UAnimationStateGraph* IdleGraph = Cast<UAnimationStateGraph>(IdleState->BoundGraph);
		if (IdleGraph)
		{
			IdleGraph->Modify();
			// Find StateResult (auto-created by schema)
			UAnimGraphNode_StateResult* StateResult = nullptr;
			for (UEdGraphNode* Node : IdleGraph->Nodes)
			{
				StateResult = Cast<UAnimGraphNode_StateResult>(Node);
				if (StateResult) break;
			}
			// Create SequencePlayer for Idle
			FGraphNodeCreator<UAnimGraphNode_SequencePlayer> PlayerCreator(*IdleGraph);
			UAnimGraphNode_SequencePlayer* IdlePlayer = PlayerCreator.CreateNode();
			PlayerCreator.Finalize();
			IdlePlayer->SetAnimationAsset(IdleAnim);
			// Connect player output → StateResult input
			if (StateResult)
			{
				UEdGraphPin* PlayerOut = IdlePlayer->FindPin(FName(TEXT("Pose")), EGPD_Output);
				if (!PlayerOut && IdlePlayer->Pins.Num() > 0) PlayerOut = IdlePlayer->Pins[0];
				UEdGraphPin* ResultIn  = StateResult->FindPin(FName(TEXT("Pose")), EGPD_Input);
				if (!ResultIn && StateResult->Pins.Num() > 0) ResultIn = StateResult->Pins[0];
				if (PlayerOut && ResultIn) PlayerOut->MakeLinkTo(ResultIn);
			}
		}
		// Rename state via raw UObject rename to avoid TSharedPtr validator path
		if (IdleState->BoundGraph)
			IdleState->BoundGraph->Rename(TEXT("Idle"), nullptr, REN_DontCreateRedirectors);
	}

	// --- Create Walk state ---
	UAnimStateNode* WalkState = FEdGraphSchemaAction_NewStateNode::SpawnNodeFromTemplate<UAnimStateNode>(
		SMGraph, NewObject<UAnimStateNode>(), FVector2D(400.f, 150.f), false);
	WalkState->Modify();
	{
		UAnimationStateGraph* WalkGraph = Cast<UAnimationStateGraph>(WalkState->BoundGraph);
		if (WalkGraph)
		{
			WalkGraph->Modify();
			UAnimGraphNode_StateResult* StateResult = nullptr;
			for (UEdGraphNode* Node : WalkGraph->Nodes)
			{
				StateResult = Cast<UAnimGraphNode_StateResult>(Node);
				if (StateResult) break;
			}
			FGraphNodeCreator<UAnimGraphNode_SequencePlayer> PlayerCreator(*WalkGraph);
			UAnimGraphNode_SequencePlayer* WalkPlayer = PlayerCreator.CreateNode();
			PlayerCreator.Finalize();
			WalkPlayer->SetAnimationAsset(WalkAnim);
			if (StateResult)
			{
				UEdGraphPin* PlayerOut = WalkPlayer->FindPin(FName(TEXT("Pose")), EGPD_Output);
				if (!PlayerOut && WalkPlayer->Pins.Num() > 0) PlayerOut = WalkPlayer->Pins[0];
				UEdGraphPin* ResultIn  = StateResult->FindPin(FName(TEXT("Pose")), EGPD_Input);
				if (!ResultIn && StateResult->Pins.Num() > 0) ResultIn = StateResult->Pins[0];
				if (PlayerOut && ResultIn) PlayerOut->MakeLinkTo(ResultIn);
			}
		}
		if (WalkState->BoundGraph)
			WalkState->BoundGraph->Rename(TEXT("Walk"), nullptr, REN_DontCreateRedirectors);
	}

	// --- Wire Entry → Idle ---
	{
		UAnimStateEntryNode* Entry = SMGraph->EntryNode;
		if (Entry)
		{
			// AnimStateEntryNode uses Pins[0] as its single output pin
			UEdGraphPin* EntryOut = (Entry->Pins.Num() > 0) ? Entry->Pins[0] : nullptr;
			UEdGraphPin* IdleIn   = IdleState->GetInputPin();
			if (EntryOut && IdleIn)
				SMGraph->GetSchema()->TryCreateConnection(EntryOut, IdleIn);
		}
	}

	// --- Idle → Walk transition (Speed >= threshold) ---
	{
		UAnimStateTransitionNode* ToWalk =
			FEdGraphSchemaAction_NewStateNode::SpawnNodeFromTemplate<UAnimStateTransitionNode>(
				SMGraph, NewObject<UAnimStateTransitionNode>(), FVector2D(0.f, 0.f), false);
		ToWalk->CreateConnections(IdleState, WalkState);
		if (ToWalk->BoundGraph)
			SetupSpeedTransitionCondition(ToWalk->BoundGraph, AnimBP,
				static_cast<float>(WalkThreshold), /*bGreater=*/true);
	}

	// --- Walk → Idle transition (Speed < threshold) ---
	{
		UAnimStateTransitionNode* ToIdle =
			FEdGraphSchemaAction_NewStateNode::SpawnNodeFromTemplate<UAnimStateTransitionNode>(
				SMGraph, NewObject<UAnimStateTransitionNode>(), FVector2D(0.f, 0.f), false);
		ToIdle->CreateConnections(WalkState, IdleState);
		if (ToIdle->BoundGraph)
			SetupSpeedTransitionCondition(ToIdle->BoundGraph, AnimBP,
				static_cast<float>(WalkThreshold), /*bGreater=*/false);
	}

	// --- Connect SM output → AnimGraph Root ---
	{
		TArray<UAnimGraphNode_Base*> RootNodes;
		AnimGraph->GetGraphNodesOfClass(UAnimGraphNode_Root::StaticClass(), RootNodes);
		if (RootNodes.Num() > 0)
		{
			UAnimGraphNode_Root* RootNode = Cast<UAnimGraphNode_Root>(RootNodes[0]);
			// SM output is typically the first output pin; Root input is the first input pin
			UEdGraphPin* SMOut   = SMNode->FindPin(FName(TEXT("Pose")), EGPD_Output);
			if (!SMOut && SMNode->Pins.Num() > 0) SMOut = SMNode->Pins[0];
			UEdGraphPin* RootIn  = RootNode ? RootNode->FindPin(FName(TEXT("Result")), EGPD_Input) : nullptr;
			if (!RootIn && RootNode && RootNode->Pins.Num() > 0) RootIn = RootNode->Pins[0];
			if (SMOut && RootIn) SMOut->MakeLinkTo(RootIn);
		}
	}

	// --- Mark dirty and compile ---
	AnimBP->MarkPackageDirty();
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(AnimBP);
	FKismetEditorUtilities::CompileBlueprint(AnimBP);

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("path"),          AnimBpPath);
	Data->SetStringField(TEXT("idleAnim"),      IdleAnimPath);
	Data->SetStringField(TEXT("walkAnim"),      WalkAnimPath);
	Data->SetNumberField(TEXT("walkThreshold"), WalkThreshold);
	Data->SetStringField(TEXT("message"),
		TEXT("State machine (Idle/Walk) created and Animation Blueprint compiled."));
	return Data;
}
