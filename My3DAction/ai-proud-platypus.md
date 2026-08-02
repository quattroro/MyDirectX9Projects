# Monster AI 기본 구조 (Behavior Tree) 설계

## Context

`My3DAction` 프로젝트의 몬스터(`AMonster_Usurper`, 드래곤)는 현재 피격 시 셰이더 이펙트만 재생하는 더미 상태다. AI 컨트롤러, 비헤이비어 트리, 블랙보드, 체력/데미지 시스템이 전혀 없고, `AIModule`조차 빌드 의존성에 없다. 애니메이션 블루프린트(`AMP_Monster`)도 Idle 상태 하나뿐이다.

최종 목표는 몬스터헌터 스타일의 정교한 AI다:
- **비전투 대기 행동**: 배회, 먹이 섭취, 잠자기 등 여러 패시브 행동
- **전투 전환**: 플레이어 감지 시 포효(Roar) 연출과 함께 전투 모드 진입
- **전투 중 공격 패턴**: 여러 공격 중 선택
- **광폭화(Enrage)**: 체력 임계치 이하에서 전투 방식이 바뀌는 상태

이번 작업 범위는 그 최종 그림으로 자연스럽게 확장 가능한 **기본 BT 골격**만 구축하는 것이다. 실제 콘텐츠(다양한 공격, 실제 Eat/Sleep 애니메이션, 부위파괴 등)는 이번 범위에서 의도적으로 제외하되, 나중에 트리 구조를 다시 짜지 않아도 되도록 설계한다.

프레임워크는 사용자와 논의 후 **Behavior Tree + Blackboard**로 확정했다 (업계 표준, StateTree/커스텀 FSM은 재검토하지 않음).

### 사전 확인 사항

- **고아 에셋 발견**: `Content/My3DAction/Monster/Animation/BT_Monster_Usurper.uasset` 파일이 이미 디스크에 존재하지만 git에는 untracked이고, 에디터 에셋 레지스트리에도 잡히지 않는다. 이전 세션의 중단된 시도로 보인다. 구현 1단계에서 이 파일을 에디터로 열어 확인 후, 비어있거나 깨져있으면 삭제하고 Content Browser에서 새로 생성한다.
- **사용 가능한 원본 애니메이션**: `Content/FourEvilDragonsHP/Animations/DragonTheUsurper/`에 `Idle01Anim, Idle02Anim, WalkAnim, RunAnim, ScreamAnim(포효), SleepAnim, AttackFlameAnim, AttackHandAnim, AttackMouthAnim, DefendAnim, GetHitAnim, DieAnim` 등이 이미 존재한다 (마켓플레이스 팩 원본). `ScreamAnim`은 포효, `SleepAnim`은 향후 Sleep 행동, 3종 Attack 애니메이션은 향후 공격 패턴 다양화에 바로 쓸 수 있다.
- **스켈레톤 호환성 확인 필요**: 이 애니메이션들은 `Content/FourEvilDragonsHP/Meshes/DragonTheUsurper/DragonTheUsurper_Skeleton`을 기준으로 만들어졌고, `BP_Monster`는 프로젝트 로컬 사본인 `DragonTheUsurper_Skeleton1`을 사용한다. 두 스켈레톤이 동일/호환되는지 에디터에서 한 번 확인 필요 (Skeleton Editor의 "Compatible Skeletons" 확인, 필요시 추가). 몽타주 제작(5단계) 전에 반드시 확인.
- **`unreal-cli asset find/info`가 현재 세션에서 오작동** (경로 중복, 존재하는 에셋도 0건 반환). BT/BB 관련 작업은 CLI 검색에 의존하지 말고 Content Browser UI 또는 `read-log`/`level inspect`/`screenshot`으로 검증한다.

---

## 1. Build.cs 변경

`Source/My3DAction/My3DAction.Build.cs`:

```csharp
PublicDependencyModuleNames.AddRange(new string[] {
    "Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput",
    "AIModule", "GameplayTasks", "NavigationSystem"
});
```

- `AIModule`: `AAIController`, `UBehaviorTree`, `UBlackboardComponent`, `UAIPerceptionComponent`, `BTTaskNode`/`BTService`/`BTDecorator` 기반 클래스.
- `GameplayTasks`: AIModule의 BT 태스크들이 의존.
- `NavigationSystem`: Wander 태스크에서 `UNavigationSystemV1::GetRandomReachablePointInRadius` 사용.

