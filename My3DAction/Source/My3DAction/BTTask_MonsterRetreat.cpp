// Fill out your copyright notice in the Description page of Project Settings.


#include "BTTask_MonsterRetreat.h"

#include "AIController.h"
#include "NavigationSystem.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/BlackboardData.h"

namespace
{
	// Montage_Play 직후 첫 프레임에는 Montage_IsPlaying이 아직 false일 수 있다.
	// 이 시간이 지나기 전에는 몽타주가 끝났다고 판단하지 않는다.
	constexpr float MontageStartGraceTime2 = 0.1f;
}

UBTTask_MonsterRetreat::UBTTask_MonsterRetreat()
{
	NodeName = TEXT("Monster Retreat");
	bNotifyTick = true;

	// 이 노드는 진행 중인 이동 ID와 완료 플래그를 멤버로 들고 있다.
	// 노드 인스턴스를 만들지 않으면 그 상태를 같은 트리를 쓰는 모든 몬스터가 공유해
	// 서로의 후퇴 완료 처리를 덮어쓴다. (BTTask_PlayMontageAndMove와 동일한 이유)
	bCreateNodeInstance = true;

	TargetKey.AddObjectFilter(this, GET_MEMBER_NAME_CHECKED(UBTTask_MonsterRetreat, TargetKey), AActor::StaticClass());
}


// Behavior Tree Asset 자체와 관련된 초기화를 진행할 때 사용한다.
// AI마다 달라지는 데이터를 넣으면 안된다. BTTask는 여러 AI가 공유할 수 있기 때문에
void UBTTask_MonsterRetreat::InitializeFromAsset(UBehaviorTree& Asset)
{
	Super::InitializeFromAsset(Asset);

	if (UBlackboardData* BBAsset = GetBlackboardAsset())
	{
		if (TargetKey.SelectedKeyName.IsNone())
		{
			TargetKey.SelectedKeyName = TEXT("TargetActor");
		}

		//Behavior Tree 에디터에서 선택해 둔 Blackboard Key의 "이름"을 실제 Blackboard에서 사용할 수 있는 Key ID(키 번호)로 찾아서 확정하는 함수
		//ResolveSelectedKey() → "어떤 Blackboard Key를 사용할지 찾는 것"
		//GetValueAsObject() → "그 Key에 현재 들어있는 값을 가져오는 것"
		TargetKey.ResolveSelectedKey(*BBAsset);
	}
}

EBTNodeResult::Type UBTTask_MonsterRetreat::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	AAIController* AIController = OwnerComp.GetAIOwner();
	ACharacter* Character = AIController ? Cast<ACharacter>(AIController->GetPawn()) : nullptr;
	UBlackboardComponent* BlackboardComp = OwnerComp.GetBlackboardComponent();

	if (!AIController || !Character || !BlackboardComp)
	{
		return EBTNodeResult::Failed;
	}

	// 실행할 때마다 상태를 초기화한다.
	CachedBTComp = &OwnerComp;
	CachedCharacter = Character;
	MoveRequestID = FAIRequestID::InvalidRequest;
	ElapsedTime = 0.f;
	SavedMaxWalkSpeed = 0.f;
	bMontagePlayStarted = false;
	bMoveFinished = false;
	bMoveSucceeded = false;
	bTaskFinished = false;

	// 후퇴는 무엇으로부터 멀어지는지가 정해져야 성립한다.
	AActor* TargetActor = Cast<AActor>(BlackboardComp->GetValueAsObject(TargetKey.SelectedKeyName));
	if (!TargetActor)
	{
		UE_LOG(LogTemp, Warning, TEXT("MonsterRetreat: 블랙보드 키 '%s'에 유효한 액터가 없다."),
			*TargetKey.SelectedKeyName.ToString());
		CachedBTComp = nullptr;
		CachedCharacter = nullptr;
		return EBTNodeResult::Failed;
	}


	// 타겟 반대 방향의 수평 단위벡터를 구한다. RetreatSpreadAngle만큼 좌우로 흔들어준다.
	const FVector AwayDir = ComputeAwayDirection(Character, TargetActor);


	// 몸통이 타겟을 계속 향하게 만드는 핵심.
	// PathFollowingComponent는 이동 중 매 프레임 SetFocalPoint(진행 방향, EAIFocusPriority::Move)를
	// 호출한다. AAIController::GetFocalPoint()는 우선순위 배열을 높은 쪽부터 훑기 때문에
	// Gameplay(2)로 건 포커스가 Move(1)를 항상 이긴다. 그래서 뒤로 이동하면서도 정면을 유지한다.
	// 해당 캐릭터가 TargetActor를 따라 회전하도록 한다.
	// AIFocusPriority는 우선순위다. 
	// EAIFocusPriority::Default -> 기본 상태이거나 아무 지정도 없을 때의 시선
	// EAIFocusPriority::Move -> AI가 길 찾기(Move To)로 이동할 때 이동 방향을 바라보도록
	// EAIFocusPriority::Gameplay -> 개발자가 코드나 블루프린트(SetFocus)로 직접 적을 조준하거나 특정 대상을 보게 만들 때 쓴다. 
	// (Move 보다 높기 때문에 옆으로 게걸음 하면서 적을 조준하는 행동이 가능해진다.)
	// EAIFocusPriority::LastFocusPriority -> 엔진 내부용 혹은 가장 절대적인 우선순위 레이어
	AIController->SetFocus(TargetActor, EAIFocusPriority::Gameplay);

	const bool bStarted = (MoveMode == ERetreatMoveMode::Backstep)
		? StartBackstep(AIController, Character, AwayDir)
		: StartBackJump(AIController, Character, AwayDir);

	if (!bStarted)
	{
		CleanupRetreat(AIController);
		CachedBTComp = nullptr;
		CachedCharacter = nullptr;
		return EBTNodeResult::Failed;
	}

	// 이미 목표 지점에 서 있는 등의 이유로 이동이 즉시 끝난 경우.
	// 물러날 것이 없으므로 몽타주를 재생하지 않고 그대로 끝낸다.
	if (bMoveFinished)
	{
		CleanupRetreat(AIController);
		CachedBTComp = nullptr;
		CachedCharacter = nullptr;
		bTaskFinished = true;
		return bMoveSucceeded ? EBTNodeResult::Succeeded : EBTNodeResult::Failed;
	}

	StartRetreatMontage(Character->GetMesh() ? Character->GetMesh()->GetAnimInstance() : nullptr);

	return EBTNodeResult::InProgress;
}

