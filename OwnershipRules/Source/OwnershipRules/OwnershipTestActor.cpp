// Fill out your copyright notice in the Description page of Project Settings.


#include "OwnershipTestActor.h"
#include "OwnershipRules.h"
#include "OwnershipRulesCharacter.h"
#include "Kismet/GameplayStatics.h"

AOwnershipTestActor::AOwnershipTestActor()
{
	PrimaryActorTick.bCanEverTick = true;
	Mesh = CreateDefaultSubobject<UStaticMeshComponent>("Mesh");
	RootComponent = Mesh;

	bReplicates = true;

}


void AOwnershipTestActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	DrawDebugSphere(GetWorld(), GetActorLocation(), OwnershipRadius, 32, FColor::Yellow);

	if (HasAuthority())
	{
		AActor* NextOwner = nullptr;
		float MinDistance = OwnershipRadius;
		TArray<AActor*> Actors;
		UGameplayStatics::GetAllActorsOfClass(this, AOwnershipRulesCharacter::StaticClass(), Actors);

		for (AActor* Actor : Actors)
		{
			const float Distance = GetDistanceTo(Actor);
			if (Distance <= MinDistance)
			{
				MinDistance = Distance;
				NextOwner = Actor;
			}
		}
		if (GetOwner() != NextOwner)
		{
			SetOwner(NextOwner);
		}
	}

	const FString LocalRoleString = ROLE_TO_STRING(GetLocalRole());
	const FString RomoteRoleString = ROLE_TO_STRING(GetRemoteRole());
	const FString OwnerString = GetOwner() != nullptr ? GetOwner()->GetName() : TEXT("No Owner");
	const FString ConnectionString = GetNetConnection() != nullptr ? TEXT("Valid Connection") : TEXT("Invalid Connection");
	const FString Values = FString::Printf(TEXT("LocalRole = %s\nRemoteRole = %s\nOwner = %s\nConnection = %s"), *LocalRoleString, *RomoteRoleString, *OwnerString, *ConnectionString);
	DrawDebugString(GetWorld(), GetActorLocation(), Values, nullptr, FColor::White, 0.0f, true);
}

