// Fill out your copyright notice in the Description page of Project Settings.


#include "MonsterRangeDebugComponent.h"
#include "MonsterAIController.h"
#include "BTTask_MonsterRetreat.h"
#include "BTTask_PlayMontageAndMove.h"
#include "BTTask_FindRandomWanderPoint.h"

#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/Tasks/BTTask_RunBehavior.h"
#include "Components/CapsuleComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Perception/AISenseConfig_Sight.h"
#include "UObject/Class.h"
#include "UObject/UObjectHash.h"

// -1 = 액터별 체크박스(bDebugEnabled)를 따른다. 0 = 전부 강제로 끈다. 1 = 전부 강제로 켠다.
// 액터를 하나하나 선택하지 않고 PIE 콘솔에서 즉시 껐다 켜기 위한 스위치다.
static TAutoConsoleVariable<int32> CVarMonsterDebugRanges(
	TEXT("Monster.Debug.Ranges"),
	-1,
	TEXT("몬스터 범위 디버그 표시. -1=액터별 설정 사용, 0=강제 OFF, 1=강제 ON"),
	ECVF_Default);

namespace MonsterRangeDebugColors
{
	static const FColor Sight(40, 220, 60);
	static const FColor LoseSight(240, 220, 40);
	static const FColor AutoSuccess(250, 140, 30);
	static const FColor Vision(90, 255, 150);
	static const FColor Backstep(60, 200, 255);
	static const FColor MinRetreat(70, 110, 255);
	static const FColor BackJump(255, 80, 220);
	static const FColor Wander(190, 190, 190);
	static const FColor Live(255, 255, 255);

	// 공격 접근 반경이 여러 개일 때 구분용. 붉은 계열로 묶어둔다.
	static const FColor AttackPalette[] =
	{
		FColor(255, 60, 60),
		FColor(255, 130, 40),
		FColor(230, 40, 130),
		FColor(200, 90, 90)
	};
}

namespace
{
	// UENUM 값을 사람이 읽을 수 있는 이름으로. 화면 텍스트 전용이다.
	template <typename TEnum>
	FString EnumToDisplayString(TEnum Value)
	{
		if (const UEnum* EnumPtr = StaticEnum<TEnum>())
		{
			return EnumPtr->GetDisplayNameTextByValue(static_cast<int64>(Value)).ToString();
		}
		return FString::Printf(TEXT("%d"), static_cast<int32>(Value));
	}
}

UMonsterRangeDebugComponent::UMonsterRangeDebugComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
	// 이동/AI가 다 끝난 뒤에 그려야 한 프레임 밀린 위치에 링이 남지 않는다.
	PrimaryComponentTick.TickGroup = TG_PostUpdateWork;

	// UActorComponent의 멤버다. PrimaryComponentTick의 멤버가 아니다.
	// 이 플래그가 켜져 있어야 에디터 뷰포트에서도 TickComponent가 불린다.
	bTickInEditor = true;

	bAutoActivate = true;
	SetIsReplicatedByDefault(false);
}

void UMonsterRangeDebugComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

#if ENABLE_DRAW_DEBUG
	if (!ShouldDraw())
	{
		return;
	}

	FMonsterRangeSnapshot Snapshot;
	if (!GatherSnapshot(Snapshot))
	{
		return;
	}

	DrawRanges(Snapshot);
#endif
}

bool UMonsterRangeDebugComponent::ShouldDraw() const
{
	const UWorld* World = GetWorld();
	if (!World || !GetOwner())
	{
		return false;
	}

	const int32 CVarValue = CVarMonsterDebugRanges.GetValueOnAnyThread();
	if (CVarValue == 0)
	{
		return false;
	}

	const bool bForce = (CVarValue > 0);
	if (!bForce && !bDebugEnabled)
	{
		return false;
	}

	// 블루프린트 에디터 프리뷰나 썸네일 렌더링 월드에서는 그리지 않는다.
	switch (World->WorldType)
	{
	case EWorldType::Editor:
		return bForce || bDrawInEditor;
	case EWorldType::PIE:
	case EWorldType::Game:
		return bForce || bDrawInGame;
	default:
		return false;
	}
}

