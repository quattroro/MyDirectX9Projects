// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "MonsterAITypes.h"
#include "MonsterRangeDebugComponent.generated.h"

class AMonsterAIController;
class UBehaviorTree;

//
// 후퇴 태스크 하나에서 읽어온 값. 공격 패턴마다 별도의 BTTask_MonsterRetreat 노드가
// 서로 다른 MoveMode/속도로 설정될 수 있어서 배열로 모은다.
//
struct FMonsterRetreatRangeInfo
{
	// 이 값을 들고 있던 BT 에셋 이름. 화면 라벨로 쓴다.
	FString Label;

	ERetreatMoveMode MoveMode = ERetreatMoveMode::Backstep;

	// Backstep 계열
	float RetreatDistance = 0.f;
	float MinRetreatDistance = 0.f;
	float SpreadAngleDegrees = 0.f;
	float AcceptanceRadius = 0.f;

	// BackJump 계열
	float LaunchXYSpeed = 0.f;
	float LaunchZSpeed = 0.f;

	// LaunchXY/LaunchZ와 중력으로 계산한 예상 수평 사거리와 체공 시간.
	// UBTTask_MonsterRetreat의 static 헬퍼로 계산하므로 실제 도약과 절대 갈라지지 않는다.
	float PredictedBackJumpRange = 0.f;
	float PredictedAirTime = 0.f;
};

//
// 공격 접근 반경 하나. BTTask_PlayMontageAndMove의 AcceptanceRadius는 패턴 에셋마다
// 오버라이드되어 있어서 C++ 기본값을 그리면 실제 정지 거리와 어긋난다.
//
struct FMonsterAttackRangeInfo
{
	FString Label;
	float AcceptanceRadius = 0.f;
};

//
// 한 프레임을 그리는 데 필요한 값을 흩어진 소유자들에게서 모아 담는 스냅샷.
// 이 구조체는 아무것도 소유하지 않는다. 전부 남의 값을 읽어온 사본이다.
//
struct FMonsterRangeSnapshot
{
	// 살아있는 컨트롤러에서 모았는지(PIE), 아니면 CDO에서 모았는지(에디터).
	bool bLive = false;

	// --- 감지 (AMonsterAIController의 SightConfig) ---
	bool bHasSight = false;
	float SightRadius = 0.f;
	float LoseSightRadius = 0.f;
	// 주의: UAISenseConfig_Sight의 이 값은 전체 시야각이 아니라 정면 기준 '반각'이다.
	// (엔진 쪽 DisplayName이 PeripheralVisionHalfAngleDegrees인 이유)
	float PeripheralVisionHalfAngleDegrees = 0.f;
	float AutoSuccessRange = 0.f;
	// 링과 시야 쐐기를 그릴 기준 높이. 루트(발밑)가 아니라 눈높이를 쓴다.
	FVector EyeLocation = FVector::ZeroVector;

	// --- 배회 (BTTask_FindRandomWanderPoint) ---
	bool bHasWander = false;
	float WanderRadius = 0.f;
	FVector HomeLocation = FVector::ZeroVector;

	// --- 후퇴 / 백점프 ---
	TArray<FMonsterRetreatRangeInfo> Retreats;

	// --- 공격 접근 반경 ---
	TArray<FMonsterAttackRangeInfo> AttackRanges;

	// --- 런타임 전용 (블랙보드) ---
	TWeakObjectPtr<AActor> Target;
	float DistanceToTarget = 0.f;
	EMonsterCombatState CombatState = EMonsterCombatState::Passive;
	EMonsterAttackType AttackType = EMonsterAttackType::AK_None;

	// 후퇴/백점프를 그릴 방향. 타겟이 있으면 타겟 반대 방향, 없으면 액터 뒤쪽.
	FVector AwayDirection = FVector::ZeroVector;

	// 중력 가속도의 절댓값. 백점프 포물선을 샘플링할 때 쓴다.
	// CharacterMovement에서 읽으므로 GravityScale이 이미 반영되어 있다.
	float AbsGravityZ = 0.f;

	// 액터 발밑 Z. 링을 지면에 깔기 위해 캡슐 절반 높이를 빼서 구한다.
	float FeetZ = 0.f;
};

