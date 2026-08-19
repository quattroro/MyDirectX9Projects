// Fill out your copyright notice in the Description page of Project Settings.


#include "BTTask_PlayMontageAndMove.h"

#include "AIController.h"
#include "GameFramework/Character.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "BehaviorTree/BlackboardComponent.h"

UBTTask_PlayMontageAndMove::UBTTask_PlayMontageAndMove()
{
	NodeName = TEXT("Play Montage And Move");
	bNotifyTick = true;
}


//UE_LOG(LogTemp, Log, TEXT("PlayMontage: Montage=%s Rate=%.2f"), *GetNameSafe(MontageToPlay), PlayRate);

EBTNodeResult::Type UBTTask_PlayMontageAndMove::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	AAIController* AIController = OwnerComp.GetAIOwner();
	ACharacter* Character = AIController ? Cast<ACharacter>(AIController->GetPawn()) : nullptr;
	UAnimInstance* AnimInstance = Character ? Character->GetMesh()->GetAnimInstance() : nullptr;
	UBlackboardComponent* BlackboardComp = OwnerComp.GetBlackboardComponent();


	if (!AnimInstance || !MontageToPlay || !BlackboardComp)
	{
		return EBTNodeResult::Failed;
	}

	// 블랙보드에서 TargetActor값을 가져온다.
	FVector TargetLocation = BlackboardComp->GetValueAsVector(TEXT("TargetActor"));

	UPathFollowingComponent* PathFollowingComp = AIController->GetPathFollowingComponent();
	if (PathFollowingComp)
	{
		CachedBTComp = &OwnerComp; // 델리게이트에서 사용하기 위해 저장
		PathFollowingComp->OnRequestFinished.AddUObject(this, &UBTTask_PlayMontageAndMove::OnMoveCompleted);
	}

	FAIMoveRequest MoveRequest(TargetLocation);
	MoveRequest.SetAcceptanceRadius(5.0f);

	FPathFollowingRequestResult RequestResult = AIController->MoveTo(MoveRequest);

	// 이미 목표치에 도달해 있는 등의 이유로 즉시 성공한 경우
	if (RequestResult.Code == EPathFollowingRequestResult::AlreadyAtGoal)
	{
		//AnimInstance->Montage_Stop(3.0, MontageToPlay);
		return EBTNodeResult::Succeeded;
	}
	// 이동 경로 탐색 실패 등의 이유로 시작조차 못 한 경우
	else if (RequestResult.Code == EPathFollowingRequestResult::Failed)
	{
		//AnimInstance->Montage_Stop(3.0, MontageToPlay);
		return EBTNodeResult::Failed;
	}

	AnimInstance->Montage_Play(MontageToPlay, PlayRate);
	return EBTNodeResult::InProgress;
	
}

void UBTTask_PlayMontageAndMove::OnMoveCompleted(FAIRequestID RequestID, const FPathFollowingResult& Result)
{
	if (!CachedBTComp) return;

	AAIController* AIController = CachedBTComp->GetAIOwner();
	if (AIController && AIController->GetPathFollowingComponent())
	{
		// 다음 이동을 위해 델리게이트 바인딩 해제
		AIController->GetPathFollowingComponent()->OnRequestFinished.RemoveAll(this);
	}

	// 이동 성공 여부에 따라 태스크 종료 처리
	if (Result.IsSuccess())
	{
		// 비헤이비어 트리에서 비동기로 동작하는 커스텀 태스크의 작업 완료를 시스템에 알리고 다음 노드로 넘어가기 위해 호출하는 함수
		FinishLatentTask(*CachedBTComp, EBTNodeResult::Succeeded);
	}
	else
	{
		FinishLatentTask(*CachedBTComp, EBTNodeResult::Failed);
	}
}


void UBTTask_PlayMontageAndMove::TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	/*AAIController* AIController = OwnerComp.GetAIOwner();
	ACharacter* Character = AIController ? Cast<ACharacter>(AIController->GetPawn()) : nullptr;
	UAnimInstance* AnimInstance = Character ? Character->GetMesh()->GetAnimInstance() : nullptr;

	if (!AnimInstance || !AnimInstance->Montage_IsPlaying(MontageToPlay))
	{
		FinishLatentTask(OwnerComp, EBTNodeResult::Succeeded);
	}*/
}