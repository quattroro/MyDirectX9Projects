// Fill out your copyright notice in the Description page of Project Settings.


#include "Anim_AttackNotifyState.h"
#include "MainCharacter.h"


// MeshComp -> 애니메이션을 재생하고 있는 대상 캐릭터나 스켈레탈 메시 컴포넌트
// Animation -> 현재 재생중인 애니메이션 시퀀스 에셋 정보
// TotalDuration -> 해당 노티파이 스테이트가 지속되도록 설정된 전체 시간(초)
// EventReference -> 해당 노티파이 이벤트에 대한 참조 정보
void UAnim_AttackNotifyState::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
    if (MeshComp && MeshComp->GetOwner())
    {
        // 3. 넘겨받은 액터를 AMainCharacter로 형변환(Cast)합니다.
        AMainCharacter* MyCharacter = Cast<AMainCharacter>(MeshComp->GetOwner());

        if (MyCharacter)
        {
            MyCharacter->EnableWeaponCollision();
            //UE_LOG(LogTemp, Log, TEXT("NotifyBegin"));
        }
    }
	//
}

//void UAnim_AttackNotifyState::NotifyTick(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float FrameDeltaTime, const FAnimNotifyEventReference& EventReference)
//{
//	UE_LOG(LogTemp, Log, TEXT("Attack2 Begin"));
//}

void UAnim_AttackNotifyState::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
    if (MeshComp && MeshComp->GetOwner())
    {
        // 3. 넘겨받은 액터를 AMainCharacter로 형변환(Cast)합니다.
        AMainCharacter* MyCharacter = Cast<AMainCharacter>(MeshComp->GetOwner());

        if (MyCharacter)
        {
            MyCharacter->DisableWeaponCollision();
            //UE_LOG(LogTemp, Log, TEXT("NotifyEnd"));
        }
    }

	//UE_LOG(LogTemp, Log, TEXT("Attack2 end"));
}