//
// 몬스터의 감지/후퇴/공격/배회 범위를 뷰포트에 그려주는 디버그 전용 컴포넌트.
//
// 이 컴포넌트는 값을 소유하지 않고 읽기만 한다. 범위 값의 진실은 여전히
// AMonsterAIController의 SightConfig와 BT 에셋의 노드들에 있다. 여기서 값을 복제하면
// 화면과 실제 동작이 조용히 갈라지기 때문에, 매 프레임 원본을 다시 읽는다.
//
// PrimaryComponentTick.bTickInEditor = true로 두어 PIE와 에디터 뷰포트를 한 코드로 처리한다.
// 에디터에서는 컨트롤러가 없으므로 Pawn의 AIControllerClass CDO에서 SightConfig와
// BehaviorTreeAsset을 읽는다.
//
// 에디터 뷰포트에서 아무것도 안 보이면 Realtime(Ctrl+R)이 꺼져 있는지 먼저 확인할 것.
// 또 DrawDebugString은 PlayerController의 HUD를 거치므로 텍스트는 PIE에서만 보인다.
//
UCLASS(ClassGroup = (Debug), meta = (BlueprintSpawnableComponent))
class MY3DACTION_API UMonsterRangeDebugComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UMonsterRangeDebugComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
	// 마스터 스위치. 콘솔 변수 Monster.Debug.Ranges가 0/1이면 이 값을 무시한다.
	UPROPERTY(EditAnywhere, Category = "Monster Debug")
	bool bDebugEnabled = true;

	// 에디터 뷰포트(비플레이)에서 그릴지.
	UPROPERTY(EditAnywhere, Category = "Monster Debug")
	bool bDrawInEditor = true;

	// PIE/게임에서 그릴지.
	UPROPERTY(EditAnywhere, Category = "Monster Debug")
	bool bDrawInGame = true;

	// SightRadius / LoseSightRadius / AutoSuccessRange 링과 시야 쐐기.
	UPROPERTY(EditAnywhere, Category = "Monster Debug|Ranges")
	bool bShowDetection = true;

	// 후퇴 거리, 최소 후퇴 거리, 산포각, 백점프 포물선.
	UPROPERTY(EditAnywhere, Category = "Monster Debug|Ranges")
	bool bShowRetreat = true;

	// 공격 패턴별 접근 반경.
	UPROPERTY(EditAnywhere, Category = "Monster Debug|Ranges")
	bool bShowAttackRange = true;

	// HomeLocation 기준 배회 반경.
	UPROPERTY(EditAnywhere, Category = "Monster Debug|Ranges")
	bool bShowWander = true;

	// 머리 위 상태 텍스트와 타겟까지의 선. 텍스트는 PIE에서만 보인다.
	UPROPERTY(EditAnywhere, Category = "Monster Debug|Ranges")
	bool bShowLiveInfo = true;

	// 원을 몇 조각으로 쪼개 그릴지. 낮추면 각져 보이고 높이면 선이 많아진다.
	UPROPERTY(EditAnywhere, Category = "Monster Debug|Style", meta = (ClampMin = "8", ClampMax = "128"))
	int32 CircleSegments = 48;

	// 링을 발밑에서 살짝 띄운다. 0이면 지형과 Z-파이팅이 생긴다.
	UPROPERTY(EditAnywhere, Category = "Monster Debug|Style")
	float GroundOffset = 5.f;

	UPROPERTY(EditAnywhere, Category = "Monster Debug|Style", meta = (ClampMin = "0.0"))
	float LineThickness = 1.5f;

private:
	// 이번 프레임에 그려야 하는지. 월드 타입, 토글, 콘솔 변수를 모두 확인한다.
	bool ShouldDraw() const;

	// 흩어진 소유자들에게서 값을 모은다. 실패하면 false.
	bool GatherSnapshot(FMonsterRangeSnapshot& Out) const;

	// BT 에셋의 노드 템플릿을 훑어 범위 값을 뽑는다. RunBehavior 서브트리까지 재귀한다.
	// 노드 인스턴스가 아니라 템플릿을 읽는 이유: 이 노드들은 런타임에 자기 범위 값을
	// 수정하지 않으므로 두 값이 항상 같고, 템플릿 쪽이 에디터에서도 그대로 동작한다.
	static void GatherFromBehaviorTree(const UBehaviorTree* Tree, FMonsterRangeSnapshot& Out, TSet<const UBehaviorTree*>& Visited);

	void DrawRanges(const FMonsterRangeSnapshot& Snapshot) const;
	void DrawDetection(const FMonsterRangeSnapshot& Snapshot) const;
	void DrawRetreat(const FMonsterRangeSnapshot& Snapshot) const;
	void DrawAttackRanges(const FMonsterRangeSnapshot& Snapshot) const;
	void DrawWander(const FMonsterRangeSnapshot& Snapshot) const;
	void DrawLiveInfo(const FMonsterRangeSnapshot& Snapshot) const;

	// 수평 링 하나. DrawDebugCircle은 기본 축이 XZ 평면이라 축을 뒤집어 넘겨야 한다.
	void DrawGroundRing(const FVector& Center, float Radius, const FColor& Color) const;

	// 수평 부채꼴. 엔진의 DrawDebugCircleArc는 FindBestAxisVectors로 평면을 고르기 때문에
	// 수평이 보장되지 않는다. 그래서 Yaw 회전으로 직접 샘플링한다.
	void DrawGroundWedge(const FVector& Center, float Radius, const FVector& Direction, float HalfAngleDegrees, const FColor& Color) const;

	// 수평면에서 Direction을 Yaw로 회전시킨 단위벡터.
	static FVector RotateYaw(const FVector& Direction, float Degrees);

	// 텍스트 한 줄. PlayerController가 없으면(에디터) 조용히 무시된다.
	void DrawLabel(const FVector& Location, const FString& Text, const FColor& Color) const;
};
