// Fill out your copyright notice in the Description page of Project Settings.


#include "BTTask_PlayMontageAndMove.h"

#include "AIController.h"
#include "GameFramework/Character.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/BlackboardData.h"
#include "MonsterAITypes.h"


namespace
{
	// Montage_Play 직후 첫 프레임에는 Montage_IsPlaying이 아직 false일 수 있다.
	// 이 시간이 지나기 전에는 몽타주가 끝났다고 판단하지 않는다.
	constexpr float MontageStartGraceTime = 0.1f;

	// AbortTask에서 몽타주를 정지시킬 때의 블렌드 아웃 시간(초)
	constexpr float AbortBlendOutTime = 0.2f; 
}

UBTTask_PlayMontageAndMove::UBTTask_PlayMontageAndMove()
{
	NodeName = TEXT("Play Montage And Move");
	bNotifyTick = true;

	// 이 노드는 진행 중인 이동 ID와 완료 플래그를 멤버로 들고 있다.
	// 노드 인스턴스를 만들지 않으면 그 상태를 같은 트리를 쓰는 모든 몬스터가 공유해
	// 서로의 이동 완료 처리를 덮어쓴다.
	bCreateNodeInstance = true;

	// 블랙보드 키가 특정 클래스 또는 그 하위 클래스의 오브젝트만 값으로 가질 수 있도록 제한한다.
	// (BTService_UpdateMonsterState와 동일한 패턴)
	TargetKey.AddObjectFilter(this, GET_MEMBER_NAME_CHECKED(UBTTask_PlayMontageAndMove, TargetKey), AActor::StaticClass());
	AttackType.AddEnumFilter(this, GET_MEMBER_NAME_CHECKED(UBTTask_PlayMontageAndMove, AttackType), StaticEnum<EMonsterAttackType>());
}

//에셋이 준비되는 단계(처음 트리 파일이 열리거나 초기화될 때)에서 단 한 번만 호출되므로, 정적 데이터 파싱 및 초기화에 최적화되어 있다.
void UBTTask_PlayMontageAndMove::InitializeFromAsset(UBehaviorTree& Asset)
{
	Super::InitializeFromAsset(Asset);

	if (UBlackboardData* BBAsset = GetBlackboardAsset())
	{
		// 이 프로퍼티가 생기기 전에 저장된 노드는 키가 비어 있다.
		// 예전 동작(TargetActor 하드코딩)과 맞추기 위해 기본값을 채워준다.
		if (TargetKey.SelectedKeyName.IsNone())
		{
			TargetKey.SelectedKeyName = TEXT("TargetActor");
		}

		TargetKey.ResolveSelectedKey(*BBAsset);
	}
}


//UE_LOG(LogTemp, Log, TEXT("PlayMontage: Montage=%s Rate=%.2f"), *GetNameSafe(MontageToPlay), PlayRate);

