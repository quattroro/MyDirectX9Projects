// Fill out your copyright notice in the Description page of Project Settings.

#include "DodgeballProjectile.h"
#include "Components/SphereComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "AITestCharacter.h"

// Sets default values
ADodgeballProjectile::ADodgeballProjectile()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
	SphereComponent = CreateDefaultSubobject<USphereComponent>(TEXT("Sphere Sollision"));
	// 반지름 설정
	SphereComponent->SetSphereRadius(35.f);
	// Collision Preset을 미리 생성한 Dodgeball 프리셋으로 설정한다.
	SphereComponent->SetCollisionProfileName(FName("Dodgeball"));
	// 피직스 시물레이션 true
	SphereComponent->SetSimulatePhysics(true);
	// 시뮬레이션을 Hit 이벤트를 발생시킨다.
	SphereComponent->SetNotifyRigidBodyCollision(true);
	// OnComponentHit 이벤트 바인딩
	SphereComponent->OnComponentHit.AddDynamic(this, &ADodgeballProjectile::OnHit);

	RootComponent = SphereComponent;
}

// Called when the game starts or when spawned
void ADodgeballProjectile::BeginPlay()
{
	Super::BeginPlay();
	
}

// Called every frame
void ADodgeballProjectile::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

void ADodgeballProjectile::OnHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	if (Cast<AAITestCharacter>(OtherActor) != nullptr)
	{
		Destroy();
	}


}
