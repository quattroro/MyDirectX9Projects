// Fill out your copyright notice in the Description page of Project Settings.


#include "MainCharacter.h"
#include "Engine/LocalPlayer.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/Controller.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"
#include "Animation/AnimInstance.h"
#include "Monster_Usurper.h"
#include "Kismet/GameplayStatics.h"

// Sets default values
AMainCharacter::AMainCharacter()
{
	// Set size for collision capsule
	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.0f);

	// Don't rotate when the controller rotates. Let that just affect the camera.
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	// Configure character movement
	GetCharacterMovement()->bOrientRotationToMovement = true; // Character moves in the direction of input...	
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 500.0f, 0.0f); // ...at this rotation rate

	// Note: For faster iteration times these variables, and many more, can be tweaked in the Character Blueprint
	// instead of recompiling to adjust them
	GetCharacterMovement()->JumpZVelocity = 700.f;
	GetCharacterMovement()->AirControl = 0.35f;
	GetCharacterMovement()->MaxWalkSpeed = 500.f;
	GetCharacterMovement()->MinAnalogWalkSpeed = 20.f;
	GetCharacterMovement()->BrakingDecelerationWalking = 2000.f;
	GetCharacterMovement()->BrakingDecelerationFalling = 1500.0f;

	// Create a camera boom (pulls in towards the player if there is a collision)
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 400.0f; // The camera follows at this distance behind the character	
	CameraBoom->bUsePawnControlRotation = true; // Rotate the arm based on the controller

	// Create a follow camera
	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName); // Attach the camera to the end of the boom and let the boom adjust to match the controller orientation
	FollowCamera->bUsePawnControlRotation = false; // Camera does not rotate relative to arm

	// Note: The skeletal mesh and anim blueprint references on the Mesh component (inherited from Character) 
	// are set in the derived blueprint asset named ThirdPersonCharacter (to avoid direct content references in C++)

}

// Called when the game starts or when spawned
void AMainCharacter::BeginPlay()
{
	// Call the base class  
	Super::BeginPlay();

	//Add Input Mapping Context
	if (APlayerController* PlayerController = Cast<APlayerController>(Controller))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PlayerController->GetLocalPlayer()))
		{
			Subsystem->AddMappingContext(DefaultMappingContext, 0);
		}
	}


	if (!TestMonster)
	{
		TestMonster = Cast<AMonster_Usurper>(UGameplayStatics::GetActorOfClass(GetWorld(), AMonster_Usurper::StaticClass()));
		UE_LOG(LogTemp, Log, TEXT("Find Monster"));
	}

	AttackCount = 0;
	IsBattleMode = false;
	Defence = false;



	// Sword컴포넌트를 이름으로 찾아 Overlap 충돌 이벤트를 바인딩
	TArray<UStaticMeshComponent*> MeshComps;
	GetComponents<UStaticMeshComponent>(MeshComps);
	for (UStaticMeshComponent* Comp : MeshComps)
	{
		if (Comp->GetName() == TEXT("Sword"))
		{
			SwordMeshComp = Comp;
			SwordMeshComp->OnComponentBeginOverlap.AddDynamic(this, &AMainCharacter::OnSwordBeinOverlap);
			break;
		}
	}
}


void AMainCharacter::EnableWeaponCollision()
{
	bWeaponCollisionEnable = true;
	bWeaponHasHitThisSwing = false;
}

void AMainCharacter::DisableWeaponCollision()
{
	bWeaponCollisionEnable = false;
}

void AMainCharacter::OnSwordBeinOverlap(UPrimitiveComponent* OverlappedComp, AActor* OthreActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (!bWeaponCollisionEnable || bWeaponHasHitThisSwing)
		return;


}

//////////////////////////////////////////////////////////////////////////
// Input

void AMainCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	// Set up action bindings
	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent)) {

		// Jumping
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Started, this, &ACharacter::Jump);
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, this, &ACharacter::StopJumping);

		// Moving
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AMainCharacter::Move);

		// Looking
		EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &AMainCharacter::Look);

		EnhancedInputComponent->BindAction(RunAction, ETriggerEvent::Started, this, &AMainCharacter::BeginRunning);
		EnhancedInputComponent->BindAction(RunAction, ETriggerEvent::Completed, this, &AMainCharacter::StopRunning);

		EnhancedInputComponent->BindAction(AttackAction, ETriggerEvent::Triggered, this, &AMainCharacter::Attack);


		EnhancedInputComponent->BindAction(DefenceAction, ETriggerEvent::Started, this, &AMainCharacter::BeginDefence);
		EnhancedInputComponent->BindAction(DefenceAction, ETriggerEvent::Completed, this, &AMainCharacter::StopDefence);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("'%s' Failed to find an Enhanced Input component! This template is built to use the Enhanced Input system. If you intend to use the legacy system, then you will need to update this C++ file."), *GetNameSafe(this));
	}
}

bool AMainCharacter::Attacking()
{
	bool bIsMontagePlaying = GetMesh()->GetAnimInstance()->Montage_IsPlaying(Attack1_Montage);
	bIsMontagePlaying |= GetMesh()->GetAnimInstance()->Montage_IsPlaying(Attack2_Montage);
	return bIsMontagePlaying;
}

void AMainCharacter::Attack()
{
	//UE_LOG(LogTemp, Log, TEXT("Attack %d"), AttackCount);

	if (Attacking())
		return;

	if (AttackCount == 0)
	{
		GetMesh()->GetAnimInstance()->Montage_Play(Attack1_Montage, 1.0f);
	}
	else if (AttackCount == 1)
	{
		GetMesh()->GetAnimInstance()->Montage_Play(Attack2_Montage, 1.0f);
	}
	
	AttackCount = (AttackCount + 1) % 2;

	AMonster_Usurper* monster = Cast<AMonster_Usurper>(TestMonster);
	if (monster)
	{
		UE_LOG(LogTemp, Log, TEXT("Player Monster Attack"));
		monster->Hit();
	}

	
}

void AMainCharacter::BeginRunning()
{
	if(!Defence)
		GetCharacterMovement()->MaxWalkSpeed = 800.f;
}

void AMainCharacter::StopRunning()
{
	if (!Defence)
		GetCharacterMovement()->MaxWalkSpeed = 500.f;
}

void AMainCharacter::BeginDefence()
{
	Defence = true;
	//GetCharacterMovement()->MaxWalkSpeed = 50.f;
	GetCharacterMovement()->bOrientRotationToMovement = false;
}
void AMainCharacter::StopDefence()
{
	Defence = false;
	//GetCharacterMovement()->MaxWalkSpeed = 500.f;
	GetCharacterMovement()->bOrientRotationToMovement = true;
}

void AMainCharacter::Move(const FInputActionValue& Value)
{
	// input is a Vector2D
	FVector2D MovementVector = Value.Get<FVector2D>();

	if (Controller != nullptr && !Defence && !Attacking())
	{
		// find out which way is forward
		const FRotator Rotation = Controller->GetControlRotation();
		const FRotator YawRotation(0, Rotation.Yaw, 0);

		// get forward vector
		const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);

		// get right vector 
		const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

		// add movement 
		AddMovementInput(ForwardDirection, MovementVector.Y);
		AddMovementInput(RightDirection, MovementVector.X);
	}
}

void AMainCharacter::Look(const FInputActionValue& Value)
{
	// input is a Vector2D
	FVector2D LookAxisVector = Value.Get<FVector2D>();

	if (Controller != nullptr)
	{
		// add yaw and pitch input to controller
		AddControllerYawInput(LookAxisVector.X);
		AddControllerPitchInput(LookAxisVector.Y);
	}
}

