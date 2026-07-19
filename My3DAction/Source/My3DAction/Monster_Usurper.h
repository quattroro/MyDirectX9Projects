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

public:	
	UFUNCTION(BlueprintCallable)
	void Hit();

	void TestTrigger();

	// Called every frame
	virtual void Tick(float DeltaTime) override;

	// Called to bind functionality to input
	//`virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

};