bool UMonsterRangeDebugComponent::GatherSnapshot(FMonsterRangeSnapshot& Out) const
{
	const ACharacter* OwnerCharacter = Cast<ACharacter>(GetOwner());
	if (!OwnerCharacter)
	{
		return false;
	}

	const FVector OwnerLocation = OwnerCharacter->GetActorLocation();
	Out.EyeLocation = OwnerCharacter->GetPawnViewLocation();

	// 캐릭터의 액터 원점은 발밑이 아니라 캡슐 중심이다.
	// 링을 지면에 깔려면 캡슐 절반 높이를 빼야 한다.
	Out.FeetZ = OwnerLocation.Z + GroundOffset;
	if (const UCapsuleComponent* Capsule = OwnerCharacter->GetCapsuleComponent())
	{
		Out.FeetZ -= Capsule->GetScaledCapsuleHalfHeight();
	}

	if (const UCharacterMovementComponent* MoveComp = OwnerCharacter->GetCharacterMovement())
	{
		Out.AbsGravityZ = FMath::Abs(MoveComp->GetGravityZ());
	}

	// 살아있는 컨트롤러가 있으면 그것을, 없으면(에디터) AIControllerClass의 CDO를 읽는다.
	const AMonsterAIController* Controller = Cast<AMonsterAIController>(OwnerCharacter->GetController());
	Out.bLive = (Controller != nullptr);
	if (!Controller)
	{
		const UClass* ControllerClass = OwnerCharacter->AIControllerClass;
		if (ControllerClass && ControllerClass->IsChildOf(AMonsterAIController::StaticClass()))
		{
			Controller = Cast<AMonsterAIController>(ControllerClass->GetDefaultObject());
		}
	}

	if (!Controller)
	{
		// 컨트롤러도 CDO도 없으면 읽어올 값이 없다.
		return false;
	}

	// --- 감지 ---
	if (const UAISenseConfig_Sight* SightConfig = Controller->GetSightConfig())
	{
		Out.bHasSight = true;
		Out.SightRadius = SightConfig->SightRadius;
		Out.LoseSightRadius = SightConfig->LoseSightRadius;
		Out.PeripheralVisionHalfAngleDegrees = SightConfig->PeripheralVisionAngleDegrees;
		Out.AutoSuccessRange = SightConfig->AutoSuccessRangeFromLastSeenLocation;
	}

	// --- BT 노드에서 후퇴 / 공격 / 배회 값 ---
	if (const UBehaviorTree* Tree = Controller->GetBehaviorTreeAsset())
	{
		TSet<const UBehaviorTree*> Visited;
		GatherFromBehaviorTree(Tree, Out, Visited);
	}

	// 백점프 예상 사거리는 실제 도약과 같은 함수로 계산한다.
	for (FMonsterRetreatRangeInfo& Retreat : Out.Retreats)
	{
		Retreat.PredictedAirTime = UBTTask_MonsterRetreat::PredictBackJumpAirTime(Retreat.LaunchZSpeed, Out.AbsGravityZ);
		Retreat.PredictedBackJumpRange = UBTTask_MonsterRetreat::PredictBackJumpRange(Retreat.LaunchXYSpeed, Retreat.LaunchZSpeed, Out.AbsGravityZ);
	}

	// --- HomeLocation ---
	// 컨트롤러는 OnPossess 시점의 위치를 캐싱한다. 에디터에는 그 값이 없으니
	// 액터를 배치한 위치를 그대로 쓴다. 플레이하면 캐싱될 값과 같다.
	Out.HomeLocation = (Out.bLive && !Controller->HomeLocation.IsNearlyZero())
		? Controller->HomeLocation
		: OwnerLocation;

	// --- 런타임 값 (블랙보드) ---
	if (Out.bLive)
	{
		if (const UBlackboardComponent* Blackboard = Controller->GetBlackboardComponent())
		{
			// 키 이름은 AMonsterAIController / BTService_UpdateMonsterState가 쓰는 것과 같다.
			Out.Target = Cast<AActor>(Blackboard->GetValueAsObject(TEXT("TargetActor")));
			Out.DistanceToTarget = Blackboard->GetValueAsFloat(TEXT("DistanceToTarget"));
			Out.CombatState = static_cast<EMonsterCombatState>(Blackboard->GetValueAsEnum(TEXT("CombatState")));
			Out.AttackType = static_cast<EMonsterAttackType>(Blackboard->GetValueAsEnum(TEXT("AttackType")));
		}
	}

	// --- 후퇴 방향 ---
	// 타겟이 있으면 타겟 반대 방향, 없으면 액터 뒤쪽.
	FVector AwayDirection = -OwnerCharacter->GetActorForwardVector();
	if (const AActor* Target = Out.Target.Get())
	{
		FVector FromTarget = OwnerLocation - Target->GetActorLocation();
		FromTarget.Z = 0.f;
		if (!FromTarget.IsNearlyZero())
		{
			AwayDirection = FromTarget;
		}
	}
	AwayDirection.Z = 0.f;
	Out.AwayDirection = AwayDirection.GetSafeNormal();

	return true;
}

