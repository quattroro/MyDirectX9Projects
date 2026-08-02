// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "MonsterAITypes.generated.h"

// 몬스터의 전투 상태. Blackboard의 CombatState 키(Enum)로 사용된다.
UENUM(BlueprintType)
enum class EMonsterCombatState : uint8
{
	Passive		UMETA(DisplayName = "Passive"),
	Alert		UMETA(DisplayName = "Alert"),		// 포효 전환 구간
	Combat		UMETA(DisplayName = "Combat"),
	Enrage		UMETA(DisplayName = "Enrage")
};
