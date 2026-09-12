// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "BehaviorTree/BTTaskNode.h"
#include "Navigation/PathFollowingComponent.h"
#include "MonsterAITypes.h"
#include "BTTask_MonsterRetreat.generated.h"

class AAIController;
class ACharacter;
class UAnimInstance;
class UAnimMontage;

//
// 타겟을 정면으로 바라본 채 뒤로 물러나는 태스크.
//
// AIController::MoveTo만 쓰면 PathFollowingComponent가 매 프레임
// SetFocalPoint(이동 방향, EAIFocusPriority::Move)를 호출하기 때문에 몬스터가 후퇴 방향으로
// 몸을 돌려버린다. 여기서는 SetFocus(Target, EAIFocusPriority::Gameplay)로 더 높은 우선순위의
// 포커스를 덮어씌워 몸통은 타겟을 향한 채로 이동만 뒤로 시킨다.
//
// MoveMode로 두 가지 표현을 고를 수 있다.
//  - Backstep : 네비메시로 검증된 후퇴 지점까지 뒷걸음질
//  - BackJump : LaunchCharacter 임펄스로 짧게 뒤로 도약
//
UCLASS()
class MY3DACTION_API UBTTask_MonsterRetreat : public UBTTaskNode
{
	GENERATED_BODY()

public:
	UBTTask_MonsterRetreat();

	// 에셋 로드 시 블랙보드 키 셀렉터를 실제 블랙보드 키에 연결한다.
	virtual void InitializeFromAsset(UBehaviorTree& Asset) override;

protected:
	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual void TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;
	// 상위 노드가 이 태스크를 중단시킬 때 호출. 포커스/이동/몽타주/이동속도를 모두 되돌린다.
	virtual EBTNodeResult::Type AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;

	// 어디에서 멀어질지를 담은 블랙보드 키. TargetActor 같은 Object(AActor) 키를 쓴다.
	UPROPERTY(EditAnywhere, Category = "Blackboard")
	FBlackboardKeySelector TargetKey;

	// 후퇴 이동 방식. Backstep = 뒷걸음질, BackJump = 백점프.
	UPROPERTY(EditAnywhere, Category = "Retreat")
	ERetreatMoveMode MoveMode = ERetreatMoveMode::Backstep;

	// 타겟 반대 방향으로 물러날 거리(cm).
	UPROPERTY(EditAnywhere, Category = "Retreat", meta = (ClampMin = "50.0"))
	float RetreatDistance = 400.f;

	// 네비메시에 막혀 이 거리보다 짧게밖에 못 물러나면 후퇴를 포기하고 Failed를 반환한다.
	UPROPERTY(EditAnywhere, Category = "Retreat", meta = (ClampMin = "0.0"))
	float MinRetreatDistance = 120.f;

	// 후퇴 방향의 좌우 랜덤 산포(도). 0이면 항상 정확히 타겟 반대편으로 물러난다.
	UPROPERTY(EditAnywhere, Category = "Retreat", meta = (ClampMin = "0.0", ClampMax = "60.0"))
	float RetreatSpreadAngle = 15.f;

	// 안전 타임아웃(초). 이동/착지 알림이 오지 않아도 이 시간이 지나면 태스크를 끝낸다.
	UPROPERTY(EditAnywhere, Category = "Retreat", meta = (ClampMin = "0.1"))
	float MaxRetreatTime = 3.f;

	// 목표 도달 판정 반경(cm).
	UPROPERTY(EditAnywhere, Category = "Retreat|Backstep")
	float AcceptanceRadius = 40.f;

	// 짧은 후퇴는 직선 이동이 자연스럽다. true로 두면 경로 탐색을 거치느라
	// 몬스터가 앞으로 우회해서 돌아가는 경우가 생긴다.
	UPROPERTY(EditAnywhere, Category = "Retreat|Backstep")
	bool bUsePathfinding = false;

	// 후퇴 지점을 네비메시에 투영할 때 쓰는 검색 범위.
	UPROPERTY(EditAnywhere, Category = "Retreat|Backstep")
	FVector NavProjectExtent = FVector(200.f, 200.f, 300.f);

	// 후퇴 지점을 못 찾았을 때 거리를 절반씩 줄여가며 재시도할 횟수.
	UPROPERTY(EditAnywhere, Category = "Retreat|Backstep", meta = (ClampMin = "0"))
	int32 NavRetryCount = 3;

	// 후퇴하는 동안 적용할 MaxWalkSpeed. 0이면 캐릭터 기본값을 그대로 쓴다.
	// 태스크가 끝나면 원래 값으로 되돌린다.
	UPROPERTY(EditAnywhere, Category = "Retreat|Backstep", meta = (ClampMin = "0.0"))
	float RetreatWalkSpeed = 220.f;

	// 백점프의 수평 속도(cm/s).
	UPROPERTY(EditAnywhere, Category = "Retreat|BackJump", meta = (ClampMin = "0.0"))
	float LaunchXYSpeed = 700.f;

	// 백점프의 수직 속도(cm/s). 체공 시간을 결정한다.
	UPROPERTY(EditAnywhere, Category = "Retreat|BackJump", meta = (ClampMin = "1.0"))
	float LaunchZSpeed = 450.f;

	UPROPERTY(EditAnywhere, Category = "Animation")
	TObjectPtr<UAnimMontage> RetreatMontage;

