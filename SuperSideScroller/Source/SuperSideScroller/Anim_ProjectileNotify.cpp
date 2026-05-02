// Fill out your copyright notice in the Description page of Project Settings.


#include "Anim_ProjectileNotify.h"

// 이 함수는 AM_Throw 몽타주에 애니메이션 노티파이로 추가되어서 실행된다.
void UAnim_ProjectileNotify::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);
	// Notify함수는 소유하는 스켈레탈 메시를 MeshComp라는 이름으로 전달받는다. GetOwner()함수를 사용해 플레이어 캐릭터의 
	// 참조를 구하는데 스켈레탈 메시를 사용할 수 있다. GetOwner함수를 통해 반환되는 액터를 SuperSideScroller_Player클래스로 
	// 형 변환해 플레이어 캐릭터의 참조를 구할 수 있다.
	UE_LOG(LogTemp, Warning, TEXT("Notify"));
	ASuperSideScroller_Player* Player = Cast<ASuperSideScroller_Player>(MeshComp->GetOwner());
	if (Player)
	{
		UE_LOG(LogTemp, Warning, TEXT("Create"));
		Player->SpawnProjectile();
	}
}
