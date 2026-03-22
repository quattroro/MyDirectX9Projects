// Fill out your copyright notice in the Description page of Project Settings.


#include "HUDWidget.h"
#include "Components/ProgressBar.h"

// 해당 함수는 PlayerController에서 호출되어진다.
void UHUDWidget::UpdateHealthPercent(float HealthPercent)
{
	HealthBar->SetPercent(HealthPercent);
}
