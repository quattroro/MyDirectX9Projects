c++에서 UFUNCTION 매크로를 사용할 때 매크로에 BlueprintNativeEvent 태그를 붙이면, 함수를 블루프린트 네이티브 이벤트로 만들 수 있다.

블루프린트 네이티브 이벤트는 c++에 선언돼 기본 동작을 가질 수 있고 c++에서 구현할 수도 있지만 블루프린트에서 오버라이딩할 수 있는 이벤트를 말한다. MyEvent라는 이름의 함수를 선언하고 UFUNCTION 매크로에 BlueprintNativeEvent 태그를 추가한 다음 Virtual MyEvent_Implementation 함수를 이어서 선언해주면 블루프린트 네이티브 이벤트를 선언할 수 있다.

``` c++
UFUNCTION(BlueprintNativeEvent)
void MyEvent();
virtual void MyEvent_Inplementation();
```

이 두 함수를 선언해야 하는 이유는 다음과 같다. 첫 번째 함수는 블루프린트에서 이벤트를 오버라이딩 할 수 있는 블루프린트 서명이고 두 번째 함수는 c++에서 이벤트를 오버라이딩할 수 있는 c++ 서명이기 때문이다.
c++ 서명은 단순히 이벤트 이름의 끝에 \_Implementation이 붙고, 항상 virtual 함수여야한다.
c++에서 이 이벤트를 선언한다는 것을 감안했을 때 기본 동작을 구현하기 위해서는 MyEvent 함수가 아니라 MyEvent_Implementation 함수를 구현해야 한다. 블루프린트 네이티브 이벤트를 호출하려면 \_Implenentation 접미사가 없는 일반 함수를 호출하면 된다. 
