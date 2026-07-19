// Fill out your copyright notice in the Description page of Project Settings.


#include "Monster_Usurper.h"
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

	UE_LOG(LogTemp, Log, TEXT("enter Hear?"));

	// APlayerController는 DefaultPawnClass로 설정되어있는 APawn을 PlayerController가 Possess해야 호출 가능하다 Player가 아닌 다른 APawn 객체들은
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

void AMonster_Usurper::Hit()
{
	if (DynamicMaterialInst)
	{
		float T = GetWorld()->GetTimeSeconds();
		DynamicMaterialInst->SetScalarParameterValue(TEXT("HitTime"), T);

		float Check;
		bool bFound = DynamicMaterialInst->GetScalarParameterValue(FName("HitTime"), Check);
		UE_LOG(LogTemp, Log, TEXT("HitTime set : found = %d, value = %f"), bFound, Check);
	}
}

void AMonster_Usurper::TestTrigger()
{
	UE_LOG(LogTemp, Log, TEXT("MonsterTrigger"));
	Hit();
}

// Called every frame
void AMonster_Usurper::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

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