EBTNodeResult::Type UBTTask_PlayMontageAndMove::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	AAIController* AIController = OwnerComp.GetAIOwner();
	ACharacter* Character = AIController ? Cast<ACharacter>(AIController->GetPawn()) : nullptr;
	UAnimInstance* AnimInstance = Character ? Character->GetMesh()->GetAnimInstance() : nullptr;
	UBlackboardComponent* BlackboardComp = OwnerComp.GetBlackboardComponent();


	if (!AIController || !AnimInstance || !MontageToPlay || !BlackboardComp)
	{
		return EBTNodeResult::Failed;
	}

	// 실행할 때마다 상태를 초기화한다.
	CachedBTComp = &OwnerComp;
	MoveRequestID = FAIRequestID::InvalidRequest;
	ElapsedTime = 0.f;
	bMontagePlayStarted = false;
	bMoveFinished = false;
	bMoveSucceeded = false;
	bTaskFinished = false;

	// 블랙보드에서 TargetActor값을 가져온다.
	// TargetActor는 Object(AActor) 키다. GetValueAsVector로 읽으면 값이 아니라
	// UBlackboardKeyType_Vector::InvalidValue(= FAISystem::InvalidLocation, FLT_MAX)가 돌아오고,
	// MoveTo는 목적지 유효성 검사에서 걸려 경로 탐색도 하지 않고 Failed를 반환한다.
	AActor* TargetActor = Cast<AActor>(BlackboardComp->GetValueAsObject(TargetKey.SelectedKeyName));
	if (!TargetActor)
	{
		UE_LOG(LogTemp, Warning, TEXT("PlayMontageAndMove: 블랙보드 키 '%s'에 유효한 액터가 없다."),
			*TargetKey.SelectedKeyName.ToString());
		CachedBTComp = nullptr;
		return EBTNodeResult::Failed;
	}

	// goal actor로 요청하면 타겟이 움직여도 경로가 계속 갱신된다.
	FAIMoveRequest MoveRequest(TargetActor);
	MoveRequest.SetAcceptanceRadius(AcceptanceRadius);

	// 델리게이트는 반드시 MoveTo "뒤에" 건다.
	// MoveTo는 이미 목표에 도착해 있거나 즉시 실패한 경우 호출 스택 안에서 곧바로
	// OnRequestFinished를 브로드캐스트한다. 미리 걸어두면 몽타주를 재생하기도 전에
	// 콜백이 들어와 태스크가 끝나버린다.
	const FPathFollowingRequestResult RequestResult = AIController->MoveTo(MoveRequest);
	MoveRequestID = RequestResult.MoveId;

	UE_LOG(LogTemp, Log, TEXT("PlayMontageAndMove: MoveTo Target=%s Goal=(%s) Code=%d MoveId=%u"),
		*GetNameSafe(TargetActor), *TargetActor->GetActorLocation().ToCompactString(),
		(int32)RequestResult.Code, MoveRequestID.GetID());

	// 이동 경로 탐색 실패 등의 이유로 시작조차 못 한 경우
	if (RequestResult.Code == EPathFollowingRequestResult::Failed)
	{
		UE_LOG(LogTemp, Warning, TEXT("PlayMontageAndMove: MoveTo 요청 실패"));
		CachedBTComp = nullptr;
		return EBTNodeResult::Failed;
	}

	// 이미 목표치에 도달해 있는 등의 이유로 즉시 성공한 경우.
	// 이동은 끝났지만 몽타주는 아직 재생해야 하므로 태스크를 여기서 끝내지 않는다.
	if (RequestResult.Code == EPathFollowingRequestResult::AlreadyAtGoal)
	{
		bMoveFinished = true;
		bMoveSucceeded = true;
		bMontagePlayStarted = (AnimInstance->Montage_Play(MontageToPlay, PlayRate) > 0.f);
	}
	else if (UPathFollowingComponent* PathFollowingComp = AIController->GetPathFollowingComponent())
	{
		PathFollowingComp->OnRequestFinished.AddUObject(this, &UBTTask_PlayMontageAndMove::OnMoveCompleted);
	}

	// Montage_Play는 재생 길이를 반환한다. 0이면 재생에 실패한 것이므로 몽타주를 기다리지 않는다.
	//bMontagePlayStarted = (AnimInstance->Montage_Play(MontageToPlay, PlayRate) > 0.f);

	// 이동이 이미 끝났고 몽타주를 기다릴 필요도 없으면 곧바로 성공 처리
	if (bMoveFinished && (!bWaitForCompletion || !bMontagePlayStarted))
	{
		UnbindMoveDelegate(AIController);
		CachedBTComp = nullptr;
		bTaskFinished = true;
		UE_LOG(LogTemp, Log, TEXT("PlayMontageAndMove: 즉시 성공(AlreadyAtGoal)"));
		return EBTNodeResult::Succeeded;
	}

	return EBTNodeResult::InProgress;
}

void UBTTask_PlayMontageAndMove::OnMoveCompleted(FAIRequestID RequestID, const FPathFollowingResult& Result)
{
	if (!CachedBTComp || bTaskFinished || bMoveFinished)
	{
		return;
	}

	// OnRequestFinished는 컨트롤러의 PathFollowingComponent가 들고 있는 공용 델리게이트다.
	// 이 태스크가 낸 요청이 아니면(예: 같은 트리의 기본 BTTask_MoveTo) 무시해야 한다.
	if (MoveRequestID.IsValid() && !RequestID.IsEquivalent(MoveRequestID))
	{
		return;
	}

	UBehaviorTreeComponent& OwnerComp = *CachedBTComp;

	// 다음 이동을 위해 델리게이트 바인딩 해제
	UnbindMoveDelegate(OwnerComp.GetAIOwner());

	bMoveFinished = true;
	bMoveSucceeded = Result.IsSuccess();

	UE_LOG(LogTemp, Log, TEXT("PlayMontageAndMove: OnMoveCompleted MoveId=%u Success=%d Code=%d"),
		RequestID.GetID(), bMoveSucceeded ? 1 : 0, (int32)Result.Code);

	// 이동 실패는 몽타주 종료를 기다리지 않고 즉시 실패 처리한다.
	if (!bMoveSucceeded)
	{
		FinishTask(OwnerComp, EBTNodeResult::Failed);
		return;
	}

	// Montage_Play는 재생 길이를 반환한다. 0이면 재생에 실패한 것이므로 몽타주를 기다리지 않는다.
	AAIController* AIController = OwnerComp.GetAIOwner();
	ACharacter* Character = Cast<ACharacter>(AIController->GetPawn());
	UAnimInstance* AnimInstance = Character->GetMesh()->GetAnimInstance();
	bMontagePlayStarted = (AnimInstance->Montage_Play(MontageToPlay, PlayRate) > 0.f);


	// 비헤이비어 트리에서 비동기로 동작하는 커스텀 태스크의 작업 완료를 시스템에 알리고 다음 노드로 넘어가기 위해 호출하는 함수
	TryFinish(OwnerComp);
}


