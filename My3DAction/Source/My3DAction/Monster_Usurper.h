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

	//
	// --- Head Look At ---
	// AI가 타겟을 감지하면 몸통은 그대로 두고 머리만 타겟을 향하게 한다.
	// 실제 본 회전은 AnimBP(AMP_Monster)의 LookAt 스켈레탈 컨트롤 노드가 수행하고,
	// 여기서는 그 노드에 먹일 값(LookAtLocation / LookAtAlpha)만 매 프레임 계산한다.
	//

	// 머리를 돌리는 기준이 되는 본. Dragon 스켈레톤의 머리 본 이름.
	UPROPERTY(EditAnywhere, Category = "Look At", meta = (AllowPrivateAccess = "true"))
	FName LookAtBoneName = TEXT("Head");

	// 좌우 최대 각도. 이 범위를 넘어가면 방향을 이 각도로 클램프한다.
	UPROPERTY(EditAnywhere, Category = "Look At", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", ClampMax = "180.0"))
	float LookAtMaxYaw = 70.f;

	// 상하 최대 각도.
	UPROPERTY(EditAnywhere, Category = "Look At", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", ClampMax = "90.0"))
	float LookAtMaxPitch = 45.f;

	// MaxYaw를 넘어선 뒤 이 각도만큼 더 벌어지면 Alpha를 0으로 페이드해 정면으로 돌아온다.
	// (타겟이 완전히 등 뒤로 갔을 때 머리가 한계각에 붙어 있는 것을 방지)
	UPROPERTY(EditAnywhere, Category = "Look At", meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
	float LookAtYawFalloff = 30.f;

	// Alpha 블렌딩 속도. 클수록 빠르게 쳐다보고 빠르게 정면 복귀한다.
	UPROPERTY(EditAnywhere, Category = "Look At", meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
	float LookAtBlendSpeed = 4.f;

	// 타겟 액터 원점(발밑) 기준 Z 오프셋. 머리가 아니라 발을 쳐다보는 것을 막는다.
	UPROPERTY(EditAnywhere, Category = "Look At", meta = (AllowPrivateAccess = "true"))
	float LookAtTargetZOffset = 60.f;

	// AnimBP가 읽어가는 값 - 메시 '컴포넌트 공간' 기준 응시 지점.
	// LookAt 노드는 LookAtTarget 본이 비어 있을 때 LookAtLocation을 컴포넌트 공간으로 해석한다.
	UPROPERTY(BlueprintReadOnly, Category = "Look At", meta = (AllowPrivateAccess = "true"))
	FVector LookAtLocation = FVector::ZeroVector;

	// AnimBP가 읽어가는 값 - LookAt 노드의 Alpha (0 = 정면, 1 = 완전히 응시).
	UPROPERTY(BlueprintReadOnly, Category = "Look At", meta = (AllowPrivateAccess = "true"))
	float LookAtAlpha = 0.f;

	// 현재 쳐다보고 있는 대상. AIController가 Perception 이벤트로 넣어준다.
	TWeakObjectPtr<AActor> LookAtTarget;

	void UpdateLookAt(float DeltaTime);

public:
	// AIController가 타겟을 감지/소실했을 때 호출. nullptr을 넣으면 정면으로 복귀한다.
	UFUNCTION(BlueprintCallable, Category = "Look At")
	void SetLookAtTarget(AActor* NewTarget);

	UFUNCTION(BlueprintPure, Category = "Look At")
	AActor* GetLookAtTarget() const { return LookAtTarget.Get(); }

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