void UMonsterRangeDebugComponent::GatherFromBehaviorTree(const UBehaviorTree* Tree, FMonsterRangeSnapshot& Out, TSet<const UBehaviorTree*>& Visited)
{
	if (!Tree || Visited.Contains(Tree))
	{
		return;
	}
	Visited.Add(Tree);

	const FString TreeLabel = Tree->GetName();

	// RunBehavior 서브트리는 순회를 끝낸 뒤에 재귀한다.
	TArray<const UBehaviorTree*> Subtrees;

	// BT 노드는 모두 BT 에셋을 Outer로 갖는다. 자식 링크를 직접 걷는 것보다 간단하고,
	// 데코레이터/서비스까지 한 번에 훑을 수 있다.
	ForEachObjectWithOuter(Tree, [&Out, &Subtrees, &TreeLabel](UObject* Object)
	{
		if (const UBTTask_MonsterRetreat* Retreat = Cast<UBTTask_MonsterRetreat>(Object))
		{
			FMonsterRetreatRangeInfo& Info = Out.Retreats.AddDefaulted_GetRef();
			Info.Label = TreeLabel;
			Info.MoveMode = Retreat->GetMoveMode();
			Info.RetreatDistance = Retreat->GetRetreatDistance();
			Info.MinRetreatDistance = Retreat->GetMinRetreatDistance();
			Info.SpreadAngleDegrees = Retreat->GetRetreatSpreadAngle();
			Info.AcceptanceRadius = Retreat->GetBackstepAcceptanceRadius();
			Info.LaunchXYSpeed = Retreat->GetLaunchXYSpeed();
			Info.LaunchZSpeed = Retreat->GetLaunchZSpeed();
		}
		else if (const UBTTask_PlayMontageAndMove* Move = Cast<UBTTask_PlayMontageAndMove>(Object))
		{
			const float Radius = Move->GetAcceptanceRadius();

			// 반경이 같은 노드가 이미 있으면 링을 겹쳐 그리지 않고 라벨만 합친다.
			FMonsterAttackRangeInfo* Existing = Out.AttackRanges.FindByPredicate(
				[Radius](const FMonsterAttackRangeInfo& Candidate)
				{
					return FMath::IsNearlyEqual(Candidate.AcceptanceRadius, Radius, 1.f);
				});

			if (Existing)
			{
				if (!Existing->Label.Contains(TreeLabel))
				{
					Existing->Label += TEXT(" / ") + TreeLabel;
				}
			}
			else
			{
				FMonsterAttackRangeInfo& Info = Out.AttackRanges.AddDefaulted_GetRef();
				Info.Label = TreeLabel;
				Info.AcceptanceRadius = Radius;
			}
		}
		else if (const UBTTask_FindRandomWanderPoint* Wander = Cast<UBTTask_FindRandomWanderPoint>(Object))
		{
			Out.bHasWander = true;
			Out.WanderRadius = FMath::Max(Out.WanderRadius, Wander->GetWanderRadius());
		}
		else if (const UBTTask_RunBehavior* RunBehavior = Cast<UBTTask_RunBehavior>(Object))
		{
			if (const UBehaviorTree* Subtree = RunBehavior->GetSubtreeAsset())
			{
				Subtrees.AddUnique(Subtree);
			}
		}
	}, /*bIncludeNestedObjects*/ true);

	for (const UBehaviorTree* Subtree : Subtrees)
	{
		GatherFromBehaviorTree(Subtree, Out, Visited);
	}
}

