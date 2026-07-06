// Fill out your copyright notice in the Description page of Project Settings.


#include "Player_3DAction.h"

// Sets default values
APlayer_3DAction::APlayer_3DAction()
{
 	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

}

// Called when the game starts or when spawned
void APlayer_3DAction::BeginPlay()
{
	Super::BeginPlay();
	
}

// Called every frame
void APlayer_3DAction::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

// Called to bind functionality to input
void APlayer_3DAction::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

}

