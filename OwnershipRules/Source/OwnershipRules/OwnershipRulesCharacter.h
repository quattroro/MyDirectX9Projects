// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Logging/LogMacros.h"
#include "OwnershipRules.h"
#include "Net/Serialization/FastArraySerializer.h"
#include "OwnershipRulesCharacter.generated.h"


UENUM()
enum class ETestEnum : uint8
{
	EnumValue1 UMETA(DisplayName = "My First Option"),
	EnumValue2,
	EnumValue3
};



// 1단계: 배열에 들어갈 개별 아이템 구조체 정의
USTRUCT(BlueprintType)
struct FAmmoItem : public FFastArraySerializerItem
{
	GENERATED_BODY()

	
	FAmmoItem() { Count = 0; }
	FAmmoItem(const int32 count) :Count(count) {}

	UPROPERTY()
	int32 Count = 10;
};

// 2단계: 구조체들을 담을 컨테이너(시리얼라이저) 구조체 정의
USTRUCT(BlueprintType)
struct FAmmoInfo : public FFastArraySerializer
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FAmmoItem> AmmoList;

	// Fast TArray 필수 함수: 복제 조건 설정
	bool NetDeltaSerialize(FNetDeltaSerializeInfo& DeltaParms)
	{
		return FFastArraySerializer::FastArrayDeltaSerialize<FAmmoItem, FAmmoInfo>(AmmoList, DeltaParms, *this);
	}
};

// NetDeltaSerialize 매크로 등록 필수
template<>
struct TStructOpsTypeTraits<FAmmoInfo> : public TStructOpsTypeTraitsBase2<FAmmoInfo>
{
	enum { WithNetDeltaSerializer = true };
};



class USpringArmComponent;
class UCameraComponent;
class UInputMappingContext;
class UInputAction;
struct FInputActionValue;



DECLARE_LOG_CATEGORY_EXTERN(LogTemplateCharacter, Log, All);

UCLASS(config=Game)



class AOwnershipRulesCharacter : public ACharacter
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

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Test")
	ETestEnum TestEnum;

public:
	AOwnershipRulesCharacter();

	

protected:

	/** Called for movement input */
	void Move(const FInputActionValue& Value);

	/** Called for looking input */
	void Look(const FInputActionValue& Value);
			

protected:
	// APawn interface
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;
	
	// To add mapping context
	virtual void BeginPlay();
	virtual void Tick(float DeltaTime) override;

public:
	/** Returns CameraBoom subobject **/
	FORCEINLINE class USpringArmComponent* GetCameraBoom() const { return CameraBoom; }
	/** Returns FollowCamera subobject **/
	FORCEINLINE class UCameraComponent* GetFollowCamera() const { return FollowCamera; }

	UPROPERTY(Replicated)
	float A = 100.0f;
	UPROPERTY(ReplicatedUsing = OnRepNotify_B)
	int32 B;

	UFUNCTION()
	void OnRepNotify_B();


protected: // 클라이언트가 발사 동작을 남발하는 것을 방지하기 위해
	FTimerHandle FireTimer;

	//UPROPERTY(Replicated) // 변수 리플리케이션 사용
	//int32 Ammo = 5;

	UPROPERTY(EditDefaultsOnly, Category = "RPC Character")
	UAnimMontage* FireAniMontage;

	UPROPERTY(EditDefaultsOnly, Category = "RPC Character")
	USoundBase* NoAmmoSound;

	UFUNCTION(Server, Reliable, WithValidation, Category = "RPC Character")
	void ServerFire();

	UFUNCTION(NetMulticast, Unreliable, Category = "RPC Character")
	void MulticastFire();

	UFUNCTION(Client, Unreliable, Category = "RPC Character")
	void ClientPlaySound2D(USoundBase* Sound);

	// 선택한 무기의 유형
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Enumerations Character")
	EWeaponType Weapon;

	UPROPERTY(Replicated)
	int iWeapon;

	UPROPERTY(Replicated, EditDefaultsOnly, BlueprintReadOnly, Category = "Enumerations Character")
	FAmmoInfo Ammo;

	/*UPROPERTY(Replicated, BlueprintReadOnly, Category = "Inventory")
	FInventoryContainer Inventory;*/

	void Pistol();
	void Shotgun();
	void RocketLauncher();
	void Fire();



};

