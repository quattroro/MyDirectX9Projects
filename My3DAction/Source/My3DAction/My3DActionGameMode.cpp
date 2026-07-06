// Copyright Epic Games, Inc. All Rights Reserved.

#include "My3DActionGameMode.h"
#include "My3DActionCharacter.h"
#include "UObject/ConstructorHelpers.h"

AMy3DActionGameMode::AMy3DActionGameMode()
{
	// set default pawn class to our Blueprinted character
	static ConstructorHelpers::FClassFinder<APawn> PlayerPawnBPClass(TEXT("/Game/ThirdPerson/Blueprints/BP_ThirdPersonCharacter"));
	if (PlayerPawnBPClass.Class != NULL)
	{
		DefaultPawnClass = PlayerPawnBPClass.Class;
	}
}