bool UBTTask_MonsterRetreat::StartBackstep(AAIController* AIController, ACharacter* Character, const FVector& AwayDir)
{
	FVector RetreatLocation;
	if (!ComputeRetreatLocation(AIController, Character->GetActorLocation(), AwayDir, RetreatDistance, RetreatLocation))
	{
		UE_LOG(LogTemp, Warning, TEXT("MonsterRetreat: 물러날 수 있는 네비메시 지점을 찾지 못했다."));
		return false;
	}

	if (RetreatWalkSpeed > 0.f)
	{
		if (UCharacterMovementComponent* MoveComp = Character->GetCharacterMovement())
		{
			SavedMaxWalkSpeed = MoveComp->MaxWalkSpeed;
			MoveComp->MaxWalkSpeed = RetreatWalkSpeed;
		}
	}

	FAIMoveRequest MoveRequest(RetreatLocation);
	MoveRequest.SetAcceptanceRadius(AcceptanceRadius);
	// 짧은 후퇴에 경로 탐색을 쓰면 몬스터가 앞으로 우회해서 돌아가는 경로를 고를 수 있다.
	MoveRequest.SetUsePathfinding(bUsePathfinding);
	MoveRequest.SetProjectGoalLocation(true);
	MoveRequest.SetReachTestIncludesAgentRadius(false);
	// 정면 유지 자체는 위의 SetFocus(Gameplay)가 담당한다.
	// bCanStrafe는 focal point를 끄는 것이 아니라 어느 지점으로 잡을지만 바꾸므로
	// 이 플래그만으로는 정면이 유지되지 않는다. 의도를 명시하는 용도로만 켠다.
	MoveRequest.SetCanStrafe(true);

	// 델리게이트는 반드시 MoveTo 뒤에 건다.
	// MoveTo는 이미 목표에 도착해 있거나 즉시 실패한 경우 호출 스택 안에서 곧바로
	// OnRequestFinished를 브로드캐스트하기 때문이다. (BTTask_PlayMontageAndMove와 동일)
	const FPathFollowingRequestResult RequestResult = AIController->MoveTo(MoveRequest);
	MoveRequestID = RequestResult.MoveId;

	UE_LOG(LogTemp, Log, TEXT("MonsterRetreat: Backstep Goal=(%s) Code=%d MoveId=%u"),
		*RetreatLocation.ToCompactString(), (int32)RequestResult.Code, MoveRequestID.GetID());

	if (RequestResult.Code == EPathFollowingRequestResult::Failed)
	{
		return false;
	}

	if (RequestResult.Code == EPathFollowingRequestResult::AlreadyAtGoal)
	{
		bMoveFinished = true;
		bMoveSucceeded = true;
		return true;
	}

	if (UPathFollowingComponent* PathFollowingComp = AIController->GetPathFollowingComponent())
	{
		PathFollowingComp->OnRequestFinished.AddUObject(this, &UBTTask_MonsterRetreat::OnMoveCompleted);
	}

	return true;
}

