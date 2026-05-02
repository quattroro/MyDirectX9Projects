// Fill out your copyright notice in the Description page of Project Settings.


#include "PlayerProjectile.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"

// Sets default values
APlayerProjectile::APlayerProjectile()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	//PrimaryActorTick.bCanEverTick = true;

	{
		// 구체 콜리전 생성
		CollisionComp = CreateDefaultSubobject<USphereComponent>(TEXT("SphereComp"));
		// 반지름 설정
		CollisionComp->InitSphereRadius(15.0f);
		// 프로파일 설정, 프로파일을 "BlockAll"로 설정하면 이 콜리전 컴포넌트는 다른 액터와 충돌할 때 OnHit에 반응한다.
		CollisionComp->BodyInstance.SetCollisionProfileName("BlockAll");
		// OnHit 바인딩
		CollisionComp->OnComponentHit.AddDynamic(this, &APlayerProjectile::OnHit);
		// 루트 컴포넌트로 설정
		RootComponent = CollisionComp;
	}

	{
		// 무브먼트 컴포넌트 생성
		ProjectileMovement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("ProjectileComp"));
		// 이렇게 설정하는 이유는 프로젝타일 무브먼트 컴포넌트가 이동을 위해 액터의 루트 컴포넌트를 사용하기 때문이다.
		ProjectileMovement->UpdatedComponent = CollisionComp;
		// 중력 설정
		ProjectileMovement->ProjectileGravityScale = 0.0f;
		// 속도 설정
		ProjectileMovement->InitialSpeed = 800.0f;
		// 최대 속도 설정
		ProjectileMovement->MaxSpeed = 800.0f;
	}

	{

		MeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComp"));
		// 생성한 메시 컴포넌트를 RootComponent에 부착한다.
		// 이때 스태틱 메시 컴포넌트가 CollisionComp에 붙어 있을 때 월드 트랜스폼을 유지하도록 FAttachmentTransformRules구조체를 호출한다.
		MeshComp->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepWorldTransform);
	}

	InitialLifeSpan = 3.0f;
}

//// Called when the game starts or when spawned
//void APlayerProjectile::BeginPlay()
//{
//	Super::BeginPlay();
//	
//}
//
//// Called every frame
//void APlayerProjectile::Tick(float DeltaTime)
//{
//	Super::Tick(DeltaTime);
//
//}

void APlayerProjectile::OnHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	UE_LOG(LogTemp, Warning, TEXT(""));
}