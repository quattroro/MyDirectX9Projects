// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "MainCharacter.generated.h"

class USpringArmComponent;
class UCameraComponent;
class UInputMappingContext;
class UInputAction;
struct FInputActionValue;
class AMonster_Usurper;

UCLASS()
class MY3DACTION_API AMainCharacter : public ACharacter
{
	GENERATED_BODY()

	/** Camera boom positioning the camera behind the character */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Camera, meta = (AllowPrivateAccess = "true"))
	USpringArmComponent* CameraBoom;

	/** Follow camera */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Camera, meta = (AllowPrivateAccess = "true"))
	UCameraComponent* FollowCamera;

	/** MappingContext */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	UInputMappingContext* DefaultMappingContext;

	/** Jump Input Action */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	UInputAction* JumpAction;

	/** Move Input Action */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	UInputAction* MoveAction;

	/** Look Input Action */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	UInputAction* LookAction;

	/** Look Input Action */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	UInputAction* RunAction;

	/** Look Input Action */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	UInputAction* AttackAction;

	/** Look Input Action */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	UInputAction* DefenceAction;

	/** Roll Input Action */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	UInputAction* RollAction;

	UPROPERTY(EditAnywhere)
	UAnimMontage* Attack1_Montage;

	UPROPERTY(EditAnywhere)
	UAnimMontage* Attack2_Montage;


	UPROPERTY(EditAnywhere)
	UAnimMontage* Defence_Montage;

	// 제자리 구르기 몽타주. 이동은 Roll()에서 Root Motion Source로 처리한다.
	UPROPERTY(EditAnywhere)
	UAnimMontage* Roll_Montage;

	// 구르기 한 번에 이동하는 거리 (원본 애니메이션의 루트 이동량)
	UPROPERTY(EditAnywhere, Category = Roll)
	float RollDistance = 280.f;

	// 구르는 동안의 속도 배율 커브 (X: 0~1 진행도, Y: 배율). 비워두면 등속.
	// 평균값이 1이어야 실제 이동 거리가 RollDistance와 같아진다.
	UPROPERTY(EditAnywhere, Category = Roll)
	class UCurveFloat* RollSpeedCurve = nullptr;


	int AttackCount;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	bool Defence;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	bool IsBattleMode;

	UPROPERTY(EditAnywhere)
	AMonster_Usurper* TestMonster;

	UPROPERTY()
	class UStaticMeshComponent* SwordMeshComp;
	bool bWeaponCollisionEnable = false;
	bool bWeaponHasHitThisSwing = false;

	
	
	UFUNCTION()
	void OnSwordBeinOverlap(UPrimitiveComponent* OverlappedComp, AActor* OthreActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);
public:
	// Sets default values for this character's properties
	AMainCharacter();
	
protected:

	/** Called for movement input */
	void Move(const FInputActionValue& Value);

	/** Called for looking input */
	void Look(const FInputActionValue& Value);

	void Attack();

	void BeginRunning();
	void StopRunning();


	void BeginDefence();
	void StopDefence();

	bool Attacking();

	void Roll();
	bool Rolling() const;

	// 구르는 도중에는 점프할 수 없다.
	virtual bool CanJumpInternal_Implementation() const override;

protected:
	// APawn interface
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	// To add mapping context
	virtual void BeginPlay();

public:
	/** Returns CameraBoom subobject **/
	FORCEINLINE class USpringArmComponent* GetCameraBoom() const { return CameraBoom; }
	/** Returns FollowCamera subobject **/
	FORCEINLINE class UCameraComponent* GetFollowCamera() const { return FollowCamera; }

	void EnableWeaponCollision();
	void DisableWeaponCollision();

	// 런타임에 생성한 다이나믹 머티리얼 인스턴스 저장용
	UPROPERTY(EditAnywhere)
	class UMaterialInstanceDynamic* DynamicMaterialInst;
};
