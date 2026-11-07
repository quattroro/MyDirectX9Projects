1. .Build.cs에 UMG 모듈 추가
   UserWidget, ProgressBar, WidgetComponent는 UMG 모듈에 있어서 이게 없으면 링크 에러가 난다.

2. UserWidget을 상속받는 c++ 클래스 생성
   ![[Pasted image 20261031145602.png]]



3. 해당 c++ 클래스를 상속받는 WidgetBlueprint 생성 & UI 편집



4. C++ 클래스에 사용할 UI 위젯을 UPROPERTY(meta = (BindWidget))로 바인딩하여 사용한다. BindWidget은 위젯의 이름이 블루프린트에서의 이름과 같아야된다.


5. 이후엔 해당 UI를 호출하고 제어할 스립트에서 UWidgetComponent* 객체를 FindComponentByClass<UWidgetComponent>등등 다양한 방법으로 받아와서 사용하면 된다.
