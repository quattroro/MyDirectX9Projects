// Fill out your copyright notice in the Description page of Project Settings.


#include "BTService_UpdateMonsterState.h"
#include "MonsterAITypes.h"
#include "Monster_Usurper.h"
#include "AIController.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "BehaviorTree/BlackboardComponent.h"

UBTService_UpdateMonsterState::UBTService_UpdateMonsterState()
{
	NodeName = TEXT("Update Monster Combat State");
	Interval = 0.5f;
	RandomDeviation = 0.1f;

	//블랙보드 키가 특정 클래스 또는 그 하위 클래스의 오브젝트만 값으로 가질 수 있도록 제한하는 헬퍼 함수
	TargetActorKey.AddObjectFilter(this, GET_MEMBER_NAME_CHECKED(UBTService_UpdateMonsterState, TargetActorKey), AActor::StaticClass());
	CombatStateKey.AddEnumFilter(this, GET_MEMBER_NAME_CHECKED(UBTService_UpdateMonsterState, CombatStateKey), StaticEnum<EMonsterCombatState>());
	DistanceToTargetKey.AddFloatFilter(this, GET_MEMBER_NAME_CHECKED(UBTService_UpdateMonsterState, DistanceToTargetKey));
	HealthPctKey.AddFloatFilter(this, GET_MEMBER_NAME_CHECKED(UBTService_UpdateMonsterState, HealthPctKey));
}

// 1. 몬스터 폰의 GetHealthPercent() -> HealthPctKey에 기록
// 2. TargetActorKey는 이미 컨트롤러가 써둔 값을 읽기만 함(Perception 소유권은 컨트롤러에 유지)
// 3. 상태 계산 : 
// - HealthPct <= EnrageHealthPctThreshold && 타겟 있음 -> Enrage.
// - 타겟 있음 && 현재 Passive -> Alert (진입 시 TimeInAlertState = 0).
// - 현재 Alert -> TimeInAlertState 누적, AlertToCombatDelay 넘으면 -> Combat.
// - 현재 Combat/Enrage이고 타겟 소실 / 체력 회복 -> 이번 범위에서는 하향 전이 없음
// - 그 외 -> Passive유지
// 4. DisranceToTarget 계산 후 기록 (향후 거리 기반 공격 선택을 위한 스캐풀링)
// 5. CombatStateKey에 Enum 값 기록.
void UBTService_UpdateMonsterState::TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	Super::TickNode(OwnerComp, NodeMemory, DeltaSeconds);

	UBlackboardComponent* BlackboardComp = OwnerComp.GetBlackboardComponent();
	AAIController* AIController = OwnerComp.GetAIOwner();
	if (!BlackboardComp || !AIController)
	{
		return;
	}

	AMonster_Usurper* Monster = Cast<AMonster_Usurper>(AIController->GetPawn());
	if (!Monster)
	{
		return;
	}

	const float HealthPct = Monster->GetHealthPercent();
	BlackboardComp->SetValueAsFloat(HealthPctKey.SelectedKeyName, HealthPct);

	AActor* Target = Cast<AActor>(BlackboardComp->GetValueAsObject(TargetActorKey.SelectedKeyName));

	float DistanceToTarget = 0.f;
	if (Target)
	{
		DistanceToTarget = FVector::Dist(Monster->GetActorLocation(), Target->GetActorLocation());
	}
	BlackboardComp->SetValueAsFloat(DistanceToTargetKey.SelectedKeyName, DistanceToTarget);

	const EMonsterCombatState CurrentState = static_cast<EMonsterCombatState>(BlackboardComp->GetValueAsEnum(CombatStateKey.SelectedKeyName));
	EMonsterCombatState NewState = CurrentState;

	if (HealthPct <= EnrageHealthPctThreshold && Target)
	{
		NewState = EMonsterCombatState::Enrage;
	}
	else if (Target && CurrentState == EMonsterCombatState::Passive)
	{
		NewState = EMonsterCombatState::Alert;
		TimeInAlertState = 0.f;
	}
	else if (CurrentState == EMonsterCombatState::Alert)
	{
		TimeInAlertState += Interval;
		if (TimeInAlertState >= AlertToCombatDelay)
		{
			NewState = EMonsterCombatState::Combat;
		}
	}
	// Combat/Enrage에서 타겟 소실/체력 회복 시 하향 전이는 이번 범위에서 의도적으로 미구현.

	if (NewState != CurrentState)
	{
		UE_LOG(LogTemp, Log, TEXT("Monster CombatState: %d -> %d"), (int32)CurrentState, (int32)NewState);
		BlackboardComp->SetValueAsEnum(CombatStateKey.SelectedKeyName, static_cast<uint8>(NewState));
	}
}
