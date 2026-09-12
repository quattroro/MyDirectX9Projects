// Fill out your copyright notice in the Description page of Project Settings.


#include "BTDecorator_RandomLoop.h"
#include "Engine/World.h"
#include "BehaviorTree/BTCompositeNode.h"
#include "BehaviorTree/Composites/BTComposite_SimpleParallel.h"

UBTDecorator_RandomLoop::UBTDecorator_RandomLoop()
{
    NodeName = TEXT("Random Loop");
    bInfiniteLoop = false;
}

void UBTDecorator_RandomLoop::OnNodeActivation(FBehaviorTreeSearchData& SearchData)
{
    FBTLoopDecoratorMemory* DecoratorMemory = GetNodeMemory<FBTLoopDecoratorMemory>(SearchData);
    FBTCompositeMemory* ParentMemory = GetParentNode()->GetNodeMemory<FBTCompositeMemory>(SearchData);
    const bool bIsSpecialNode = GetParentNode()->IsA(UBTComposite_SimpleParallel::StaticClass());

    // 사이클 시작 시점에만 새로 굴린다. 부모의 재장전 가드가 RemainingExecutions를 NumLoops로
    // 덮어쓰므로 Super를 부르지 않고, 부모와 동일한 첫 활성화 판정을 여기서 그대로 쓴다.
    if ((bIsSpecialNode && ParentMemory->CurrentChild == BTSpecialChild::NotInitialized) ||
        (!bIsSpecialNode && ParentMemory->CurrentChild != ChildIndex))
    {
        const int32 Rolled = FMath::RandRange(FMath::Min(MinLoops, MaxLoops),
            FMath::Max(MinLoops, MaxLoops));
        DecoratorMemory->RemainingExecutions = (uint8)FMath::Clamp(Rolled, 1, 255);
        DecoratorMemory->TimeStarted = GetWorld()->GetTimeSeconds();
        UE_LOG(LogTemp, Log, TEXT("Loop Count = %d"), Rolled);
    }


    bool bShouldLoop = false;
    if (bInfiniteLoop)
    {
        // protect from truly infinite loop within single search
        if (SearchData.SearchId != DecoratorMemory->SearchId)
        {
            if ((InfiniteLoopTimeoutTime < 0.f) || ((DecoratorMemory->TimeStarted + InfiniteLoopTimeoutTime) > GetWorld()->GetTimeSeconds()))
            {
                bShouldLoop = true;
            }
        }

        DecoratorMemory->SearchId = SearchData.SearchId;
    }
    else
    {
        if (DecoratorMemory->RemainingExecutions > 0)
        {
            DecoratorMemory->RemainingExecutions--;
        }
        bShouldLoop = DecoratorMemory->RemainingExecutions > 0;
    }


    // set child selection overrides
    if (bShouldLoop)
    {
        GetParentNode()->SetChildOverride(SearchData, ChildIndex);
    }

    //// 부모와 동일한 소모/루프백 처리.
    //if (Mem->RemainingExecutions > 0)
    //{
    //    Mem->RemainingExecutions--;
    //}

    //if (Mem->RemainingExecutions > 0)
    //{
    //    GetParentNode()->SetChildOverride(SearchData, ChildIndex);
    //}
}

FString UBTDecorator_RandomLoop::GetStaticDescription() const
{
    return FString::Printf(TEXT("loop %d~%d times"), MinLoops, MaxLoops);
}