bool UBTTask_MonsterRetreat::StartBackJump(AAIController* AIController, ACharacter* Character, const FVector& AwayDir)
{
	UCharacterMovementComponent* MoveComp = Character->GetCharacterMovement();
	if (!MoveComp)
	{
		return false;
	}

	// 체공 시간 t = 2 * Vz / |g|, 수평 사거리 = Vxy * t.
	// UCharacterMovementComponent::GetGravityZ()는 GravityScale이 곱해진 값을 돌려준다.
	const float GravityZ = FMath::Abs(MoveComp->GetGravityZ());
	if (GravityZ < KINDA_SMALL_NUMBER)
	{
		return false;
	}

	const float AirTime = 2.f * LaunchZSpeed / GravityZ;
	const float PredictedRange = LaunchXYSpeed * AirTime;

	// 착지 예상 지점이 네비메시 밖이거나 중간이 막혀 있으면 도약 거리를 그만큼 줄인다.
	const FVector StartLocation = Character->GetActorLocation();
	FVector LandingLocation;
	if (!ComputeRetreatLocation(AIController, StartLocation, AwayDir, PredictedRange, LandingLocation))
	{
		UE_LOG(LogTemp, Warning, TEXT("MonsterRetreat: 백점프 착지 지점을 찾지 못했다."));
		return false;
	}

	const float ActualRange = FVector::Dist2D(StartLocation, LandingLocation);
	const float LaunchXY = (PredictedRange > KINDA_SMALL_NUMBER)
		? LaunchXYSpeed * FMath::Clamp(ActualRange / PredictedRange, 0.f, 1.f)
		: 0.f;

	// PathFollowing이 계속 이동 입력을 넣으면 도약 궤적이 망가진다.
	AIController->StopMovement();

	// 착지 판정은 LandedDelegate로 한다. IsMovingOnGround() 폴링은 쓰면 안 된다.
	// LaunchCharacter는 그 프레임에 아직 MOVE_Falling이 아니라서 같은 프레임 폴링이 true를
	// 반환해 태스크가 즉시 오종료된다.
	Character->LandedDelegate.AddDynamic(this, &UBTTask_MonsterRetreat::OnCharacterLanded);

	// bXYOverride / bZOverride를 모두 true로 준다.
	// 달려오던 전방 관성이 더해져 백점프를 상쇄하는 것을 막기 위해 속도를 덮어쓴다.
	Character->LaunchCharacter(AwayDir * LaunchXY + FVector::UpVector * LaunchZSpeed, true, true);

	UE_LOG(LogTemp, Log, TEXT("MonsterRetreat: BackJump XY=%.0f Z=%.0f 예상사거리=%.0f 실제=%.0f"),
		LaunchXY, LaunchZSpeed, PredictedRange, ActualRange);

	return true;
}

// 타겟 반대 방향의 수평 단위벡터를 구한다. RetreatSpreadAngle만큼 좌우로 흔들어준다.
FVector UBTTask_MonsterRetreat::ComputeAwayDirection(const ACharacter* Character, const AActor* Target) const
{
	FVector AwayDir = Character->GetActorLocation() - Target->GetActorLocation();
	AwayDir.Z = 0.f;

	// 타겟과 거의 겹쳐 있으면 방향을 구할 수 없다. 이때는 그냥 등 뒤로 물러난다.
	if (!AwayDir.Normalize())
	{
		AwayDir = -Character->GetActorForwardVector();
		AwayDir.Z = 0.f;
		if (!AwayDir.Normalize())
		{
			AwayDir = FVector::ForwardVector;
		}
	}

	if (RetreatSpreadAngle > 0.f)
	{
		AwayDir = AwayDir.RotateAngleAxis(FMath::FRandRange(-RetreatSpreadAngle, RetreatSpreadAngle), FVector::UpVector);
	}

	return AwayDir;
}

