// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BTTask_PlayMontage.generated.h"

/**
 * 
 */
UCLASS()
class MY3DACTION_API UBTTask_PlayMontage : public UBTTaskNode
{
	GENERATED_BODY()
public:
	UBTTask_PlayMontage();

	// 에셋 로드 시 블랙보드 키 셀렉터를 실제 블랙보드 키에 연결한다.
	virtual void InitializeFromAsset(UBehaviorTree& Asset) override;
protected:
	
	// 태스크 시작 시 호출
	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	//
	virtual void TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;
	// 상위 노드가 이 태스크를 중단시킬 때 호출. 몽타주와 이동을 정리한다.
	// ex) 데코레이터(Decorator)의 조건 변경에 의한 중단, 컴포지트 노드(Composite)의 규칙에 따른 중단
	virtual EBTNodeResult::Type AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	// 이동 완료 시 호출될 콜백 함수
	//void OnMoveCompleted(FAIRequestID RequestID, const FPathFollowingResult& Result);

	UPROPERTY(EditAnywhere, Category = "Animation")
	UAnimMontage* MontageToPlay;

	// 공격 종료 이후에 공격 상태를 None으로 다시 세팅해주기 위해
	UPROPERTY(EditAnywhere, Category = "Blackboard")
	FBlackboardKeySelector AttackType;

	UPROPERTY(EditAnywhere, Category = "Animation")
	float PlayRate = 1.f;

	// true면 이동이 끝나도 몽타주 재생이 끝날 때까지 태스크를 유지한다.
	UPROPERTY(EditAnywhere, Category = "Animation")
	bool bWaitForCompletion = true;
private:
	// 태스크를 한 번만 종료시킨다. FinishLatentTask 중복 호출 방지.
	void FinishTask(UBehaviorTreeComponent& OwnerComp, EBTNodeResult::Type Result);
	// OnRequestFinished 바인딩 해제. 모든 종료 경로에서 호출해야 한다.
	void UnbindMoveDelegate(AAIController* AIController);
	// 이동/몽타주 조건이 모두 충족됐으면 태스크를 종료한다.
	void TryFinish(UBehaviorTreeComponent& OwnerComp);
	// 몽타주 재생이 끝났다고 볼 수 있는지. 재생 시작 전 첫 프레임 오종료를 막는다.
	bool IsMontageDone(UAnimInstance* AnimInstance) const;
	// OwnerComp에서 AnimInstance를 얻는다.
	static UAnimInstance* GetAnimInstance(UBehaviorTreeComponent& OwnerComp);

	// 컴포넌트 참조 저장용 (델리게이트 바인딩 해제 시 사용)
	UPROPERTY()
	TObjectPtr<UBehaviorTreeComponent> CachedBTComp;

	// 이 태스크가 요청한 이동 ID. 같은 컨트롤러의 다른 MoveTo(기본 BTTask_MoveTo 등)가
	// 끝난 것을 이 태스크의 완료로 오인하지 않기 위해 비교한다.
	FAIRequestID MoveRequestID;

	// ExecuteTask 이후 경과 시간(초)
	float ElapsedTime = 0.f;

	// Montage_Play가 유효한 길이를 반환했는지. false면 몽타주를 기다리지 않는다.
	bool bMontagePlayStarted = false;

	// 이동이 끝났는지와 그 결과
	//bool bMoveFinished = false;
	//bool bMoveSucceeded = false;

	// FinishLatentTask를 이미 호출했는지
	bool bTaskFinished = false;
};
