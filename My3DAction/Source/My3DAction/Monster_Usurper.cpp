// Fill out your copyright notice in the Description page of Project Settings.


#include "Monster_Usurper.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"
#include "EngineUtils.h"

// Sets default values
AMonster_Usurper::AMonster_Usurper()
{
 	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	//
	// --- 회전 정책 ---
	// bUseControllerRotationYaw는 쓰지 않는다. APawn::FaceRotation이 보간 없이 SetActorRotation을
	// 호출해 컨트롤 로테이션으로 즉시 스냅해버리기 때문이다(RotationRate가 무시된다).
	// 대신 CMC의 bUseControllerDesiredRotation을 쓰면 PhysicsRotation()이 RotationRate로 보간한다.
	//
	// bOrientRotationToMovement는 꺼둔다. 진행 방향으로 몸을 돌려버리면
	// BTTask_MonsterRetreat의 "정면을 유지한 채 뒤로 물러나기"가 성립하지 않는다.
	// 후퇴 중 몸통이 타겟을 향하는 것은 AIController의 SetFocus(..., Gameplay)가 만든
	// ControlRotation을 CMC가 따라가면서 이루어진다.
	//
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	UCharacterMovementComponent* MoveComp = GetCharacterMovement();
	MoveComp->bOrientRotationToMovement = false;
	MoveComp->bUseControllerDesiredRotation = true;
	MoveComp->RotationRate = FRotator(0.f, 180.f, 0.f);
	MoveComp->MaxWalkSpeed = 400.f;

	// 루트 모션이 있는 몽타주를 재생하는 동안에는 회전을 잠가 공격 방향을 커밋시킨다.
	// 현재 Usurper 애니메이션 18종은 전부 Enable Root Motion이 꺼져 있어서 이 플래그는
	// 사실상 동작하지 않는다. 나중에 루트 모션 공격을 넣을 때를 위한 사전 설정이다.
	MoveComp->bAllowPhysicsRotationDuringAnimRootMotion = false;
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


	UWorld* World = GetWorld();
	if (!World) return;

	// 찾고자 하는 월드 아웃라이너 상의 이름 (정확히 일치해야 합니다)
	FString TargetName = TEXT("TestCube");
	//AActor* FoundActor = nullptr;

	// 월드의 모든 액터를 순회하는 이터레이터(TActorIterator) 사용
	for (TActorIterator<AActor> ActorItr(World); ActorItr; ++ActorItr)
	{
		AActor* Actor = *ActorItr;

		// 에디터 상의 이름(Actor Label)이 우리가 찾는 이름과 같은지 확인
		if (Actor && Actor->GetActorLabel() == TargetName)
		{
			TestBox = Actor;
		}
		else if (Actor && Actor->GetActorLabel() == TEXT("TestCube2"))
		{
			TestBox2 = Actor;
		}
		else if (Actor && Actor->GetActorLabel() == TEXT("TestCube3"))
		{
			TestBox3 = Actor;
		}
	}
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

	// LookAtBoneName = "Head" 초기값
	const FVector HeadWorldLocation = MeshComp->DoesSocketExist(LookAtBoneName)
		? MeshComp->GetSocketLocation(LookAtBoneName)
		: GetActorLocation();

	// GetActorRotation() -> AActor의 RootComponent의 현재 회전값을 리턴한다.
	FRotator ActorRotation = GetActorRotation();
	
	// 스켈레탈 메쉬의 회전값
	//USkeletalMeshComponent* MeshComp2 = FindComponentByClass<USkeletalMeshComponent>();
	if (MeshComp)
	{
		//ActorRotation = ActorRotation + MeshComp->GetComponentRotation();
		//UE_LOG(LogTemp, Log, TEXT("MeshRot = (%f, %f, %f)"), MeshComp->GetComponentRotation().Roll, MeshComp->GetComponentRotation().Pitch, MeshComp->GetComponentRotation().Yaw);
	}
	
	//UE_LOG(LogTemp, Log, TEXT("ActorRotation = (%f, %f, %f)"), GetActorRotation().Roll, GetActorRotation().Pitch, GetActorRotation().Yaw);

	// 타겟이 없을 때의 기본값: 정면을 본다(= Alpha가 0으로 빠지므로 실제로는 원본 포즈).
	FVector DesiredWorldLocation = HeadWorldLocation + ActorRotation.Vector() * 500.f;
	float DesiredAlpha = 0.f;

	AActor* Target = LookAtTarget.Get();
	if (Target && !IsDead())
	{
		// 타겟의 위치
		const FVector TargetWorldLocation = Target->GetActorLocation() + FVector(0.f, LookAtTargetZOffset, 0.0f);
		//  머리에서 타겟으로 향하는 방향
		const FVector ToTarget = TargetWorldLocation - HeadWorldLocation;

		/*if (TestBox3)
		{
			TestBox3->SetActorLocation(Target->GetActorLocation() + FVector(0.f, LookAtTargetZOffset, 0.0f));
		}*/

		if (!ToTarget.IsNearlyZero())
		{
			// 액터 정면 기준 상대 각도
			const FRotator DeltaRotation = (ToTarget.Rotation() - ActorRotation).GetNormalized();

			

			const FRotator ClampedDelta(
				FMath::Clamp(DeltaRotation.Pitch, -LookAtMaxPitch, LookAtMaxPitch),
				FMath::Clamp(DeltaRotation.Yaw, -LookAtMaxYaw, LookAtMaxYaw),
				0.f);

			FRotator ResulRotation = (ActorRotation + ClampedDelta);

			//UE_LOG(LogTemp, Log, TEXT("DeltaRotation = (%f, %f, %f)"), DeltaRotation.Roll, DeltaRotation.Pitch, DeltaRotation.Yaw);
			//UE_LOG(LogTemp, Log, TEXT("ClampedDelta = (%f, %f, %f)"), ClampedDelta.Roll, ClampedDelta.Pitch, ClampedDelta.Yaw);
			//UE_LOG(LogTemp, Log, TEXT("ResulRotation = (%f, %f, %f)"), ResulRotation.Roll, ResulRotation.Pitch, ResulRotation.Yaw);

			DesiredWorldLocation = HeadWorldLocation + (ActorRotation + ClampedDelta).Vector() * ToTarget.Size();

			// 등 뒤로 완전히 넘어가면 목을 한계각에 붙여두지 말고 정면으로 복귀시킨다.
			const float YawOvershoot = FMath::Abs(DeltaRotation.Yaw) - LookAtMaxYaw;
			DesiredAlpha = (LookAtYawFalloff > KINDA_SMALL_NUMBER)
				? 1.f - FMath::Clamp(YawOvershoot / LookAtYawFalloff, 0.f, 1.f)
				: (YawOvershoot > 0.f ? 0.f : 1.f);
		}
	}

	LookAtAlpha = FMath::FInterpTo(LookAtAlpha, DesiredAlpha, DeltaTime, LookAtBlendSpeed);
	// LookAtLocation은 어떻게 사용되는가.
	//LookAtLocation = MeshComp->GetComponentTransform().InverseTransformPosition(DesiredWorldLocation);

	LookAtLocation = DesiredWorldLocation;

	if (TestBox)
	{
		//TestBox->SetActorLocation(DesiredWorldLocation);
	}

	/*if (TestBox2)
	{
		TestBox2->SetActorLocation(HeadWorldLocation);
	}*/

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

