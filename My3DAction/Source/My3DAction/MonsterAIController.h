// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "MonsterAIController.generated.h"

class UBehaviorTree;
class UAIPerceptionComponent;
class UAISenseConfig_Sight;
struct FAIStimulus;

UCLASS()
class MY3DACTION_API AMonsterAIController : public AAIController
{
	GENERATED_BODY()

public:
	AMonsterAIController();

protected:
	virtual void OnPossess(APawn* InPawn) override;

	UPROPERTY(EditDefaultsOnly, Category = "AI")
	UBehaviorTree* BehaviorTreeAsset;

	UPROPERTY(VisibleAnywhere, Category = "AI|Perception")
	UAIPerceptionComponent* AIPerceptionComp;

	UPROPERTY(EditDefaultsOnly, Category = "AI|Perception")
	UAISenseConfig_Sight* SightConfig;

	UFUNCTION()
	void OnTargetPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus);

public:
	// OnPossess 시점의 스폰 위치. Wander 반경의 기준점으로 쓰인다.
	UPROPERTY(BlueprintReadOnly, Category = "AI")
	FVector HomeLocation;
};
