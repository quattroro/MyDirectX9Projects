// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/Decorators/BTDecorator_Loop.h"
#include "BTDecorator_RandomLoop.generated.h"

/**
 * 
 */
// 부모(UBTDecorator_Loop)의 NumLoops / bInfiniteLoop / InfiniteLoopTimeoutTime 은 이 노드에서
// 쓰이지 않으므로 Decorator 카테고리째로 숨긴다. 아래 두 프로퍼티는 별도 카테고리로 뺐다.
UCLASS(HideCategories = (Decorator))
class MY3DACTION_API UBTDecorator_RandomLoop : public UBTDecorator_Loop
{
	GENERATED_BODY()

public:
    UBTDecorator_RandomLoop();

    UPROPERTY(EditAnywhere, Category = "Random Loop", meta = (ClampMin = "1", ClampMax = "255"))
    int32 MinLoops = 1;

    UPROPERTY(EditAnywhere, Category = "Random Loop", meta = (ClampMin = "1", ClampMax = "255"))
    int32 MaxLoops = 5;

    virtual FString GetStaticDescription() const override;

protected:
    virtual void OnNodeActivation(FBehaviorTreeSearchData& SearchData) override;
};
