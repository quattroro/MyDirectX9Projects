// Fill out your copyright notice in the Description page of Project Settings.

#include "EnemyCharacter.h"
#include "Engine/World.h"

#include "DrawDebugHelpers.h"
#include "Kismet/KismetMathLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "DodgeballProjectile.h"
//#include "Components/ProjectileMovementComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "DodgeballFunctionLibrary.h"


// Sets default values
AEnemyCharacter::AEnemyCharacter()
{
 	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
	SightSource = CreateDefaultSubobject<USceneComponent>(TEXT("SightSource"));
	SightSource->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepWorldTransform);
}

// Called when the game starts or when spawned
void AEnemyCharacter::BeginPlay()
{
	Super::BeginPlay();
	bCanSeePlayer = false;
	bPreviousCanSeePlayer = false;
}

// Called every frame
void AEnemyCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	ACharacter* PlayerCharacter = UGameplayStatics::GetPlayerCharacter(this, 0);

	bCanSeePlayer = LookAtActor(PlayerCharacter);
	

	if (bCanSeePlayer != bPreviousCanSeePlayer)
	{
		if (bCanSeePlayer)
		{
			UE_LOG(LogTemp, Warning, TEXT("Start Timer"));
			// 닷지볼 던지기를 시작한다.
			GetWorldTimerManager().SetTimer(ThrowTimerHandle, this, &AEnemyCharacter::ThrowDodgeball, ThrowingInterval, true, ThrowingDelay);
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("Stop Timer"));
			// 닷지볼 던지기를 멈춘다.
			GetWorldTimerManager().ClearTimer(ThrowTimerHandle);
		}
	}
	bPreviousCanSeePlayer = bCanSeePlayer;
}

// Called to bind functionality to input
void AEnemyCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

}

void AEnemyCharacter::ThrowDodgeball()
{
	if (DodgeballClass == nullptr)
	{
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("Throw"));
	FVector ForwardVector = GetActorForwardVector();
	float SpawnDistance = 40.f;
	FVector SpawnLocation = GetActorLocation() + (ForwardVector * SpawnDistance);
	FRotator SpawnRotation = GetActorRotation();


	FTransform SpawnTransform = FTransform(SpawnRotation, SpawnLocation);

	// 새 닷지볼 스폰하기
	//GetWorld()->SpawnActor<ADodgeballProjectile>(DodgeballClass, SpawnLocation, GetActorRotation());

	ADodgeballProjectile* Projectile = GetWorld()->SpawnActorDeferred<ADodgeballProjectile>(DodgeballClass, SpawnTransform);
	Projectile->GetProjectileMovementComponent()->InitialSpeed = 2200;
	//FinishSpawning(SpawnTransform);
	Projectile->FinishSpawning(SpawnTransform);
}


//bool AEnemyCharacter::CanSeeActor(const AActor* TargetActor) const
//{
//	if (TargetActor == nullptr)
//	{
//		return false;
//	}
//	
//	FHitResult Hit;
//	FVector start = SightSource->GetComponentLocation()/*GetActorLocation()*/;
//	FVector end = TargetActor->GetActorLocation();
//
//	ECollisionChannel Channel = ECollisionChannel::/*ECC_Visibility*/ECC_GameTraceChannel1;
//
//	FCollisionQueryParams QueryParams;
//	QueryParams.AddIgnoredActor(this);
//	QueryParams.AddIgnoredActor(TargetActor);
//
//	GetWorld()->LineTraceSingleByChannel(Hit, start, end, Channel, QueryParams);
//	//GetWorld()->LineTraceMultiByChannel()
//
//	DrawDebugLine(GetWorld(), start, end, FColor::Red);
//
//	//
//	//FQuat Rotation = FQuat::Identity;
//	//FCollisionShape Shape = FCollisionShape::MakeBox(FVector(20.0f, 20.0f, 20.0f));
//	//GetWorld()->SweepSingleByChannel(Hit, start, end, Rotation, Channel, Shape, QueryParams);
//
//
//	return !Hit.bBlockingHit;
//}

bool AEnemyCharacter::LookAtActor(AActor* TargetActor)
{
	if (TargetActor == nullptr)
		return false;


	const TArray<const AActor*> IgnoreActors = { this, TargetActor };
	if (/*CanSeeActor(TargetActor)*/UDodgeballFunctionLibrary::CanSeeActor(GetWorld(), SightSource->GetComponentLocation(), TargetActor, IgnoreActors))
	{
		FVector Start = GetActorLocation();
		FVector End = TargetActor->GetActorLocation();

		FRotator LookAtRotation = UKismetMathLibrary::FindLookAtRotation(Start, End);

		SetActorRotation(LookAtRotation);

		return true;
	}

	return false;
}