bool UBTTask_MonsterRetreat::ComputeRetreatLocation(AAIController* AIController, const FVector& FromLocation,
	const FVector& AwayDir, float DesiredDistance, FVector& OutLocation)
{
	UNavigationSystemV1* NavSystem = UNavigationSystemV1::GetCurrent(GetWorld());
	if (!NavSystem)
	{
		return false;
	}

	const float MinDistanceSq = FMath::Square(MinRetreatDistance);

	float Distance = DesiredDistance;
	for (int32 Retry = 0; Retry <= NavRetryCount; ++Retry, Distance *= 0.5f)
	{
		// 목표 지점이 네비메시 위에 없으면(허공, 절벽 아래 등) 거리를 줄여 다시 시도한다.
		FNavLocation ProjectedLocation;
		if (!NavSystem->ProjectPointToNavigation(FromLocation + AwayDir * Distance, ProjectedLocation, NavProjectExtent))
		{
			continue;
		}

		// 끝점만 검사하면 중간의 구멍이나 절벽을 건너뛰어 버린다. 세그먼트 전체를 본다.
		// NavigationRaycast는 막혔으면 true를 반환하고, 네비 데이터가 아예 없어도 true를 반환한다.
		FVector CandidateLocation = ProjectedLocation.Location;
		FVector HitLocation;
		if (UNavigationSystemV1::NavigationRaycast(this, FromLocation, ProjectedLocation.Location, HitLocation, nullptr, AIController))
		{
			// 막힌 지점 직전까지만 물러난다.
			CandidateLocation = HitLocation;
		}

		// 그마저도 너무 짧으면 후퇴로 쳐주지 않는다.
		if (FVector::DistSquared2D(FromLocation, CandidateLocation) < MinDistanceSq)
		{
			continue;
		}

		OutLocation = CandidateLocation;
		return true;
	}

	return false;
}

bool UBTTask_MonsterRetreat::StartRetreatMontage(UAnimInstance* AnimInstance)
{
	bMontagePlayStarted = false;

	if (!AnimInstance || !RetreatMontage)
	{
		return false;
	}

	// 음수 PlayRate로 역재생할 때는 시작 위치를 몽타주 끝으로 잡아줘야 한다.
	// 0에서 역재생을 시작하면 첫 프레임에 그대로 끝나버린다.
	const float StartPosition = (PlayRate < 0.f) ? RetreatMontage->GetPlayLength() : 0.f;

	// Montage_Play는 재생 길이를 반환한다. 0이면 재생에 실패한 것이다.
	bMontagePlayStarted = (AnimInstance->Montage_Play(RetreatMontage, PlayRate,
		EMontagePlayReturnType::MontageLength, StartPosition) > 0.f);

	if (bMontagePlayStarted && bDisableMontageRootMotion)
	{
		// 이 재생 인스턴스 한 번에 한해서만 루트 모션을 끈다.
		// 에셋도, AnimInstance의 RootMotionMode 전역 설정도 건드리지 않는다.
		// 루트 모션이 살아 있으면 PerformMovement가 Velocity를 애니메이션 값으로 통째로 덮어써서
		// MoveTo도 LaunchCharacter도 전부 무시된다.
		if (FAnimMontageInstance* MontageInstance = AnimInstance->GetActiveInstanceForMontage(RetreatMontage))
		{
			MontageInstance->PushDisableRootMotion();
		}
	}

	return bMontagePlayStarted;
}

void UBTTask_MonsterRetreat::OnMoveCompleted(FAIRequestID RequestID, const FPathFollowingResult& Result)
{
	if (!CachedBTComp || bTaskFinished || bMoveFinished)
	{
		return;
	}

	// OnRequestFinished는 컨트롤러의 PathFollowingComponent가 들고 있는 공용 델리게이트다.
	// 이 태스크가 낸 요청이 아니면 무시해야 한다.
	if (MoveRequestID.IsValid() && !RequestID.IsEquivalent(MoveRequestID))
	{
		return;
	}

	bMoveFinished = true;
	bMoveSucceeded = Result.IsSuccess();

	UE_LOG(LogTemp, Log, TEXT("MonsterRetreat: 후퇴 이동 종료 MoveId=%u Success=%d Code=%d"),
		RequestID.GetID(), bMoveSucceeded ? 1 : 0, (int32)Result.Code);

	TryFinish(*CachedBTComp);
}

void UBTTask_MonsterRetreat::OnCharacterLanded(const FHitResult& Hit)
{
	if (!CachedBTComp || bTaskFinished || bMoveFinished)
	{
		return;
	}

	bMoveFinished = true;
	bMoveSucceeded = true;

	UE_LOG(LogTemp, Log, TEXT("MonsterRetreat: 백점프 착지"));

	TryFinish(*CachedBTComp);
}

