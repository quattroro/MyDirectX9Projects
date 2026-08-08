// Fill out your copyright notice in the Description page of Project Settings.


#include "MonsterAIController.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/BlackboardData.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISenseConfig_Sight.h"


// 생성자에서 AIPerceptionComp + SightConfig(Sight, Pawn감지) 구성
// OnPossess : Super::OnPossess -> 블랙보드 초기화(UseBlackboard) -> HomeLocation 캐싱 -> RunBehaciorTree(BehaviorTreeAsset).
// OnTargetPerceptionUpdated : 플레이어를 감지하면 TargetActor 블랙보드 키만 기록, 상태 전이 로직(Passive->Alert->Combat)은 여기서 하지 않고 Service가 전담
// BehaviorTreeAsset은 EditDefaultsOnly로 비워두고 실제 BT에셋은 이 컨트롤러의 블루프린트 자식(BP_MonsterAIControlle)에서 지정 - BP_Monster가 AMonster_Usurper를 감싸는 기존 패턴과 동일한 방식.
AMonsterAIController::AMonsterAIController()
{
	AIPerceptionComp = CreateDefaultSubobject<UAIPerceptionComponent>(TEXT("AIPerceptionComp"));
	SightConfig = CreateDefaultSubobject<UAISenseConfig_Sight>(TEXT("SightConfig"));

	SightConfig->SightRadius = 1500.f;// 인지 반경 1500
	SightConfig->LoseSightRadius = 1800.f;// 시야 상실 반경 1800.f
	SightConfig->PeripheralVisionAngleDegrees = 90.f;// 시야각 90도 (좌우 45도)
	SightConfig->SetMaxAge(5.f); // AI가 대상을 시야에서 놓치거나 소리가 끊겼을 때, "방금 거기서 무언가를 인지했다"라는 사실을 몇 초 동안 기억할지 결정.
	SightConfig->AutoSuccessRangeFromLastSeenLocation = 900.f; //AI가 대상을 시야에서 놓친 후, 대상의 '마지막 목격 위치'를 기준으로 설정된 반지름 내에 대상이 머물러 있다면 시야에 보이지 않더라도 감지를 성공(Success) 상태로 유지하는 자동 성공 반경입니다.
	SightConfig->DetectionByAffiliation.bDetectEnemies = true; // 적을 감지할것인지
	SightConfig->DetectionByAffiliation.bDetectNeutrals = true; // 중립을 감지할것인지
	SightConfig->DetectionByAffiliation.bDetectFriendlies = true; // 아군은 감지할것인지

	AIPerceptionComp->ConfigureSense(*SightConfig);
	AIPerceptionComp->SetDominantSense(SightConfig->GetSenseImplementation());
	AIPerceptionComp->OnTargetPerceptionUpdated.AddDynamic(this, &AMonsterAIController::OnTargetPerceptionUpdated);

	SetPerceptionComponent(*AIPerceptionComp);
}

void AMonsterAIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	if (InPawn)
	{
		HomeLocation = InPawn->GetActorLocation();
	}

	if (BehaviorTreeAsset && BehaviorTreeAsset->BlackboardAsset)
	{
		UBlackboardComponent* BlackboardComp = nullptr;
		UseBlackboard(BehaviorTreeAsset->BlackboardAsset, BlackboardComp);

		if (BlackboardComp)
		{
			BlackboardComp->SetValueAsVector(TEXT("HomeLocation"), HomeLocation);
		}

		UE_LOG(LogTemp, Log, TEXT("Monster Behavior Tree Is Run"));

		RunBehaviorTree(BehaviorTreeAsset);
	}
}

// OnTargetPerceptionUpdated : 플레이어를 감지하면 TargetActor 블랙보드 키만 기록, 상태 전이 로직(Passive->Alert->Combat)은 여기서 하지 않고 Service가 전담
void AMonsterAIController::OnTargetPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus)
{
	if (!Actor || !GetBlackboardComponent())
	{
		return;
	}

	if (Stimulus.WasSuccessfullySensed())
	{
		GetBlackboardComponent()->SetValueAsObject(TEXT("TargetActor"), Actor);
	}
}
