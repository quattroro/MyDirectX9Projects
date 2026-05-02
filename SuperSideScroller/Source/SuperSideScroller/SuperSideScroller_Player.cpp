// Fill out your copyright notice in the Description page of Project Settings.


#include "SuperSideScroller_Player.h"
#include "Components/InputComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Animation/AnimInstance.h"
#include "PlayerProjectile.h"
#include "Engine/World.h"
#include "Components/SphereComponent.h"

ASuperSideScroller_Player::ASuperSideScroller_Player()
{
	bIsSprinting = false;
	GetCharacterMovement()->MaxWalkSpeed = 300.0f;
}

void ASuperSideScroller_Player::Sprint()
{
	if (!bIsSprinting)
	{
		bIsSprinting = true;
		GetCharacterMovement()->MaxWalkSpeed = 500.0f;
	}
}

void ASuperSideScroller_Player::StopSprinting()
{
	if (bIsSprinting)
	{
		bIsSprinting = false;
		GetCharacterMovement()->MaxWalkSpeed = 300.0f;
	}
}

void ASuperSideScroller_Player::TrowProjectile()
{
	if (ThrowMontage)
	{
		//UE_LOG(LogTemp, Warning, TEXT("마우스 클릭 들어옴"));
		// 이미 해당 몽타주가 재생중인지 확읺한다.
		bool bIsMontagePlaying = GetMesh()->GetAnimInstance()->Montage_IsPlaying(ThrowMontage);
		if (!bIsMontagePlaying)
		{
			// 1.0의 속도로 몽타주를 재생한다.
			GetMesh()->GetAnimInstance()->Montage_Play(ThrowMontage, 1.0f);
		}
	}
}

void ASuperSideScroller_Player::SpawnProjectile()
{
	if (PlayerProjectile)
	{
		UE_LOG(LogTemp, Warning, TEXT("Create Projectile1"));

		// 플레이어가 현재 존재하는 UWorld를 구하고 이 월드가 유효한지 확인한다.
		UWorld* World = GetWorld();
		if (World)
		{
			UE_LOG(LogTemp, Warning, TEXT("Create Projectile2"));
			FActorSpawnParameters SpawnParams;
			SpawnParams.Owner = this;

			// UWorld 객체에서 호출하는 SpawnActor()함수는 생성하는 객체의 초기화를 위해
			// FActorSpawnParameters 구조체를 필요로 한다.
			const FVector SpawnLocation = this->GetMesh()->GetSocketLocation(FName("ProjectileSocket"));
			const FRotator Rotation = GetActorForwardVector().Rotation();

			// World->SpawnActor 함수는 생성하려는 클래스의 객체를 반환한다.
			APlayerProjectile* Projectile = World->SpawnActor<APlayerProjectile>(PlayerProjectile, SpawnLocation, Rotation, SpawnParams);


		}
	}
}

void ASuperSideScroller_Player::SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	// 기존 입력과 향상된 입력 시스템을 모두 지원하도록 설정
	UEnhancedInputComponent* EnhancedPlayerInput = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	if (EnhancedPlayerInput)
	{
		APlayerController* PlayerController = Cast<APlayerController>(GetController());
		UEnhancedInputLocalPlayerSubsystem* EnhancedSubsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PlayerController->GetLocalPlayer());
		if (EnhancedSubsystem)
		{
			// IC_Character 입력 매핑 콘텍스트를 플레이어 컨트롤러에 추가해준다.
			EnhancedSubsystem->AddMappingContext(IC_Character, 1);

			// Sprint 입력 액션의 누름 이벤트를 Sprint 함수에 바인딩 한다.
			EnhancedPlayerInput->BindAction(IA_Sprint, ETriggerEvent::Triggered, this, &ASuperSideScroller_Player::Sprint);

			// Sprint 입력 액션의 입력 해제 이벤트를 StopSprinting 함수에 바인딩 한다.
			EnhancedPlayerInput->BindAction(IA_Sprint, ETriggerEvent::Completed, this, &ASuperSideScroller_Player::StopSprinting);

			// Sprint 입력 액션의 입력 해제 이벤트를 StopSprinting 함수에 바인딩 한다.
			EnhancedPlayerInput->BindAction(IA_Throw, ETriggerEvent::Triggered, this, &ASuperSideScroller_Player::TrowProjectile);
		}
	}
}