**중요**: `PublicDependencyModuleNames` 변경은 보통 Hot Reload(`compile`)로 반영되지 않는다. 이 변경 직후 **에디터를 닫고 전체 리빌드 후 재시작**해야 한다 (CLAUDE.md의 플러그인 리빌드 규칙과 동일한 이유, 다만 이번엔 게임 모듈 대상).

---

## 2. 신규/수정 C++ 클래스

기존 프로젝트는 `Source/My3DAction/` 아래 서브폴더 없이 평평한(flat) 구조이므로 동일하게 유지한다.

### 2.1 `AMonsterAIController` (신규) — `MonsterAIController.h/.cpp`

```cpp
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
    FVector HomeLocation; // OnPossess 시점 스폰 위치, Wander 반경 기준점
};
```

- 생성자에서 `AIPerceptionComp` + `SightConfig`(Sight, Pawn 감지) 구성.
- `OnPossess`: `Super::OnPossess` → 블랙보드 초기화(`UseBlackboard`) → `HomeLocation` 캐싱 → `RunBehaviorTree(BehaviorTreeAsset)`.
- `OnTargetPerceptionUpdated`: 플레이어를 감지하면 `TargetActor` 블랙보드 키만 기록. 상태 전이 로직(Passive→Alert→Combat)은 여기서 하지 않고 2.3의 Service가 전담 — 상태 쓰기를 한 곳에 집중시켜 경쟁 조건을 피한다.
- `BehaviorTreeAsset`은 `EditDefaultsOnly`로 비워두고 실제 BT 에셋은 이 컨트롤러의 블루프린트 자식(`BP_MonsterAIController`)에서 지정 — `BP_Monster`가 `AMonster_Usurper`를 감싸는 기존 패턴과 동일한 방식.

### 2.2 체력 추가 — `AMonster_Usurper` 수정

몬스터 종류가 하나뿐이고 이미 `AMonster_Usurper`가 자체 상태(`DynamicMaterialInst`)를 갖고 있으므로, 별도 `UHealthComponent`를 만들지 않고 기존 클래스에 최소한으로 추가한다 (몬스터 종류가 늘어나면 그때 컴포넌트로 승격).

`Monster_Usurper.h` 추가:
```cpp
protected:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Health", meta = (AllowPrivateAccess = "true"))
    float MaxHealth = 1000.f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Health", meta = (AllowPrivateAccess = "true"))
    float Health;

public:
    UFUNCTION(BlueprintCallable, Category = "Health")
    void ApplyDamage(float DamageAmount);

    UFUNCTION(BlueprintCallable, Category = "Health")
    float GetHealthPercent() const { return MaxHealth > 0.f ? Health / MaxHealth : 0.f; }

    UFUNCTION(BlueprintCallable, Category = "Health")
    bool IsDead() const { return Health <= 0.f; }
```

- `BeginPlay()`에 `Health = MaxHealth;` 추가.
- `ApplyDamage`는 이번 범위에서 단순 감산 + 로그만 (사망 처리/랙돌은 제외, `IsDead()`는 나중에 BT가 바로 쓸 수 있도록 미리 만들어둠).
- **`Hit(FVector pos, FVector dir)`와는 분리 유지** — `Hit()`는 VFX 전용으로 그대로 두고, `MainCharacter::OnSwordBeinOverlap`(기존 `monster->Hit(ImpactPoint, ImpactNormal)` 호출부 바로 옆)에 `monster->ApplyDamage(50.f);` 를 추가한다. 고정값은 이번 범위에서 테스트 가능하게만 만드는 임시값(무기 데미지 스탯 시스템은 범위 밖).

### 2.3 `UBTService_UpdateMonsterState` (신규) — `BTService_UpdateMonsterState.h/.cpp`

트리의 유일한 "두뇌" 노드. 모든 상태 전이 블랙보드 키의 유일한 작성자로, 루트 Selector에 Service로 부착한다.

```cpp
UCLASS()
class MY3DACTION_API UBTService_UpdateMonsterState : public UBTService
{
    GENERATED_BODY()
public:
    UBTService_UpdateMonsterState();

protected:
    virtual void TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;

    UPROPERTY(EditAnywhere, Category = "Blackboard")
    FBlackboardKeySelector TargetActorKey, CombatStateKey, DistanceToTargetKey, HealthPctKey;

    UPROPERTY(EditAnywhere, Category = "Monster AI")
    float EnrageHealthPctThreshold = 0.3f;

    UPROPERTY(EditAnywhere, Category = "Monster AI")
    float AlertToCombatDelay = 1.5f; // Roar 몽타주 길이와 대략 맞춤

private:
    float TimeInAlertState = 0.f;
};
```

