UActorComponent를 상속받는 컴포넌트
트랜스폼과 Attach를 지원하지만 렌더링과 충돌은 지원하지 않는다
액터에 콜리전 컴포넌트가 필요하지 않다면. 일반적으로 USceneComponent를 오브젝트의 RootComponent로 추가하는 것이 좋다. 이렇게 하면 자식 컴포넌트에 더 많은 유연성을 제공할 수 있다.
액터의 RootComponent는 위치나 회전을 수정할 수 없다. 따라서 실습에서 USceneComponet를 RootComponent로 설정하는 대신, 스태틱 메시 컴포넌트를 RootComponent로 추가했다면 위치와 회전을 수정할 수 없기 때문에 어려움을 격을 수 있다.