void UBTTask_PlayMontageAndMove::TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	if (bTaskFinished)
	{
		return;
	}

	ElapsedTime += DeltaSeconds;

	if (!GetAnimInstance(OwnerComp))
	{
		UE_LOG(LogTemp, Warning, TEXT("PlayMontageAndMove: AnimInstance 소실"));
		FinishTask(OwnerComp, EBTNodeResult::Failed);
		return;
	}

	TryFinish(OwnerComp);
}

EBTNodeResult::Type UBTTask_PlayMontageAndMove::AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	AAIController* AIController = OwnerComp.GetAIOwner();

	UnbindMoveDelegate(AIController);

	if (AIController && !bMoveFinished && MoveRequestID.IsValid())
	{
		AIController->StopMovement();
	}

	if (UAnimInstance* AnimInstance = GetAnimInstance(OwnerComp))
	{
		if (MontageToPlay && AnimInstance->Montage_IsPlaying(MontageToPlay))
		{
			AnimInstance->Montage_Stop(AbortBlendOutTime, MontageToPlay);
		}
	}

	bTaskFinished = true;
	CachedBTComp = nullptr;

	UE_LOG(LogTemp, Log, TEXT("PlayMontageAndMove: AbortTask"));

	return Super::AbortTask(OwnerComp, NodeMemory);
}

void UBTTask_PlayMontageAndMove::TryFinish(UBehaviorTreeComponent& OwnerComp)
{
	if (bTaskFinished || !bMoveFinished)
	{
		return;
	}

	if (bWaitForCompletion && !IsMontageDone(GetAnimInstance(OwnerComp)))
	{
		return;
	}

	UBlackboardComponent* BlackboardComp = OwnerComp.GetBlackboardComponent();
	BlackboardComp->SetValueAsEnum(AttackType.SelectedKeyName, static_cast<uint8>(EMonsterAttackType::AK_None));

	UE_LOG(LogTemp, Log, TEXT("PlayMontageAndMove: 완료 (이동 성공=%d)"), bMoveSucceeded ? 1 : 0);
	FinishTask(OwnerComp, bMoveSucceeded ? EBTNodeResult::Succeeded : EBTNodeResult::Failed);
}

bool UBTTask_PlayMontageAndMove::IsMontageDone(UAnimInstance* AnimInstance) const
{
	if (!bMontagePlayStarted || !AnimInstance || !MontageToPlay)
	{
		return true;
	}

	// Montage_Play 직후 첫 프레임에는 아직 재생 중으로 잡히지 않을 수 있다.
	if (ElapsedTime < MontageStartGraceTime)
	{
		return false;
	}

	return !AnimInstance->Montage_IsPlaying(MontageToPlay);
}

void UBTTask_PlayMontageAndMove::FinishTask(UBehaviorTreeComponent& OwnerComp, EBTNodeResult::Type Result)
{
	if (bTaskFinished)
	{
		return;
	}

	bTaskFinished = true;
	UnbindMoveDelegate(OwnerComp.GetAIOwner());
	CachedBTComp = nullptr;

	FinishLatentTask(OwnerComp, Result);
}

void UBTTask_PlayMontageAndMove::UnbindMoveDelegate(AAIController* AIController)
{
	if (AIController)
	{
		if (UPathFollowingComponent* PathFollowingComp = AIController->GetPathFollowingComponent())
		{
			PathFollowingComp->OnRequestFinished.RemoveAll(this);
		}
	}
}

UAnimInstance* UBTTask_PlayMontageAndMove::GetAnimInstance(UBehaviorTreeComponent& OwnerComp)
{
	AAIController* AIController = OwnerComp.GetAIOwner();
	ACharacter* Character = AIController ? Cast<ACharacter>(AIController->GetPawn()) : nullptr;
	USkeletalMeshComponent* MeshComp = Character ? Character->GetMesh() : nullptr;

	return MeshComp ? MeshComp->GetAnimInstance() : nullptr;
}
