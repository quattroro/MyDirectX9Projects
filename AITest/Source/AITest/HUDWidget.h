// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "HUDWidget.generated.h"

/**
 * 
 */
UCLASS()
class AITEST_API UHUDWidget : public UUserWidget
{
	GENERATED_BODY()
	
	UPROPERTY(meta = (BindWidget))
	class UProgressBar* HealthBar;

public:
	// 프로그레스 바의 Percennt 속성을 업데이트하기 위해 호출
	void UpdateHealthPercent(float HealthPercent);
};
