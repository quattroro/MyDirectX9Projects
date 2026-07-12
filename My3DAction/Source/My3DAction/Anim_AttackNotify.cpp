// Fill out your copyright notice in the Description page of Project Settings.


#include "Anim_AttackNotify.h"

void UAnim_AttackNotify::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	UE_LOG(LogTemp, Log, TEXT("Attack2"));
}