생성자에서 `Interval = 0.5f; RandomDeviation = 0.1f;` (BT Service 관례 — 매 틱이 아닌 저비용 주기 폴링).

`TickNode` 로직 (이번 범위의 상태 머신 전체):
1. 몬스터 폰의 `GetHealthPercent()` → `HealthPctKey`에 기록.
2. `TargetActorKey`는 이미 컨트롤러가 써둔 값을 읽기만 함 (Perception 소유권은 컨트롤러에 유지).
3. 상태 계산:
   - `HealthPct <= EnrageHealthPctThreshold` && 타겟 있음 → `Enrage`.
   - 타겟 있음 && 현재 `Passive` → `Alert` (진입 시 `TimeInAlertState = 0`).
   - 현재 `Alert` → `TimeInAlertState` 누적, `AlertToCombatDelay` 넘으면 → `Combat`.
   - 현재 `Combat`/`Enrage`이고 타겟 소실/체력 회복 → **이번 범위에서는 하향 전이 없음** (의도적 보류 — "이탈" 로직은 별도 설계 질문).
   - 그 외 → `Passive` 유지.
4. `DistanceToTarget` 계산 후 기록 (향후 거리 기반 공격 선택을 위한 스캐폴딩, 이번 범위에선 게이팅에 사용 안 함).
5. `CombatStateKey`에 Enum 값 기록.

### 2.4 `EMonsterCombatState` (신규 enum) — 작은 공유 헤더 `MonsterAITypes.h`에 선언 (Service가 AIController 전체 헤더를 끌어오지 않도록)

```cpp
UENUM(BlueprintType)
enum class EMonsterCombatState : uint8
{
    Passive  UMETA(DisplayName = "Passive"),
    Alert    UMETA(DisplayName = "Alert"),   // 포효 전환 구간
    Combat   UMETA(DisplayName = "Combat"),
    Enrage   UMETA(DisplayName = "Enrage")
};
```

`UENUM(BlueprintType)`이면 블랙보드 키 타입(Enum)으로 바로 선택 가능 — 별도 Blackboard-Enum 에셋 불필요.

### 2.5 `UBTTask_FindRandomWanderPoint` (신규) — `BTTask_FindRandomWanderPoint.h/.cpp`

`ExecuteTask`에서 컨트롤러의 `HomeLocation` 기준 `UNavigationSystemV1::GetRandomReachablePointInRadius`로 지점을 찾아 `WanderLocation` 키에 기록. 실제 이동은 표준 `BTTask_MoveTo`가 그 키를 소비하므로 커스텀 이동 코드는 불필요.

### 2.6 `UBTTask_PlayMontageAndWait` (신규) — `BTTask_PlayMontageAndWait.h/.cpp`

범용/재사용 목적 — Roar에도, 향후 모든 공격 몽타주에도 이 하나로 재사용 (공격마다 새 클래스 안 만들어도 됨).

```cpp
UCLASS()
class MY3DACTION_API UBTTask_PlayMontageAndWait : public UBTTaskNode
{
    GENERATED_BODY()
public:
    UBTTask_PlayMontageAndWait();
protected:
    virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
    virtual void TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;

    UPROPERTY(EditAnywhere, Category = "Animation")
    UAnimMontage* MontageToPlay;

    UPROPERTY(EditAnywhere, Category = "Animation")
    bool bWaitForCompletion = true;
};
```

`ExecuteTask`는 `MainCharacter::Attack()`과 동일한 패턴으로 `Montage_Play` 호출. `bWaitForCompletion`이면 `InProgress` 반환 후 `TickTask`에서 `Montage_IsPlaying` 폴링 (기존 `AMainCharacter::Attacking()`과 동일 아이디어를 범용화).

커스텀 Decorator는 이번 범위에서 불필요 — `CombatState`(Enum)는 표준 Blackboard 데코레이터의 "Is Equal To", `TargetActor`(Object)는 "Is Set"으로 충분.

---

## 3. 블랙보드 키 (`BB_Monster_Usurper`)

| 키 | 타입 | 작성 주체 | 용도 |
|---|---|---|---|
| `TargetActor` | Object(AActor) | `AMonsterAIController::OnTargetPerceptionUpdated` | 감지된 플레이어. Alert/Combat/Enrage 게이팅 |
| `CombatState` | Enum(`EMonsterCombatState`) | `UBTService_UpdateMonsterState` | 루트 Selector 브랜치 게이팅 |
| `HomeLocation` | Vector | `OnPossess`(1회) | Wander 반경 기준점 |
| `WanderLocation` | Vector | `UBTTask_FindRandomWanderPoint` | `MoveTo`가 소비할 목적지 |
| `DistanceToTarget` | Float | `UBTService_UpdateMonsterState` | 향후 거리 기반 공격 선택용 스캐폴딩 |
| `HealthPct` | Float(0-1) | `UBTService_UpdateMonsterState` | 디버그/향후 세분화된 Enrage 단계용 |

