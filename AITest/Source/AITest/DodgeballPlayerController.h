// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "DodgeballPlayerController.generated.h"

/**
 * 
 */
UCLASS()
class AITEST_API ADodgeballPlayerController : public APlayerController
{
	GENERATED_BODY()
public:
	UPROPERTY(EditDefaultsOnly)
	TSubclassOf<class URestartWidget> BP_RestartWidget;

private:
	// 이 변수는 블루프린트 클래스에서 편집할 수 없지만 UPROPERTY로 지정해야 한다.
	// 이렇게 하지 안으면 가비지 컬렉터가 이 변수의 콘텐츠를 삭제해버린다.
	UPROPERTY()
	class URestartWidget* RestartWidget;
public:
	void ShowRestartWidget();
	void HideRestartWidget();

	void UpdateHealthPercent(float HealthPercent);

	UPROPERTY(EditDefaultsOnly)
	TSubclassOf<class UHUDWidget> BP_HUDWidget;
private:
	UPROPERTY()
	class UHUDWidget* HUDWidget;

protected:
	virtual void BeginPlay() override;
};