	// 음수로 두면 역재생한다. (뒤로 걷는 애니가 없을 때 WalkAnim 몽타주를 재활용하는 용도)
	UPROPERTY(EditAnywhere, Category = "Animation")
	float PlayRate = 1.f;

	// 후퇴 몽타주의 루트 모션을 이 재생 한 번에 한해 끈다.
	// 루트 모션이 켜져 있으면 CharacterMovement가 Velocity를 애니메이션 값으로 통째로 덮어써서
	// MoveTo도 LaunchCharacter도 전부 무시된다.
	// 현재 Usurper 애니메이션은 전부 루트 모션이 꺼져 있어 실질적으로는 안전장치지만,
	// 루트 모션이 있는 애니로 몽타주를 갈아끼웠을 때 조용히 망가지는 것을 막아준다.
	UPROPERTY(EditAnywhere, Category = "Animation")
	bool bDisableMontageRootMotion = true;

	// true면 이동/착지가 끝나도 몽타주 재생이 끝날 때까지 태스크를 유지한다.
	// Backstep 몽타주는 보통 루프라서 false로 둔다.
	UPROPERTY(EditAnywhere, Category = "Animation")
	bool bWaitForMontage = false;

	UPROPERTY(EditAnywhere, Category = "Animation", meta = (ClampMin = "0.0"))
	float MontageBlendOutTime = 0.2f;

private:
	// 타겟 반대 방향의 수평 단위벡터를 구한다. RetreatSpreadAngle만큼 좌우로 흔들어준다.
	FVector ComputeAwayDirection(const ACharacter* Character, const AActor* Target) const;

	// FromLocation에서 AwayDir로 DesiredDistance만큼 떨어진, 네비메시상 유효한 지점을 찾는다.
	// 중간이 막혀 있으면 막힌 지점 직전까지로 줄이고, MinRetreatDistance보다 짧으면 실패로 본다.
	// UNavigationSystemV1::NavigationRaycast가 WorldContextObject를 비-const UObject*로 받기 때문에
	// 이 함수는 const로 둘 수 없다.
	bool ComputeRetreatLocation(AAIController* AIController, const FVector& FromLocation,
		const FVector& AwayDir, float DesiredDistance, FVector& OutLocation);

	// Backstep 모드 시작. 후퇴 지점을 찾아 MoveTo를 건다.
	bool StartBackstep(AAIController* AIController, ACharacter* Character, const FVector& AwayDir);

	// BackJump 모드 시작. 착지 지점을 검증하고 LaunchCharacter로 도약시킨다.
	bool StartBackJump(AAIController* AIController, ACharacter* Character, const FVector& AwayDir);

	// 후퇴 몽타주를 재생하고 루트 모션을 억제한다. 재생 성공 여부를 반환한다.
	bool StartRetreatMontage(UAnimInstance* AnimInstance);

	// 이동 완료 콜백(Backstep 모드)
	void OnMoveCompleted(FAIRequestID RequestID, const FPathFollowingResult& Result);

	// 착지 콜백(BackJump 모드). 다이내믹 델리게이트라 UFUNCTION이 필수다.
	UFUNCTION()
	void OnCharacterLanded(const FHitResult& Hit);

	// 태스크를 한 번만 종료시킨다. FinishLatentTask 중복 호출 방지.
	void FinishTask(UBehaviorTreeComponent& OwnerComp, EBTNodeResult::Type Result);
	// 이동/착지 조건이 충족됐으면 태스크를 종료한다.
	void TryFinish(UBehaviorTreeComponent& OwnerComp);
	// 포커스, 델리게이트, 이동속도, 몽타주를 원래대로 되돌린다. 모든 종료 경로에서 호출해야 한다.
	void CleanupRetreat(AAIController* AIController);
	// OnRequestFinished / LandedDelegate 바인딩 해제.
	void UnbindAll(AAIController* AIController);
	// 몽타주 재생이 끝났다고 볼 수 있는지. 재생 시작 전 첫 프레임 오종료를 막는다.
	bool IsMontageDone(UAnimInstance* AnimInstance) const;
	// OwnerComp에서 AnimInstance를 얻는다.
	static UAnimInstance* GetAnimInstance(UBehaviorTreeComponent& OwnerComp);

	// 컴포넌트 참조 저장용 (델리게이트 바인딩 해제 시 사용)
	UPROPERTY()
	TObjectPtr<UBehaviorTreeComponent> CachedBTComp;

	// LandedDelegate 해제와 MaxWalkSpeed 복구에 쓴다.
	UPROPERTY()
	TObjectPtr<ACharacter> CachedCharacter;

	// 이 태스크가 요청한 이동 ID. 같은 컨트롤러의 다른 MoveTo가 끝난 것을
	// 이 태스크의 완료로 오인하지 않기 위해 비교한다.
	FAIRequestID MoveRequestID;

	// ExecuteTask 이후 경과 시간(초)
	float ElapsedTime = 0.f;

	// 후퇴 전 MaxWalkSpeed. 0이면 건드리지 않았다는 뜻이다.
	float SavedMaxWalkSpeed = 0.f;

	bool bMontagePlayStarted = false;

	// 이동(또는 착지)이 끝났는지와 그 결과
	bool bMoveFinished = false;
	bool bMoveSucceeded = false;

	// FinishLatentTask를 이미 호출했는지
	bool bTaskFinished = false;
};
