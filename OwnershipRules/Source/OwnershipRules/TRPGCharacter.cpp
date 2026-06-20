#include "TRPGCharacter.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/Controller.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"

ATRPGCharacter::ATRPGCharacter()
{
	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.0f);

	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(0.f, 500.f, 0.f);
	GetCharacterMovement()->JumpZVelocity = 600.f;
	GetCharacterMovement()->AirControl = 0.35f;
	GetCharacterMovement()->MaxWalkSpeed = 400.f;
	GetCharacterMovement()->BrakingDecelerationWalking = 2000.f;

	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 400.f;
	CameraBoom->bUsePawnControlRotation = true;

	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;

	PrimaryActorTick.bCanEverTick = false;
}

void ATRPGCharacter::BeginPlay()
{
	Super::BeginPlay();

	if (APlayerController* PC = Cast<APlayerController>(Controller))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem =
			ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
		{
			Subsystem->AddMappingContext(DefaultMappingContext, 0);
		}
	}
}

void ATRPGCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	if (UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		EIC->BindAction(MoveAction,   ETriggerEvent::Triggered, this, &ATRPGCharacter::Move);
		EIC->BindAction(LookAction,   ETriggerEvent::Triggered, this, &ATRPGCharacter::Look);
		EIC->BindAction(JumpAction,   ETriggerEvent::Started,   this, &ACharacter::Jump);
		EIC->BindAction(JumpAction,   ETriggerEvent::Completed, this, &ACharacter::StopJumping);
		EIC->BindAction(AttackAction, ETriggerEvent::Started,   this, &ATRPGCharacter::StartAttack);
		EIC->BindAction(DefendAction, ETriggerEvent::Started,   this, &ATRPGCharacter::StartDefend);
		EIC->BindAction(DefendAction, ETriggerEvent::Completed, this, &ATRPGCharacter::StopDefend);
	}
}

void ATRPGCharacter::Move(const FInputActionValue& Value)
{
	FVector2D Input = Value.Get<FVector2D>();
	if (Controller)
	{
		const FRotator Yaw(0.f, Controller->GetControlRotation().Yaw, 0.f);
		AddMovementInput(FRotationMatrix(Yaw).GetUnitAxis(EAxis::X), Input.Y);
		AddMovementInput(FRotationMatrix(Yaw).GetUnitAxis(EAxis::Y), Input.X);
	}
}

void ATRPGCharacter::Look(const FInputActionValue& Value)
{
	FVector2D Input = Value.Get<FVector2D>();
	if (Controller)
	{
		AddControllerYawInput(Input.X);
		AddControllerPitchInput(Input.Y);
	}
}

void ATRPGCharacter::StartAttack()
{
	// Blueprint에서 이벤트로 확장 가능
	UE_LOG(LogTemp, Log, TEXT("TRPG Attack!"));
}

void ATRPGCharacter::StartDefend()
{
	bIsDefending = true;
	UE_LOG(LogTemp, Log, TEXT("TRPG Defend Start"));
}

void ATRPGCharacter::StopDefend()
{
	bIsDefending = false;
	UE_LOG(LogTemp, Log, TEXT("TRPG Defend Stop"));
}
