
얼리얼에서 기본으로 정의되어있는 ECollisionChannel 열거형
``` c++
enum ECollisionChannel : int
{

	ECC_WorldStatic UMETA(DisplayName="WorldStatic"),
	ECC_WorldDynamic UMETA(DisplayName="WorldDynamic"),
	ECC_Pawn UMETA(DisplayName="Pawn"),
	ECC_Visibility UMETA(DisplayName="Visibility" , TraceQuery="1"),
	ECC_Camera UMETA(DisplayName="Camera" , TraceQuery="1"),
	ECC_PhysicsBody UMETA(DisplayName="PhysicsBody"),
	ECC_Vehicle UMETA(DisplayName="Vehicle"),
	ECC_Destructible UMETA(DisplayName="Destructible"),

	/** Reserved for gizmo collision */
	ECC_EngineTraceChannel1 UMETA(Hidden),

	ECC_EngineTraceChannel2 UMETA(Hidden),
	ECC_EngineTraceChannel3 UMETA(Hidden),
	ECC_EngineTraceChannel4 UMETA(Hidden), 
	ECC_EngineTraceChannel5 UMETA(Hidden),
	ECC_EngineTraceChannel6 UMETA(Hidden),

	ECC_GameTraceChannel1 UMETA(Hidden),
	ECC_GameTraceChannel2 UMETA(Hidden),
	ECC_GameTraceChannel3 UMETA(Hidden),
	ECC_GameTraceChannel4 UMETA(Hidden),
	ECC_GameTraceChannel5 UMETA(Hidden),
	ECC_GameTraceChannel6 UMETA(Hidden),
	ECC_GameTraceChannel7 UMETA(Hidden),
	ECC_GameTraceChannel8 UMETA(Hidden),
	ECC_GameTraceChannel9 UMETA(Hidden),
	ECC_GameTraceChannel10 UMETA(Hidden),
	ECC_GameTraceChannel11 UMETA(Hidden),
	ECC_GameTraceChannel12 UMETA(Hidden),
	ECC_GameTraceChannel13 UMETA(Hidden),
	ECC_GameTraceChannel14 UMETA(Hidden),
	ECC_GameTraceChannel15 UMETA(Hidden),
	ECC_GameTraceChannel16 UMETA(Hidden),
	ECC_GameTraceChannel17 UMETA(Hidden),
	ECC_GameTraceChannel18 UMETA(Hidden),
	
	/** Add new serializeable channels above here (i.e. entries that exist in FCollisionResponseContainer) */
	/** Add only nonserialized/transient flags below */

	// NOTE!!!! THESE ARE BEING DEPRECATED BUT STILL THERE FOR BLUEPRINT. PLEASE DO NOT USE THEM IN CODE

	ECC_OverlapAll_Deprecated UMETA(Hidden),
	ECC_MAX,
};

```   

1. 에디터 왼쪽 상단 메뉴에서 ***편집*** 을 클릭하고 *프로젝트 세팅* 을 선택한 다음. *콜리전* 섹션으로 이동한다. 여기서 *트레이스 채널* 을 확인할 수 있다.

2. *새 트레이스 채널* 옵션을 선택한다. 그러면 창이 뜨고, 새로 추가할 채널의 이름을 지정하는 옵션과 프로젝트에 있는 다른 물체들의 기본 반응을 설정할 수 있는 옵션이 제공된다.

