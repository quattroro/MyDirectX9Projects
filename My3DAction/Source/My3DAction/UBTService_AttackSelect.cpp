// Fill out your copyright notice in the Description page of Project Settings.


#include "UBTService_AttackSelect.h"
#include "MonsterAITypes.h"
#include "Monster_Usurper.h"
#include "AIController.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "BehaviorTree/BlackboardComponent.h"


UUBTService_AttackSelect::UUBTService_AttackSelect()
{
	NodeName = TEXT("Monster Attack Selector");
	Interval = 0.5f;
	RandomDeviation = 0.1f;
}


void UUBTService_AttackSelect::TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	//UE_LOG(LogTemp, Log, TEXT("Enter"));
	Super::TickNode(OwnerComp, NodeMemory, DeltaSeconds);

	UBlackboardComponent* BlackboardComp = OwnerComp.GetBlackboardComponent();
	AAIController* AIController = OwnerComp.GetAIOwner();
	if (!BlackboardComp || !AIController)
	{
		//UE_LOG(LogTemp, Log, TEXT("return1"));
		return;
	}

	AMonster_Usurper* Monster = Cast<AMonster_Usurper>(AIController->GetPawn());
	if (!Monster)
	{
		//UE_LOG(LogTemp, Log, TEXT("return2"));
		return;
	}

	//// 몬스터의 현재 체력을 받아와서 블랙보드에 기록한다.
	//const float HealthPct = Monster->GetHealthPercent();
	//BlackboardComp->SetValueAsFloat(HealthPctKey.SelectedKeyName, HealthPct);

	//TargetActor는  AMonsterAIController::OnTargetPerceptionUpdated 함수에서 세팅된다.
	AActor* Target = Cast<AActor>(BlackboardComp->GetValueAsObject(TargetActorKey.SelectedKeyName));

	float DistanceToTarget = 0.f;
	if (Target)
	{
		DistanceToTarget = FVector::Dist(Monster->GetActorLocation(), Target->GetActorLocation());
	}

	// 타깃과 몬스터의 위치에 따라서 AttackType을 세팅한다.
	EMonsterAttackType CurrentType = static_cast<EMonsterAttackType>(BlackboardComp->GetValueAsEnum(AttackTypeKey.SelectedKeyName));
	EMonsterAttackType AttackType = EMonsterAttackType::AK_None;

	//UE_LOG(LogTemp, Log, TEXT("Enter Service 1"));

	if (CurrentType == EMonsterAttackType::AK_None)
	{
		int Rand = FMath::RandRange(0, 2);
		//int LoopCount = 0;

		if (Rand == 0)
		{
			AttackType = EMonsterAttackType::AK_Mouth;
			//LoopCount = FMath::RandRange(0, 2);
		}
		else if (Rand == 1)
		{
			AttackType = EMonsterAttackType::AK_Crow;
			//LoopCount = FMath::RandRange(0, 1);
		}
		else
		{
			AttackType = EMonsterAttackType::AK_Flame;
		}

		if (CurrentType != AttackType)
		{
			UE_LOG(LogTemp, Log, TEXT("Monster Attack Type: %d -> %d"), (int32)CurrentType, (int32)AttackType);
			BlackboardComp->SetValueAsEnum(AttackTypeKey.SelectedKeyName, static_cast<uint8>(AttackType));
			//BlackboardComp->SetValueAsInt(LoopCountKey.SelectedKeyName, LoopCount);
		}
	}
}