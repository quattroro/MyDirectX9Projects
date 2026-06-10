// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#define ROLE_TO_STRING(Value) FindObject<UEnum>(ANY_PACKAGE, TEXT("ENetRole"), true)->GetNameStringByIndex(static_cast<int32>(Value))
#define ENUM_TO_INT32(Value) static_cast<int32>(Value)
#define ENUM_TO_FSTRING(Enum, Value) FindObject<UEnum>(ANY_PACKAGE, TEXT(Enum), true)->GetDisplayNameTextByIndex((int32)Value).ToString()

UENUM(BlueprintType)
enum class EWeaponType : uint8
{
	Pistol UMETA(DisplayName = "Glock 19"),
	Shotgun UMETA(DisplayName = "Winchester M1897"),
	RocketLauncher UMETA(DisplayName = "RPG"),
	MAX
};


UENUM(BlueprintType)
enum class EAmmoType : uint8
{
	Bullets UMETA(DisplayName = "9mm Bullets"),
	Shells UMETA(DisplayName = "12 Gauge Shotgun Shells"),
	Rockets UMETA(DisplayName = "RPG Rockets"),
	MAX
};