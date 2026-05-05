// Fill out your copyright notice in the Description page of Project Settings.


#include "PickableActor_Base.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/RotatingMovementComponent.h"
#include "SuperSideScroller_Player.h"
#include "Kismet/GameplayStatics.h"

// Sets default values
APickableActor_Base::APickableActor_Base()
{
	{
		CollisionComp = CreateDefaultSubobject<USphereComponent>(TEXT("SphereComp"));
		CollisionComp->InitSphereRadius(30.0f);
		// 플레이어 캐릭터는 이 컴포넌트와의 오버랩이 필요하므로 
		// USphereComponent는 Overlap All Dynamic에 대한 콜리전 설정을 제공하도록 한다.
		CollisionComp->BodyInstance.SetCollisionProfileName("OverlapAllDynamic");

		RootComponent = CollisionComp;
		
	}

	{
		MeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComp"));
		// MeshComp를 루트 컴포넌트인 CollisionComp에 연결한다.
		MeshComp->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepWorldTransform);
		// MeshComp UStaticMeshComponent는 기본적으로 충돌이 발생하면 안 되기 때문에 이를 위해 NoCollision으로 설정해준다.
		MeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	{
		RotationComp = CreateDefaultSubobject<URotatingMovementComponent>(TEXT("RotationComp"));
	}
}

// Called when the game starts or when spawned
void APickableActor_Base::BeginPlay()
{
	Super::BeginPlay();
	

	CollisionComp->OnComponentBeginOverlap.AddDynamic(this, &APickableActor_Base::BeginOverlap);
}

void APickableActor_Base::PlayerPickedUp(ASuperSideScroller_Player* Player)
{
	UE_LOG(LogTemp, Warning, TEXT("EnterPickedUp"));
	const UWorld* World = GetWorld();
	if (World)
	{
		if (PickupSound)
		{
			UE_LOG(LogTemp, Warning, TEXT("SoundPlay"));
			UGameplayStatics::SpawnSoundAtLocation(World, PickupSound, GetActorLocation());
		}
	}

	Destroy();
}

void APickableActor_Base::BeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	UE_LOG(LogTemp, Warning, TEXT("BeginOverlap"));
	ASuperSideScroller_Player* Player = Cast<ASuperSideScroller_Player>(OtherActor);
	if (Player)
	{
		UE_LOG(LogTemp, Warning, TEXT("BeginOverlap_FindPlayer"));
		PlayerPickedUp(Player);
	}
}


