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


UENUM(BlueprintType)
enum class EMonsterAttackType : uint8
{
	AK_None		UMETA(DisplayName = "AK_None"),
	AK_Mouth	UMETA(DisplayName = "AK_Mouth"),	// 깨물기 공격
	AK_Crow		UMETA(DisplayName = "AK_Crow"),		// 대쉬 할퀴기
	AK_Flame	UMETA(DisplayName = "AK_Flame")		// 불 뿜기
};

// 후퇴 이동 방식. BTTask_MonsterRetreat의 MoveMode 프로퍼티로 사용된다.
UENUM(BlueprintType)
enum class ERetreatMoveMode : uint8
{
	Backstep	UMETA(DisplayName = "Backstep"),	// 네비메시를 따라 뒤로 걸어서 물러난다
	BackJump	UMETA(DisplayName = "BackJump")		// LaunchCharacter 임펄스로 뒤로 도약한다
};
