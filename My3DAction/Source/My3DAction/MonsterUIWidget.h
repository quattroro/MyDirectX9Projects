// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MonsterUIWidget.generated.h"


class UProgressBar;

UCLASS()
class MY3DACTION_API UMonsterUIWidget : public UUserWidget
{
	GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category = "Health")
    void SetHealthPercent(float percent);


protected:
    UPROPERTY(meta = (BindWidget)) // 언리얼 엔진 C++에서 UMG(디자이너 화면)에서 만든 UI 위젯과 C++ 코드를 이름 기준으로 서로 연결(바인딩)해주는 매크로 속성
    TObjectPtr<UProgressBar> HealthProgressBar;
};
