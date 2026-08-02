// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTService.h"
#include "BTService_UpdateMonsterState.generated.h"

// 몬스터 전투 상태(Passive/Alert/Combat/Enrage) 전이를 전담하는 유일한 노드.
// CombatState/HealthPct/DistanceToTarget 블랙보드 키는 이 클래스만 기록한다.
// 트리의 유일한 "두뇌" 노드, 모든 상태 전이 블랙보드 키의 유일한 작성자로, 루트 Selector에 Service로 부착한다.
UCLASS()
class MY3DACTION_API UBTService_UpdateMonsterState : public UBTService
{
	GENERATED_BODY()

public:
	UBTService_UpdateMonsterState();

protected:
	virtual void TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;

	UPROPERTY(EditAnywhere, Category = "Blackboard")
	FBlackboardKeySelector TargetActorKey;

	UPROPERTY(EditAnywhere, Category = "Blackboard")
	FBlackboardKeySelector CombatStateKey;

	UPROPERTY(EditAnywhere, Category = "Blackboard")
	FBlackboardKeySelector DistanceToTargetKey;

	UPROPERTY(EditAnywhere, Category = "Blackboard")
	FBlackboardKeySelector HealthPctKey;

	UPROPERTY(EditAnywhere, Category = "Monster AI")
	float EnrageHealthPctThreshold = 0.3f;

	// Alert(포효) 상태에서 Combat으로 자동 전환되기까지 걸리는 시간(초). Roar 몽타주 길이와 대략 맞춘다.
	UPROPERTY(EditAnywhere, Category = "Monster AI")
	float AlertToCombatDelay = 1.5f;

private:
	float TimeInAlertState = 0.f;
};
