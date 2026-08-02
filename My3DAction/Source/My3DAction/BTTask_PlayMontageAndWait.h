// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BTTask_PlayMontageAndWait.generated.h"

class UAnimMontage;

// 범용 몽타주 재생 태스크. Roar, 각종 공격 등 모든 몽타주 재생에 재사용한다.
UCLASS()
class MY3DACTION_API UBTTask_PlayMontageAndWait : public UBTTaskNode
{
	GENERATED_BODY()

public:
	UBTTask_PlayMontageAndWait();

protected:
	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual void TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;

	UPROPERTY(EditAnywhere, Category = "Animation")
	UAnimMontage* MontageToPlay;

	UPROPERTY(EditAnywhere, Category = "Animation")
	float PlayRate = 1.f;

	UPROPERTY(EditAnywhere, Category = "Animation")
	bool bWaitForCompletion = true;
};