---

## 4. Behavior Tree 구조 (`BT_Monster_Usurper`)

```
Root
└─ Selector "Root"                         [Service: BTService_UpdateMonsterState, 0.5s±0.1s]
   ├─ (CombatState == Enrage, Observer Aborts = Lower Priority)
   │  └─ Sequence "Enrage"
   │     ├─ MoveTo (TargetActor)
   │     └─ Selector "Enrage Attack Selector"     ← 나중에 형제 추가할 wrapper
   │        └─ Sequence "Attack(placeholder)" → PlayMontageAndWait → Wait(0.5s)
   │
   ├─ (CombatState == Combat, Observer Aborts = Lower Priority)
   │  └─ Sequence "Combat"
   │     ├─ MoveTo (TargetActor)
   │     └─ Selector "Attack Pattern Selector"    ← 나중에 형제 추가할 wrapper
   │        └─ Sequence "Attack(placeholder)" → PlayMontageAndWait → Wait(1.5~2.5s)
   │
   ├─ (CombatState == Alert, Observer Aborts = Lower Priority)
   │  └─ Sequence "Alert/Roar"
   │     └─ PlayMontageAndWait(ScreamAnim 기반 몽타주) → Wait(0.1s)
   │        ↳ Alert→Combat 전이 자체는 Service의 AlertToCombatDelay가 시간 기반으로 처리 (이 브랜치는 연출용)
   │
   └─ (데코레이터 없음, 반드시 마지막 자식 = 기본값)
      └─ Selector "Passive Behavior Selector"     ← 나중에 Eat/Sleep 형제 추가할 wrapper
         └─ Sequence "Wander(Eat/Sleep 자리)"
            ├─ FindRandomWanderPoint → MoveTo(WanderLocation) → Wait(3s±1s)
```

**의도적 보류 사항** (구조는 유지, 나중에 형제 노드만 추가하면 됨):
- Passive: Wander만 구현. Eat/Sleep은 "Passive Behavior Selector" 아래 형제 Sequence로 추가 (예: `SleepAnim` 사용, 재선택 방지용 랜덤 확률 데코레이터는 그때 작은 커스텀 클래스로 추가).
- Combat/Enrage 공격: Selector로 감싸둔 이유가 바로 `AttackHandAnim`/`AttackMouthAnim`/`AttackFlameAnim` 등을 형제 Sequence로 추가하기 위함.
- 거리 기반 공격 선택: 키만 존재, 데코레이터 미연결.
- 이탈(Combat/Enrage → Passive 복귀): 미구현.

---

## 5. AnimBP (`AMP_Monster`) 변경

**이번 범위에서 필요**:
1. `Speed` float 변수 + 최소 로코모션 블렌드(Idle↔Walk↔Run, `Idle01Anim`/`WalkAnim`/`RunAnim` 사용). 없으면 `MoveTo`가 시각적으로 제자리걸음이 되어 PIE 테스트가 어려움. `unreal-cli anim setup-statemachine`으로 Idle/Walk까지 만들고 Run은 수동 추가.
2. **몽타주 슬롯**(`DefaultSlot`) 추가 — `PlayMontageAndWait`가 실제로 재생되려면 필요.
3. 몽타주 에셋 생성: `AM_Monster_Roar`(ScreamAnim 기반), `AM_Monster_Attack_Placeholder`(Attack 3종 중 택1). 스켈레톤 호환성 확인 후 진행.

**이번 범위에서 보류**: 로코모션 폴리시(turn-in-place, strafing), 공격 몽타주의 히트박스 AnimNotify(기존 `UAnim_AttackNotifyState`는 `AMainCharacter`로만 하드캐스팅되어 있어 몬스터에서는 무동작 — 나중에 `IAttackCollisionOwner` 같은 인터페이스로 리팩터링 권장, 지금은 손대지 않음), Sleep/Eat/Die/GetHit 상태.

---

## 6. 작업 순서