void UBTTask_MonsterRetreat::TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	if (bTaskFinished)
	{
		return;
	}

	ElapsedTime += DeltaSeconds;

	// 이동 완료나 착지 알림이 끝내 오지 않는 상황(경로 소실, 발판 없는 낙하 등)에 대한 안전장치.
	// 여기까지 왔다면 몬스터는 이미 그 시간만큼 물러난 뒤이므로 성공으로 끝낸다.
	if (ElapsedTime >= MaxRetreatTime)
	{
		UE_LOG(LogTemp, Warning, TEXT("MonsterRetreat: %.1f초 타임아웃으로 태스크를 종료한다."), MaxRetreatTime);
		FinishTask(OwnerComp, EBTNodeResult::Succeeded);
		return;
	}

	TryFinish(OwnerComp);
}

EBTNodeResult::Type UBTTask_MonsterRetreat::AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	CleanupRetreat(OwnerComp.GetAIOwner());

	bTaskFinished = true;
	CachedBTComp = nullptr;
	CachedCharacter = nullptr;

	UE_LOG(LogTemp, Log, TEXT("MonsterRetreat: AbortTask"));

	return Super::AbortTask(OwnerComp, NodeMemory);
}

void UBTTask_MonsterRetreat::TryFinish(UBehaviorTreeComponent& OwnerComp)
{
	if (bTaskFinished || !bMoveFinished)
	{
		return;
	}

	if (bWaitForMontage && !IsMontageDone(GetAnimInstance(OwnerComp)))
	{
		return;
	}

	FinishTask(OwnerComp, bMoveSucceeded ? EBTNodeResult::Succeeded : EBTNodeResult::Failed);
}

void UBTTask_MonsterRetreat::FinishTask(UBehaviorTreeComponent& OwnerComp, EBTNodeResult::Type Result)
{
	if (bTaskFinished)
	{
		return;
	}

	bTaskFinished = true;

	CleanupRetreat(OwnerComp.GetAIOwner());

	CachedBTComp = nullptr;
	CachedCharacter = nullptr;

	FinishLatentTask(OwnerComp, Result);
}

void UBTTask_MonsterRetreat::CleanupRetreat(AAIController* AIController)
{
	UnbindAll(AIController);

	if (AIController)
	{
		// 몸통을 타겟에 고정하던 포커스를 반드시 푼다.
		// 남겨두면 이후의 평범한 MoveTo까지 계속 타겟을 바라본 채 이동하게 된다.
		AIController->ClearFocus(EAIFocusPriority::Gameplay);

		if (MoveMode == ERetreatMoveMode::Backstep && !bMoveFinished && MoveRequestID.IsValid())
		{
			AIController->StopMovement();
		}
	}

	if (CachedCharacter)
	{
		if (SavedMaxWalkSpeed > 0.f)
		{
			if (UCharacterMovementComponent* MoveComp = CachedCharacter->GetCharacterMovement())
			{
				MoveComp->MaxWalkSpeed = SavedMaxWalkSpeed;
			}
			SavedMaxWalkSpeed = 0.f;
		}

		if (RetreatMontage)
		{
			if (USkeletalMeshComponent* MeshComp = CachedCharacter->GetMesh())
			{
				if (UAnimInstance* AnimInstance = MeshComp->GetAnimInstance())
				{
					if (AnimInstance->Montage_IsPlaying(RetreatMontage))
					{
						AnimInstance->Montage_Stop(MontageBlendOutTime, RetreatMontage);
					}
				}
			}
		}
	}
}

void UBTTask_MonsterRetreat::UnbindAll(AAIController* AIController)
{
	if (AIController)
	{
		if (UPathFollowingComponent* PathFollowingComp = AIController->GetPathFollowingComponent())
		{
			PathFollowingComp->OnRequestFinished.RemoveAll(this);
		}
	}

	if (CachedCharacter)
	{
		CachedCharacter->LandedDelegate.RemoveDynamic(this, &UBTTask_MonsterRetreat::OnCharacterLanded);
	}
}

bool UBTTask_MonsterRetreat::IsMontageDone(UAnimInstance* AnimInstance) const
{
	if (!bMontagePlayStarted || !AnimInstance || !RetreatMontage)
	{
		return true;
	}

	// Montage_Play 직후 첫 프레임에는 아직 재생 중으로 잡히지 않을 수 있다.
	if (ElapsedTime < MontageStartGraceTime2)
	{
		return false;
	}

	return !AnimInstance->Montage_IsPlaying(RetreatMontage);
}

UAnimInstance* UBTTask_MonsterRetreat::GetAnimInstance(UBehaviorTreeComponent& OwnerComp)
{
	AAIController* AIController = OwnerComp.GetAIOwner();
	ACharacter* Character = AIController ? Cast<ACharacter>(AIController->GetPawn()) : nullptr;
	USkeletalMeshComponent* MeshComp = Character ? Character->GetMesh() : nullptr;

	return MeshComp ? MeshComp->GetAnimInstance() : nullptr;
}
 