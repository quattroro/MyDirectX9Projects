UAISenseConfig_Sight는 언리얼 엔진의 AI 인지 시스템(UAIPerceptionComponent)에서 AI의 시야(Sight)감각을 설정하는 C++ 클래스이다.

``` 
//사용 예시

// AIController 생성자 등에서 설정
AIPerceptionComponent = CreateDefaultSubobject<UAIPerceptionComponent>(TEXT("AIPerception"));
SightConfig = CreateDefaultSubobject<UAISenseConfig_Sight>(TEXT("SightConfig"));

SightConfig->SightRadius = 1500.f;         // 인지 반경 1500
SightConfig->LoseSightRadius = 2000.f;     // 시야 상실 반경 2000
SightConfig->PeripheralVisionAngleDegrees = 90.f; // 시야각 90도 (좌우 45도)

// 컴포넌트에 시각 감각 등록
AIPerceptionComponent->ConfigureSense(*SightConfig);
AIPerceptionComponent->SetDominantSense(SightConfig->GetSenseImplementation());


```