void UMonsterRangeDebugComponent::DrawRanges(const FMonsterRangeSnapshot& Snapshot) const
{
#if ENABLE_DRAW_DEBUG
	// 큰 링부터 그려서 작은 링이 위에 남게 한다.
	if (bShowWander)
	{
		DrawWander(Snapshot);
	}
	if (bShowDetection)
	{
		DrawDetection(Snapshot);
	}
	if (bShowAttackRange)
	{
		DrawAttackRanges(Snapshot);
	}
	if (bShowRetreat)
	{
		DrawRetreat(Snapshot);
	}
	if (bShowLiveInfo)
	{
		DrawLiveInfo(Snapshot);
	}
#endif
}

void UMonsterRangeDebugComponent::DrawDetection(const FMonsterRangeSnapshot& Snapshot) const
{
#if ENABLE_DRAW_DEBUG
	if (!Snapshot.bHasSight)
	{
		return;
	}

	const AActor* OwnerActor = GetOwner();
	const FVector OwnerLocation = OwnerActor->GetActorLocation();
	const FVector GroundCenter(OwnerLocation.X, OwnerLocation.Y, Snapshot.FeetZ);

	DrawGroundRing(GroundCenter, Snapshot.SightRadius, MonsterRangeDebugColors::Sight);
	DrawGroundRing(GroundCenter, Snapshot.LoseSightRadius, MonsterRangeDebugColors::LoseSight);
	if (Snapshot.AutoSuccessRange > 0.f)
	{
		DrawGroundRing(GroundCenter, Snapshot.AutoSuccessRange, MonsterRangeDebugColors::AutoSuccess);
	}

	// 시야 쐐기는 눈높이에서 그린다. Perception도 눈높이를 기준으로 판정한다.
	// PeripheralVisionAngleDegrees는 정면 기준 반각이므로 좌우로 그 값만큼 벌린다.
	DrawGroundWedge(Snapshot.EyeLocation, Snapshot.SightRadius, OwnerActor->GetActorForwardVector(),
		Snapshot.PeripheralVisionHalfAngleDegrees, MonsterRangeDebugColors::Vision);

	DrawLabel(GroundCenter + FVector(0.f, 0.f, 20.f),
		FString::Printf(TEXT("Sight %.0f / Lose %.0f / Auto %.0f / FOV +-%.0f (전체 %.0f)"),
			Snapshot.SightRadius, Snapshot.LoseSightRadius, Snapshot.AutoSuccessRange,
			Snapshot.PeripheralVisionHalfAngleDegrees, Snapshot.PeripheralVisionHalfAngleDegrees * 2.f),
		MonsterRangeDebugColors::Sight);
#endif
}

