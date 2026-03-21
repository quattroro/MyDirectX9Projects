// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "RestartWidget.generated.h"

/**
 * 
 */
UCLASS()
class AITEST_API URestartWidget : public UUserWidget
{
	GENERATED_BODY()
	
	// 이 클래스를 장속하는 블루프린트 클래스 버튼에 이 속성을 연결하려면 
	// UPROPERTY 매크로를 사용해 BindWidget 메타 태그를 지정해야 한다.
	// 이렇게 하면 위젯 블루프린트가 RestartButton이라는 이름의 버튼을 가진다.
	// 또한 이 속성을 통해 c++에 접근해 위지, 크기 등의 속성을 자유롭게 위젯 블루프린트에서 편집할 수 있다.
	UPROPERTY(meta = (BindWidget, OptionalWidget = true))
	class UButton* RestartButton;
};
