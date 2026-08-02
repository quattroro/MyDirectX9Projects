// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BTTask_FindRandomWanderPoint.generated.h"

// HomeLocation을 중심으로 반경 내 임의 지점을 찾아 WanderLocation 키에 기록한다.
// 실제 이동은 표준 BTTask_MoveTo가 이 키를 소비해서 처리한다.
UCLASS()
class MY3DACTION_API UBTTask_FindRandomWanderPoint : public UBTTaskNode
{
	GENERATED_BODY()

public:
	UBTTask_FindRandomWanderPoint();

protected:
	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;

	UPROPERTY(EditAnywhere, Category = "Blackboard")
	FBlackboardKeySelector HomeLocationKey;

	UPROPERTY(EditAnywhere, Category = "Blackboard")
	FBlackboardKeySelector WanderLocationKey;

	UPROPERTY(EditAnywhere, Category = "Monster AI")
	float WanderRadius = 1000.f;
};
