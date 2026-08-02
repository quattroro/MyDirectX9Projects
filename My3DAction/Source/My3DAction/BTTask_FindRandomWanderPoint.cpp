// Fill out your copyright notice in the Description page of Project Settings.


#include "BTTask_FindRandomWanderPoint.h"
#include "AIController.h"
#include "NavigationSystem.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "BehaviorTree/BlackboardComponent.h"

UBTTask_FindRandomWanderPoint::UBTTask_FindRandomWanderPoint()
{
	NodeName = TEXT("Find Random Wander Point");

	HomeLocationKey.AddVectorFilter(this, GET_MEMBER_NAME_CHECKED(UBTTask_FindRandomWanderPoint, HomeLocationKey));
	WanderLocationKey.AddVectorFilter(this, GET_MEMBER_NAME_CHECKED(UBTTask_FindRandomWanderPoint, WanderLocationKey));
}

EBTNodeResult::Type UBTTask_FindRandomWanderPoint::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	UBlackboardComponent* BlackboardComp = OwnerComp.GetBlackboardComponent();
	if (!BlackboardComp)
	{
		return EBTNodeResult::Failed;
	}

	UNavigationSystemV1* NavSystem = UNavigationSystemV1::GetCurrent(GetWorld());
	if (!NavSystem)
	{
		return EBTNodeResult::Failed;
	}

	const FVector HomeLocation = BlackboardComp->GetValueAsVector(HomeLocationKey.SelectedKeyName);

	FNavLocation ResultLocation;
	if (NavSystem->GetRandomReachablePointInRadius(HomeLocation, WanderRadius, ResultLocation))
	{
		BlackboardComp->SetValueAsVector(WanderLocationKey.SelectedKeyName, ResultLocation.Location);
		return EBTNodeResult::Succeeded;
	}

	return EBTNodeResult::Failed;
}