void UMonsterRangeDebugComponent::DrawRetreat(const FMonsterRangeSnapshot& Snapshot) const
{
#if ENABLE_DRAW_DEBUG
	const UWorld* World = GetWorld();
	const AActor* OwnerActor = GetOwner();
	if (!World || Snapshot.AwayDirection.IsNearlyZero())
	{
		return;
	}

	const FVector Start = OwnerActor->GetActorLocation();
	const FVector GroundStart(Start.X, Start.Y, Snapshot.FeetZ);

	for (const FMonsterRetreatRangeInfo& Retreat : Snapshot.Retreats)
	{
		if (Retreat.MoveMode == ERetreatMoveMode::BackJump)
		{
			// 백점프 거리는 authored 값이 아니라 LaunchXY/LaunchZ와 중력에서 나온 결과다.
			// 링 하나로는 부족해서 실제 포물선을 샘플링해 착지 지점을 보여준다.
			const float AirTime = Retreat.PredictedAirTime;
			if (AirTime <= KINDA_SMALL_NUMBER)
			{
				continue;
			}

			const int32 Steps = 24;
			FVector PreviousPoint = Start;
			for (int32 Step = 1; Step <= Steps; ++Step)
			{
				const float Time = AirTime * static_cast<float>(Step) / static_cast<float>(Steps);
				const FVector Point = Start
					+ Snapshot.AwayDirection * Retreat.LaunchXYSpeed * Time
					+ FVector(0.f, 0.f, Retreat.LaunchZSpeed * Time - 0.5f * Snapshot.AbsGravityZ * Time * Time);

				DrawDebugLine(World, PreviousPoint, Point, MonsterRangeDebugColors::BackJump,
					false, 0.f, SDPG_World, LineThickness);
				PreviousPoint = Point;
			}

			const FVector Landing = GroundStart + Snapshot.AwayDirection * Retreat.PredictedBackJumpRange;
			DrawGroundRing(Landing, 60.f, MonsterRangeDebugColors::BackJump);
			DrawLabel(Landing + FVector(0.f, 0.f, 40.f),
				FString::Printf(TEXT("%s BackJump %.0f (XY %.0f / Z %.0f / 체공 %.2fs)"),
					*Retreat.Label, Retreat.PredictedBackJumpRange,
					Retreat.LaunchXYSpeed, Retreat.LaunchZSpeed, AirTime),
				MonsterRangeDebugColors::BackJump);
		}
		else
		{
			const FVector Goal = GroundStart + Snapshot.AwayDirection * Retreat.RetreatDistance;

			DrawDebugLine(World, GroundStart, Goal, MonsterRangeDebugColors::Backstep,
				false, 0.f, SDPG_World, LineThickness);
			DrawGroundRing(Goal, FMath::Max(Retreat.AcceptanceRadius, 1.f), MonsterRangeDebugColors::Backstep);

			// 최소 후퇴 거리. 이보다 짧게밖에 못 물러나면 태스크가 Failed를 낸다.
			const FVector MinPoint = GroundStart + Snapshot.AwayDirection * Retreat.MinRetreatDistance;
			const FVector Side = RotateYaw(Snapshot.AwayDirection, 90.f) * 40.f;
			DrawDebugLine(World, MinPoint - Side, MinPoint + Side, MonsterRangeDebugColors::MinRetreat,
				false, 0.f, SDPG_World, LineThickness);

			// 좌우 산포각. 후퇴 방향이 매번 이 범위 안에서 흔들린다.
			if (Retreat.SpreadAngleDegrees > 0.f)
			{
				DrawGroundWedge(GroundStart, Retreat.RetreatDistance, Snapshot.AwayDirection,
					Retreat.SpreadAngleDegrees, MonsterRangeDebugColors::Backstep);
			}

			DrawLabel(Goal + FVector(0.f, 0.f, 40.f),
				FString::Printf(TEXT("%s Backstep %.0f (최소 %.0f / 산포 +-%.0f)"),
					*Retreat.Label, Retreat.RetreatDistance, Retreat.MinRetreatDistance, Retreat.SpreadAngleDegrees),
				MonsterRangeDebugColors::Backstep);
		}
	}
#endif
}

