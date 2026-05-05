// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "SuperSideScrollerCharacter.h"
#include "SuperSideScroller_Player.generated.h"


UCLASS()
class SUPERSIDESCROLLER_API ASuperSideScroller_Player : public ASuperSideScrollerCharacter
{
	GENERATED_BODY()
	
public:
	ASuperSideScroller_Player();
	void Sprint();
	void StopSprinting();
	void TrowProjectile();
	void SpawnProjectile();

	// 이 함수를 블루프린트에 노출시키고, 이후 UMG에서 사용할 수 있도록 BlueprintPure 키워드를 사용
	UFUNCTION(BlueprintPure)
	int GetCurrentNumberOfCollectables() { return Numberofcollectabels; }

	// 플레이어가 수집한 동전의 수를 추적하는 데 사용할 함수
	void IncrementNumberofCollectables(int32 Value);
protected:
	// 플레이어 입력 컴포넌트를 설정하기 위해 부모 캐릭터 클래스의 함수를 재정의한다.
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

private:
	// 전력 질주 중인지 확인하는 bool 변수.
	bool bIsSprinting;

	UPROPERTY(EditAnywhere, Category = "Input")
	class UInputMappingContext* IC_Character;

	UPROPERTY(EditAnywhere, Category = "Input")
	class UInputAction* IA_Sprint;

	UPROPERTY(EditAnywhere, Category = "Input")
	class UInputAction* IA_Throw;

	// 애니메이션 몽타주
	UPROPERTY(EditAnywhere)
	class UAnimMontage* ThrowMontage;

	// projectile 발사체 오브젝트 참조 변수
	UPROPERTY(EditAnywhere)
	TSubclassOf<class APlayerProjectile> PlayerProjectile;

	// 플레이어가 수집한 코인의 수를 추적
	int32 Numberofcollectabels;
};
