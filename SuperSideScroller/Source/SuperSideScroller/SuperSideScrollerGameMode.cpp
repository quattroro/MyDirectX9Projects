// Copyright Epic Games, Inc. All Rights Reserved.

#include "SuperSideScrollerGameMode.h"
#include "SuperSideScrollerCharacter.h"
#include "UObject/ConstructorHelpers.h"

ASuperSideScrollerGameMode::ASuperSideScrollerGameMode()
{
	// set default pawn class to our Blueprinted character
	static ConstructorHelpers::FClassFinder<APawn> PlayerPawnBPClass(TEXT(/*"/Game/ThirdPerson/Blueprints/BP_ThirdPersonCharacter"*/"/Game/MainCharacter/Blueprints/BP_SuperSideScroller_MainCharacter"));
	if (PlayerPawnBPClass.Class != NULL)
	{
		DefaultPawnClass = PlayerPawnBPClass.Class;
	}
}
