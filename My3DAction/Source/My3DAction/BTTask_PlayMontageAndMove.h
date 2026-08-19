// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "Navigation/PathFollowingComponent.h"
#include "BTTask_PlayMontageAndMove.generated.h"

class UAnimMontage;

UCLASS()
class MY3DACTION_API UBTTask_PlayMontageAndMove : public UBTTaskNode
{
	GENERATED_BODY()
public:
	UBTTask_PlayMontageAndMove();

protected:
	// 태스크 시작 시 호출
	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	// 
	virtual void TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;

	// 이동 완료 시 호출될 콜핵 함수
	void OnMoveCompleted(FAIRequestID RequestID, const FPathFollowingResult& Result);

	UPROPERTY(EditAnywhere, Category = "Animation")
	UAnimMontage* MontageToPlay;

	UPROPERTY(EditAnywhere, Category = "Animation")
	float PlayRate = 1.f;

	UPROPERTY(EditAnywhere, Category = "Animation")
	bool bWaitForCompletion = true;

private:
	// 컴포넌트 참조 저장용 (델리게이트 바인딩 해제 시 사용)
	UPROPERTY()
	TObjectPtr<UBehaviorTreeComponent> CachedBTComp;
};
