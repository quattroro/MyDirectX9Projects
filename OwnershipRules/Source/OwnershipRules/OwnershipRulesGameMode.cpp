// Copyright Epic Games, Inc. All Rights Reserved.

#include "OwnershipRulesGameMode.h"
#include "OwnershipRulesCharacter.h"
#include "UObject/ConstructorHelpers.h"

AOwnershipRulesGameMode::AOwnershipRulesGameMode()
{
	// set default pawn class to our Blueprinted character
	static ConstructorHelpers::FClassFinder<APawn> PlayerPawnBPClass(TEXT("/Game/ThirdPerson/Blueprints/BP_ThirdPersonCharacter"));
	if (PlayerPawnBPClass.Class != NULL)
	{
		DefaultPawnClass = PlayerPawnBPClass.Class;
	}
}
