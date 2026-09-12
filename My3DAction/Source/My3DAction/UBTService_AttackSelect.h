// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTService.h"
#include "UBTService_AttackSelect.generated.h"

/**
 * 
 */
UCLASS()

// 각 상황에 따라서 몬스터의 공격 패턴을 정해준다.
class MY3DACTION_API UUBTService_AttackSelect : public UBTService
{
	GENERATED_BODY()
public:
	UUBTService_AttackSelect();


	UPROPERTY(EditAnywhere, Category = "Blackboard")
	FBlackboardKeySelector TargetActorKey;


	UPROPERTY(EditAnywhere, Category = "Blackboard")
	FBlackboardKeySelector AttackTypeKey;

	UPROPERTY(EditAnywhere, Category = "Blackboard")
	FBlackboardKeySelector LoopCountKey;
protected:
	virtual void TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;
};