void UMonsterRangeDebugComponent::DrawAttackRanges(const FMonsterRangeSnapshot& Snapshot) const
{
#if ENABLE_DRAW_DEBUG
	const AActor* OwnerActor = GetOwner();
	const FVector OwnerLocation = OwnerActor->GetActorLocation();
	const FVector GroundCenter(OwnerLocation.X, OwnerLocation.Y, Snapshot.FeetZ);

	const int32 PaletteCount = UE_ARRAY_COUNT(MonsterRangeDebugColors::AttackPalette);

	for (int32 Index = 0; Index < Snapshot.AttackRanges.Num(); ++Index)
	{
		const FMonsterAttackRangeInfo& Info = Snapshot.AttackRanges[Index];
		const FColor& Color = MonsterRangeDebugColors::AttackPalette[Index % PaletteCount];

		// 이 반경은 타겟 캡슐 중심까지의 거리 기준이라 몬스터 중심 링이 정확한 표현이다.
		DrawGroundRing(GroundCenter, Info.AcceptanceRadius, Color);

		// 라벨은 링 위에 올려서 링이 여러 개일 때 어떤 게 어떤 패턴인지 구분한다.
		const FVector LabelLocation = GroundCenter + OwnerActor->GetActorForwardVector() * Info.AcceptanceRadius;
		DrawLabel(LabelLocation + FVector(0.f, 0.f, 20.f),
			FString::Printf(TEXT("%s 접근반경 %.0f"), *Info.Label, Info.AcceptanceRadius), Color);
	}
#endif
}

void UMonsterRangeDebugComponent::DrawWander(const FMonsterRangeSnapshot& Snapshot) const
{
#if ENABLE_DRAW_DEBUG
	const UWorld* World = GetWorld();
	if (!World || !Snapshot.bHasWander)
	{
		return;
	}

	const FVector Home(Snapshot.HomeLocation.X, Snapshot.HomeLocation.Y, Snapshot.FeetZ);
	const FVector OwnerLocation = GetOwner()->GetActorLocation();
	const FVector Current(OwnerLocation.X, OwnerLocation.Y, Snapshot.FeetZ);

	DrawGroundRing(Home, Snapshot.WanderRadius, MonsterRangeDebugColors::Wander);
	DrawDebugSphere(World, Home, 30.f, 8, MonsterRangeDebugColors::Wander, false, 0.f, SDPG_World, LineThickness);
	DrawDebugLine(World, Home, Current, MonsterRangeDebugColors::Wander, false, 0.f, SDPG_World, 1.f);

	DrawLabel(Home + FVector(0.f, 0.f, 60.f),
		FString::Printf(TEXT("Home / 배회반경 %.0f"), Snapshot.WanderRadius),
		MonsterRangeDebugColors::Wander);
#endif
}

void UMonsterRangeDebugComponent::DrawLiveInfo(const FMonsterRangeSnapshot& Snapshot) const
{
#if ENABLE_DRAW_DEBUG
	const UWorld* World = GetWorld();
	const ACharacter* OwnerCharacter = Cast<ACharacter>(GetOwner());
	if (!World || !OwnerCharacter)
	{
		return;
	}

	FVector HeadLocation = OwnerCharacter->GetActorLocation();
	if (const UCapsuleComponent* Capsule = OwnerCharacter->GetCapsuleComponent())
	{
		HeadLocation.Z += Capsule->GetScaledCapsuleHalfHeight() + 60.f;
	}

	if (!Snapshot.bLive)
	{
		// 에디터에서는 살아있는 상태가 없다. 어떤 출처의 값인지만 알려준다.
		DrawLabel(HeadLocation, TEXT("[Editor] AIController CDO + BT 에셋 값"), MonsterRangeDebugColors::Live);
		return;
	}

	DrawLabel(HeadLocation,
		FString::Printf(TEXT("%s / %s / 타겟거리 %.0f"),
			*EnumToDisplayString(Snapshot.CombatState),
			*EnumToDisplayString(Snapshot.AttackType),
			Snapshot.DistanceToTarget),
		MonsterRangeDebugColors::Live);

	if (const AActor* Target = Snapshot.Target.Get())
	{
		DrawDebugLine(World, OwnerCharacter->GetActorLocation(), Target->GetActorLocation(),
			MonsterRangeDebugColors::LoseSight, false, 0.f, SDPG_World, LineThickness);
	}
#endif
}

