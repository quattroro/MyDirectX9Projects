UObject를 상속받음

AActor의 Initialization순서 

1. UObject::PostLoad
   - ULevel에 AActor를 배치하면 ULevel의 패키지에 AActor가 저장된다.
   - 게임 빌드에서 ULevel이 스트리밍(또는 로드)중일 때 배치된 AActor가 로드되고 UObject::PostLoad를 호출한다.
   - 참고로 UObject::PostLoad는 [오브젝트가 생성되고] -> [에셋이 로드되고] -> [호출 이후 UObject::PostLoad() 이벤트가 호출된 후]에 호출된다.

2. UActorComponent::OnComponentCreated
   - AActor는 이미 스폰된 상태이다.
   - UActorComponent별 알림
   - 기억해야할 한가지는 'component-creation'이 먼저라는 것이다.
   - UActor는 컴포넌트다.

3. AActor::PreRegisterAllComponents
   - UActorComponent를 AActor에 등록하기 위한 사전 이벤트
   - [UActorComponent Creation] -> [UActorComponentRegistration]


4. AActor::RegisterComponent
   - incremental-registration: 여러 프레임에 걸쳐 UActorComponent를 AActor에 등록한다.
   - UActorComponent를 월드에 등록(GameWorld[UWorld], RenderWorld[FScene], PhysicsWorld[FPhysScene], ...) 각 월드들에 현재 액터를 똑같이 생성해준다.
   - 각 world마다 initialize state가 필요

5. AActor::PostRegisterAllcomponents
   - UActor컴포넌트를 AActor에 등록하기 위한 사후 이벤트

6. AActor::UserConstructionScript
   - Editor에서 UserConstructionScript 과 SimpleConstructionScript in the editor에 대해 설명한다.
   - UserConstructionScript: BP이벤트 그래프에서 UActorComponent를 생성하기 위한 함수 호출 (블루프린트창의 EventGraph 창에서 노드를 이용해서 생성하는 Component를 이야기함)
   - SimpleConstructScript: BP뷰포트에서 UActorComponent를 구성한다.(계층적) (블루프린트 창에서 왼쪽에 존재하는 하이어라키 창을 이야기함)

7. AActor::PreInitializeComponents
   - UActor의 UActorComponents를 초기화하기 위한 사전 이벤트
   - [UActorComponent 생성]->[UActorComponent 등록]->[UActorComponent 초기화]

8. UActorComponent::Activate
   - UActorComponent를 초기화하기 전에 먼저 UActorComponent의 활성화를 호출한다.

9. UActorComponent::InitializeComponent
   - UActorComponent의 초기화는 두 단계로 이루어져 있다.
   - Activate -> InitializeComponent

10. AActor::PostInitiallizeComponents
   - UActorComponent들의 초기화가 완료된 후 이벤트

11. AActor::Beginplay
   - 레벨이 플레이되면 AActor가 Beginplay를 호출한다.



werw
