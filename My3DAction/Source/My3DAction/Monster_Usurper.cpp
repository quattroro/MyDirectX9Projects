// Fill out your copyright notice in the Description page of Project Settings.


#include "Monster_Usurper.h"
#include "Components/SkeletalMeshComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"

// Sets default values
AMonster_Usurper::AMonster_Usurper()
{
 	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

}

// Called when the game starts or when spawned
void AMonster_Usurper::BeginPlay()
{
	Super::BeginPlay();
	
	UMaterialInterface* BaseMat = GetMesh()->GetMaterial(0);
	DynamicMaterialInst = UMaterialInstanceDynamic::Create(BaseMat, this);
	GetMesh()->SetMaterial(0, DynamicMaterialInst);

	Health = MaxHealth;

	UE_LOG(LogTemp, Log, TEXT("enter Hear?"));

	// APlayerController의 DefaultPawnClass로 지정되어 있는 APawn은 PlayerController가 Possess해야 호출 가능하지만, Player가 아닌 다른 APawn 객체들은
	// 자동으로 AAIController가 빙의한다.

	/*if (APlayerController* PlayerController = Cast<APlayerController>(Controller))
	{
		UE_LOG(LogTemp, Log, TEXT("enter Hear?"));
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PlayerController->GetLocalPlayer()))
		{
			UE_LOG(LogTemp, Log, TEXT("enter Hear?"));
			Subsystem->AddMappingContext(DefaultMappingContext, 0);
		}
	}*/
}

void AMonster_Usurper::Hit(FVector pos, FVector dir)
{
	if (DynamicMaterialInst)
	{
		float T = GetWorld()->GetTimeSeconds();
		DynamicMaterialInst->SetScalarParameterValue(TEXT("HitTime"), T);
		DynamicMaterialInst->SetVectorParameterValue(TEXT("HitPos"), pos);
		DynamicMaterialInst->SetVectorParameterValue(TEXT("HitDir"), dir);


		float Check;
		FLinearColor CurrentColor;
		bool bFound = DynamicMaterialInst->GetScalarParameterValue(FName("HitTime"), Check);
		UE_LOG(LogTemp, Log, TEXT("HitTime set : found = %d, value = %f"), bFound, Check);
		bFound = DynamicMaterialInst->GetVectorParameterValue(FName("HitPos"), CurrentColor);
		UE_LOG(LogTemp, Log, TEXT("HitTime set : found = %d, value = %s"), bFound, *CurrentColor.ToString());
	}
}

void AMonster_Usurper::ApplyDamage(float DamageAmount)
{
	Health = FMath::Clamp(Health - DamageAmount, 0.f, MaxHealth);
	UE_LOG(LogTemp, Log, TEXT("ApplyDamage: -%f, Health = %f / %f"), DamageAmount, Health, MaxHealth);
}

//void AMonster_Usurper::TestTrigger()
//{
//	UE_LOG(LogTemp, Log, TEXT("MonsterTrigger"));
//	Hit();
//}

//
// --- Head Look At ---
//

void AMonster_Usurper::SetLookAtTarget(AActor* NewTarget)
{
	LookAtTarget = NewTarget;
}

// 매 프레임 LookAt 노드에 먹일 값을 계산한다.
// 1. 머리 본의 현재 월드 위치를 기준으로 타겟까지의 방향을 구한다.
// 2. 액터 정면(= 머리 본의 기본 정면) 기준 상대 각도를 구해 MaxYaw/MaxPitch로 클램프한다.
//    -> 클램프하지 않으면 목이 꺾이는 각도까지 그대로 돌아가 버린다.
// 3. 한계각을 LookAtYawFalloff 이상 넘어서거나 타겟이 없거나 죽었으면 Alpha를 0으로 보낸다.
// 4. 최종 응시 지점을 메시 '컴포넌트 공간'으로 변환해 LookAtLocation에 기록한다.
//    (LookAt 노드는 LookAtTarget 본이 비어 있으면 LookAtLocation을 컴포넌트 공간으로 해석한다)
void AMonster_Usurper::UpdateLookAt(float DeltaTime)
{
	USkeletalMeshComponent* MeshComp = GetMesh();
	if (!MeshComp)
	{
		return;
	}

	const FVector HeadWorldLocation = MeshComp->DoesSocketExist(LookAtBoneName)
		? MeshComp->GetSocketLocation(LookAtBoneName)
		: GetActorLocation();

	const FRotator ActorRotation = GetActorRotation();

	// 타겟이 없을 때의 기본값: 정면을 본다(= Alpha가 0으로 빠지므로 실제로는 원본 포즈).
	FVector DesiredWorldLocation = HeadWorldLocation + ActorRotation.Vector() * 500.f;
	float DesiredAlpha = 0.f;

	AActor* Target = LookAtTarget.Get();
	if (Target && !IsDead())
	{
		// 타겟의 위치
		const FVector TargetWorldLocation = Target->GetActorLocation() + FVector(0.f, 0.f, LookAtTargetZOffset);
		// 
		const FVector ToTarget = TargetWorldLocation - HeadWorldLocation;

		if (!ToTarget.IsNearlyZero())
		{
			// 액터 정면 기준 상대 각도
			const FRotator DeltaRotation = (ToTarget.Rotation() - ActorRotation).GetNormalized();

			const FRotator ClampedDelta(
				FMath::Clamp(DeltaRotation.Pitch, -LookAtMaxPitch, LookAtMaxPitch),
				FMath::Clamp(DeltaRotation.Yaw, -LookAtMaxYaw, LookAtMaxYaw),
				0.f);

			DesiredWorldLocation = HeadWorldLocation + (ActorRotation + ClampedDelta).Vector() * ToTarget.Size();

			// 등 뒤로 완전히 넘어가면 목을 한계각에 붙여두지 말고 정면으로 복귀시킨다.
			const float YawOvershoot = FMath::Abs(DeltaRotation.Yaw) - LookAtMaxYaw;
			DesiredAlpha = (LookAtYawFalloff > KINDA_SMALL_NUMBER)
				? 1.f - FMath::Clamp(YawOvershoot / LookAtYawFalloff, 0.f, 1.f)
				: (YawOvershoot > 0.f ? 0.f : 1.f);
		}
	}

	LookAtAlpha = FMath::FInterpTo(LookAtAlpha, DesiredAlpha, DeltaTime, LookAtBlendSpeed);
	LookAtLocation = MeshComp->GetComponentTransform().InverseTransformPosition(DesiredWorldLocation);
}

// Called every frame
void AMonster_Usurper::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	UpdateLookAt(DeltaTime);
}

//
// 해당 함수는 PlayerController가 Player를 possess할 때 호출되는 함수이기 때문에 여기서는 호출되지 않는다.
//void AMonster_Usurper::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
//{
//	UE_LOG(LogTemp, Log, TEXT("Binding?"));
//	Super::SetupPlayerInputComponent(PlayerInputComponent);
//	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent)) 
//	{
//		UE_LOG(LogTemp, Log, TEXT("Binding?"));
//		EnhancedInputComponent->BindAction(AttackAction, ETriggerEvent::Triggered, this, &AMonster_Usurper::TestTrigger);
//	}
//}