1. **정리 + Build.cs**: 고아 `BT_Monster_Usurper.uasset` 확인/정리. Build.cs에 3개 모듈 추가. 에디터 닫고 전체 리빌드 후 재시작. 컴파일만 확인.
2. **AIController 골격**: `AMonsterAIController` 추가(빈 BT 가드), `BP_MonsterAIController` 블루프린트 생성, `BP_Monster`에 Auto Possess AI + AIControllerClass 설정. PIE로 `OnPossess` 로그 확인.
3. **BB/BT 에셋 생성**: Content Browser에서 `BB_Monster_Usurper`, `BT_Monster_Usurper` 생성 및 6개 키 추가 (Python `execute` 스크립트로 자동화 시도 가능, 안 되면 수동 UI).
4. **Passive/Wander 루프**: `UBTTask_FindRandomWanderPoint` 구현, Passive 브랜치 구성, AnimBP에 Speed 로코모션 추가. PIE + BT 디버거로 배회 확인 (수동/시각 확인, CLI 불가).
5. **감지→Alert→Combat 전환 + 포효**: Perception 컴포넌트, `EMonsterCombatState`, `UBTService_UpdateMonsterState`, `UBTTask_PlayMontageAndWait` 구현. `AM_Monster_Roar` 몽타주+슬롯 생성. Alert/Combat 브랜치 구성 (Combat 공격은 임시로 Roar 몽타주 재사용 가능). PIE로 플레이어 접근 시 상태 전이 및 포효 확인.
6. **체력 + Enrage**: `AMonster_Usurper`에 체력 추가, `OnSwordBeinOverlap`에 `ApplyDamage` 연결. Enrage 브랜치 구성 (Combat 구조 복사 + 빠른 쿨다운). PIE로 반복 공격 후 30% 이하에서 Enrage 전환 확인.
7. **전체 회귀 확인**: Passive→Alert→Combat→Enrage 전체 사이클을 BT 디버거로 한 번에 관찰, `read-log --type warning/error`로 블랙보드 키 타입 불일치 등 조용한 오류 점검.

---

## 7. 검증 방법

- **컴파일**: `unreal-cli.exe compile --wait` → `read-log --type error --limit 50` (매 클래스 추가 후).
- **빙의/상태 확인**: `unreal-cli.exe play` → `level inspect --path /Game/My3DAction/My3DAciton_Level --with-values`로 AIController 할당 확인.
- **BT/블랙보드 시각 확인 (수동)**: PIE 중 월드 아웃라이너에서 몬스터 폰 선택 → Behavior Tree Debugger 탭에서 활성 노드와 블랙보드 값 실시간 확인. CLI로는 대체 불가 — 매 단계마다 수동 확인 필요.
- **로그 기반 보조 확인**: `UBTService_UpdateMonsterState::TickNode`에서 상태 변경 시점에만 로그 남겨서 `read-log`로 전이 이력(Passive→Alert→Combat→Enrage) 텍스트로 확인 가능하게 함.
- **시각 확인**: `unreal-cli.exe screenshot --viewport game --path <scratch>.png`로 포효/애니메이션 정합성 확인.
- **데미지/Enrage 테스트**: 반복 공격 대신 `unreal-cli.exe execute --code "..."`로 `ApplyDamage(1000)`를 직접 호출해 즉시 Enrage 트리거 확인 (매번 수동 전투로 테스트하지 않아도 됨).
- **세션 간 리셋**: 매 PIE 반복 사이 `unreal-cli.exe stop`으로 블랙보드/BT 상태 초기화.

---

## 이번 범위에서 제외 (하지 않음)

- 실제 공격 패턴 다양화 / 부위파괴 시스템
- 실제 Eat/Sleep 콘텐츠 (구조만 준비, 애니메이션 자체는 있음)
- 로코모션 폴리시
- 전투 이탈/하향 전이 로직
- 몬스터 전용 히트박스 AnimNotify 시스템 (현재 `UAnim_AttackNotifyState`는 플레이어 전용)
- GAS, EQS 기반 포지셔닝, StateTree (프레임워크는 BT+BB로 이미 확정)
- 재사용 가능한 `UHealthComponent` (몬스터 종류가 늘어나기 전까지는 불필요)

### 핵심 파일
- `Source/My3DAction/My3DAction.Build.cs`
- `Source/My3DAction/Monster_Usurper.h` / `.cpp`
- `Source/My3DAction/MainCharacter.cpp`
- `Content/My3DAction/Monster/Animation/AMP_Monster.uasset`
- `Content/My3DAction/Monster/BP_Monster.uasset`
- `Content/My3DAction/Monster/Animation/BT_Monster_Usurper.uasset` (기존 고아 에셋 — 먼저 정리)
