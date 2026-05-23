// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "OwnershipTestActor.generated.h"

UCLASS()
class OWNERSHIPRULES_API AOwnershipTestActor : public AActor
{
	GENERATED_BODY()
	
protected:
	AOwnershipTestActor();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ownership Test Actor")
	UStaticMeshComponent* Mesh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ownership Test Actor")
	float OwnershipRadius = 400.0f;

	virtual void Tick(float DeltaTime) override;


};
