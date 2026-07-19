// Fill out your copyright notice in the Description page of Project Settings.


#include "Anim_AttackNotifyState.h"

void UAnim_AttackNotifyState::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
	//UE_LOG(LogTemp, Log, TEXT("Attack2 Begin"));
}

//void UAnim_AttackNotifyState::NotifyTick(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float FrameDeltaTime, const FAnimNotifyEventReference& EventReference)
//{
//	UE_LOG(LogTemp, Log, TEXT("Attack2 Begin"));
//}

void UAnim_AttackNotifyState::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	//UE_LOG(LogTemp, Log, TEXT("Attack2 end"));
}
