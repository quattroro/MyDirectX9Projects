// Fill out your copyright notice in the Description page of Project Settings.


#include "BTTask_PlayMontageAndWait.h"
#include "AIController.h"
#include "GameFramework/Character.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "BehaviorTree/BehaviorTreeComponent.h"

UBTTask_PlayMontageAndWait::UBTTask_PlayMontageAndWait()
{
	NodeName = TEXT("Play Montage And Wait");
	bNotifyTick = true;
}

EBTNodeResult::Type UBTTask_PlayMontageAndWait::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	AAIController* AIController = OwnerComp.GetAIOwner();
	ACharacter* Character = AIController ? Cast<ACharacter>(AIController->GetPawn()) : nullptr;
	UAnimInstance* AnimInstance = Character ? Character->GetMesh()->GetAnimInstance() : nullptr;

	if (!AnimInstance || !MontageToPlay)
	{
		return EBTNodeResult::Failed;
	}

	AnimInstance->Montage_Play(MontageToPlay, PlayRate);

	return bWaitForCompletion ? EBTNodeResult::InProgress : EBTNodeResult::Succeeded;
}

void UBTTask_PlayMontageAndWait::TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	AAIController* AIController = OwnerComp.GetAIOwner();
	ACharacter* Character = AIController ? Cast<ACharacter>(AIController->GetPawn()) : nullptr;
	UAnimInstance* AnimInstance = Character ? Character->GetMesh()->GetAnimInstance() : nullptr;

	if (!AnimInstance || !AnimInstance->Montage_IsPlaying(MontageToPlay))
	{
		FinishLatentTask(OwnerComp, EBTNodeResult::Succeeded);
	}
}
