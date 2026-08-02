// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Monster_Usurper.generated.h"

class UInputAction;
class UInputMappingContext;

UCLASS()
class MY3DACTION_API AMonster_Usurper : public ACharacter
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	UInputMappingContext* DefaultMappingContext;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	UInputAction* AttackAction;

public:
	// Sets default values for this character's properties
	AMonster_Usurper();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	class UMaterialInstanceDynamic* DynamicMaterialInst;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Health, meta = (AllowPrivateAccess = "true"))
	float MaxHealth = 1000.f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Health, meta = (AllowPrivateAccess = "true"))
	float Health;

public:
	UFUNCTION(BlueprintCallable)
	void Hit(FVector pos, FVector dir);

	// 실제 게임플레이 데미지 처리. Hit()과는 분리되어 있음 (Hit()은 VFX 전용).
	UFUNCTION(BlueprintCallable, Category = Health)
	void ApplyDamage(float DamageAmount);

	UFUNCTION(BlueprintCallable, Category = Health)
	float GetHealthPercent() const { return MaxHealth > 0.f ? Health / MaxHealth : 0.f; }

	UFUNCTION(BlueprintCallable, Category = Health)
	bool IsDead() const { return Health <= 0.f; }

	//void TestTrigger();

	// Called every frame
	virtual void Tick(float DeltaTime) override;

	// Called to bind functionality to input
	//`virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

};