void UMonsterRangeDebugComponent::DrawGroundRing(const FVector& Center, float Radius, const FColor& Color) const
{
#if ENABLE_DRAW_DEBUG
	const UWorld* World = GetWorld();
	if (!World || Radius <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	// DrawDebugCircle의 기본 축은 세로(XZ) 평면이다. YAxis/ZAxis를 넘겨 XY 평면으로 눕힌다.
	DrawDebugCircle(World, Center, Radius, FMath::Max(CircleSegments, 8), Color,
		false, 0.f, SDPG_World, LineThickness,
		FVector(1.f, 0.f, 0.f), FVector(0.f, 1.f, 0.f), /*bDrawAxis*/ false);
#endif
}

void UMonsterRangeDebugComponent::DrawGroundWedge(const FVector& Center, float Radius, const FVector& Direction, float HalfAngleDegrees, const FColor& Color) const
{
#if ENABLE_DRAW_DEBUG
	const UWorld* World = GetWorld();
	if (!World || Radius <= KINDA_SMALL_NUMBER || HalfAngleDegrees <= 0.f)
	{
		return;
	}

	FVector Forward = Direction;
	Forward.Z = 0.f;
	Forward = Forward.GetSafeNormal();
	if (Forward.IsNearlyZero())
	{
		return;
	}

	const float ClampedHalfAngle = FMath::Min(HalfAngleDegrees, 180.f);
	const int32 Segments = FMath::Max(FMath::CeilToInt(CircleSegments * ClampedHalfAngle / 180.f), 2);
	const float AngleStep = (ClampedHalfAngle * 2.f) / static_cast<float>(Segments);

	// 부채꼴의 양 옆 경계선
	const FVector LeftEdge = Center + RotateYaw(Forward, -ClampedHalfAngle) * Radius;
	const FVector RightEdge = Center + RotateYaw(Forward, ClampedHalfAngle) * Radius;
	DrawDebugLine(World, Center, LeftEdge, Color, false, 0.f, SDPG_World, LineThickness);
	DrawDebugLine(World, Center, RightEdge, Color, false, 0.f, SDPG_World, LineThickness);

	// 호. 엔진의 DrawDebugCircleArc는 FindBestAxisVectors로 평면을 고르기 때문에
	// 수평이 보장되지 않는다. 그래서 Yaw 회전으로 직접 샘플링한다.
	FVector PreviousPoint = LeftEdge;
	for (int32 Step = 1; Step <= Segments; ++Step)
	{
		const float Angle = -ClampedHalfAngle + AngleStep * static_cast<float>(Step);
		const FVector Point = Center + RotateYaw(Forward, Angle) * Radius;
		DrawDebugLine(World, PreviousPoint, Point, Color, false, 0.f, SDPG_World, LineThickness);
		PreviousPoint = Point;
	}
#endif
}

FVector UMonsterRangeDebugComponent::RotateYaw(const FVector& Direction, float Degrees)
{
	return FRotator(0.f, Degrees, 0.f).RotateVector(Direction);
}

void UMonsterRangeDebugComponent::DrawLabel(const FVector& Location, const FString& Text, const FColor& Color) const
{
#if ENABLE_DRAW_DEBUG
	// DrawDebugString은 PlayerController의 HUD를 거친다. 에디터 월드에는 PC가 없어서
	// 텍스트는 PIE에서만 보인다. 에러는 아니고, 에디터에서는 색으로 구분해야 한다.
	if (const UWorld* World = GetWorld())
	{
		DrawDebugString(World, Location, Text, nullptr, Color, 0.f, /*bDrawShadow*/ true);
	}
#endif
}
