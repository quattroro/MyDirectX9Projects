// Fill out your copyright notice in the Description page of Project Settings.


#include "RestartWidget.h"
#include "Kismet/GameplayStatics.h"
#include "Components/Button.h"
#include "DodgeballPlayerController.h"
void URestartWidget::OnRestartClicked()
{
	ADodgeballPlayerController* PlayerController = Cast<ADodgeballPlayerController>(GetOwningPlayer());
	if (PlayerController != nullptr)
	{
		PlayerController->HideRestartWidget();
	}
	UGameplayStatics::OpenLevel(this, FName(*UGameplayStatics::GetCurrentLevelName(this)));

}


void  URestartWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	if (RestartButton != nullptr)
	{
		// AddDynamic 함수를 호출할 때는 UFUNCTION 매크로로 표시된 함수의 포인터를 파라미터로
		// 전달해야 한다. 전달하는 함수를 UFUNCTION 매크로로 표시하지 않으면, 함수를 호출할 때 오류가 발생한다.
		RestartButton->OnClicked.AddDynamic(this, &URestartWidget::OnRestartClicked);

	}
}