// Fill out your copyright notice in the Description page of Project Settings.


#include "DodgeballPlayerController.h"
#include "RestartWidget.h"
#include "HUDWidget.h"

void ADodgeballPlayerController::BeginPlay()
{
	Super::BeginPlay();
	if (BP_HUDWidget != nullptr)
	{
		// 왜 이렇게 하는거지???
		HUDWidget = CreateWidget<UHUDWidget>(this, BP_HUDWidget);
	}
}


void ADodgeballPlayerController::ShowRestartWidget()
{
	if (BP_RestartWidget != nullptr)
	{
		// 게임을 정지하고
		SetPause(true);
		// 입력 모드를 UIOnly로 해준다.
		SetInputMode(FInputModeUIOnly());
		// 마우스 커서가 보이도록 해준다.
		bShowMouseCursor = true;
		// 플레이어 컨트롤러의 CreateWidget 함수를 사용해 위젯을 실제로 생성한다.
		RestartWidget = CreateWidget<URestartWidget>(this, BP_RestartWidget);
		// 위젯을 생성하고 해당 위젯을 AddToViewport함수를 사용해 화면에 추가한다.
		RestartWidget->AddToViewport();
	}
}

void ADodgeballPlayerController::HideRestartWidget()
{
	// 화면에 추가된 위젯을 제거한다.
	RestartWidget->RemoveFromParent();
	// 위젯 삭제
	RestartWidget->Destruct();
	// 일시 정지 해제
	SetPause(false);
	// 입력 모드를 GameOnly로 해준다.
	SetInputMode(FInputModeGameOnly());
	// 마우스 커서를 숨긴다.
	bShowMouseCursor = false;
}


void ADodgeballPlayerController::UpdateHealthPercent(float HealthPercent)
{

}