// Fill out your copyright notice in the Description page of Project Settings.


#include "MonsterUIWidget.h"
#include "Components/ProgressBar.h"

void UMonsterUIWidget::SetHealthPercent(float percent)
{
    if (HealthProgressBar)
    {
        HealthProgressBar->SetPercent(FMath::Clamp(percent, 0.0, 1.0));
    }
}