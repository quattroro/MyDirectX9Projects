// Fill out your copyright notice in the Description page of Project Settings.


#include "MyThirdPersonChar.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"
#include "InputAction.h"
#include "Kismet/KismetMathLibrary.h"
#include "GameFramework/Controller.h"

// Sets default values
AMyThirdPersonChar::AMyThirdPersonChar()
{
 	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	// ĸ�� �ݸ����� ũ�⸦ �����Ѵ�.
	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.0f);

	// ��Ʈ�ѷ��� ȸ���� �� ĳ���ʹ� ȸ������ �ʵ��� �����Ѵ�.
	// ĳ���Ͱ� ī�޶� ������ �ֵ��� ���д�.
	bUseControllerRotationPitch = false;
	bUseControllerRotationRoll = false;
	bUseControllerRotationYaw = false;

	// ĳ���� �����Ʈ�� �����Ѵ�.
	GetCharacterMovement()->bOrientRotationToMovement = true;

	// ī�޶� ���� �����Ѵ�.(�浹�� �߻��� ��� �÷��̾� ������ �ٰ������� �����Ѵ�.)
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 300.0f;
	CameraBoom->bUsePawnControlRotation = true;

	// ĳ���͸� ����ٴ� ī�޶� �����Ѵ�.
	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;

}

// Called when the game starts or when spawned
void AMyThirdPersonChar::BeginPlay()
{
	Super::BeginPlay();
	
}

// Called every frame
void AMyThirdPersonChar::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

// Called to bind functionality to input
void AMyThirdPersonChar::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	//Super::SetupPlayerInputComponent(PlayerInputComponent);
	UEnhancedInputComponent* EnhancedPlayerInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	if (EnhancedPlayerInputComponent != nullptr)
	{
		APlayerController* PlayerController = Cast<APlayerController>(GetController());
		if (PlayerController != nullptr)
		{
			UEnhancedInputLocalPlayerSubsystem* EnhancedSubsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PlayerController->GetLocalPlayer());
			if (EnhancedSubsystem != nullptr)
			{
				EnhancedSubsystem->AddMappingContext(IC_Character, 1);
			}
		}

		EnhancedPlayerInputComponent->BindAction(IA_Move, ETriggerEvent::Triggered, this, &AMyThirdPersonChar::Move);
		EnhancedPlayerInputComponent->BindAction(IA_Jump, ETriggerEvent::Started, this, &AMyThirdPersonChar::Jump);
		EnhancedPlayerInputComponent->BindAction(IA_Jump, ETriggerEvent::Completed, this, &AMyThirdPersonChar::StopJumping);
		EnhancedPlayerInputComponent->BindAction(IA_Look, ETriggerEvent::Triggered, this, &AMyThirdPersonChar::Look);
		EnhancedPlayerInputComponent->BindAction(IA_Walk, ETriggerEvent::Started, this, &AMyThirdPersonChar::BeginWalking);
		EnhancedPlayerInputComponent->BindAction(IA_Walk, ETriggerEvent::Completed, this, &AMyThirdPersonChar::StopWalking);
	}

}

void AMyThirdPersonChar::Move(const FInputActionValue& Value)
{
	FVector2D InputValue = Value.Get<FVector2D>();
	if (Controller != nullptr && InputValue.X != 0.0f || InputValue.Y != 0.0f)
	{
		const FRotator YawRotation(0, Controller->GetControlRotation().Yaw, 0);
		if (InputValue.X != 0.0f)
		{
			const FVector RightDirection = UKismetMathLibrary::GetRightVector(YawRotation);
			AddMovementInput(RightDirection, InputValue.X);
		}
		if (InputValue.Y != 0.0f)
		{
			const FVector ForwardDirection = YawRotation.Vector();
			AddMovementInput(ForwardDirection, InputValue.Y);
		}
	}
}

void AMyThirdPersonChar::Look(const FInputActionValue& Value)
{
	FVector2D InputValue = Value.Get<FVector2D>();

	if (InputValue.X != 0.0f)
	{
		AddControllerYawInput(InputValue.X);
	}
	if (InputValue.Y != 0.0f)
	{
		AddControllerPitchInput(InputValue.Y);
	}
}

void AMyThirdPersonChar::BeginWalking()
{
	UE_LOG(LogTemp, Warning, TEXT("BeginWalking Called - IsInAir: %s"), GetCharacterMovement()->IsFalling() ? TEXT("True") : TEXT("False"));
	GetCharacterMovement()->MaxWalkSpeed *= 0.4;
}
void AMyThirdPersonChar::StopWalking()
{
	UE_LOG(LogTemp, Warning, TEXT("StopWalking Called - IsInAir: %s"), GetCharacterMovement()->IsFalling() ? TEXT("True") : TEXT("False"));
	GetCharacterMovement()->MaxWalkSpeed *= 2.5;
}