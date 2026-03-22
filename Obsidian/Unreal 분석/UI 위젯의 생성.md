#### CreateWidget<>

``` C++
template <typename WidgetT = UUserWidget, typename OwnerType = UObject>
WidgetT* CreateWidget(OwnerType OwningObject, TSubclassOf<UUserWidget> UserWidgetClass = WidgetT::StaticClass(), FName WidgetName = NAME_None)
```

두번째 인자로 건네받은 UserWidgetClass 객체를 실제 인스턴스로 생성해서 리턴해준다.
첫번째 인자인 OwningObject는 어떤 UWorld에서 위젯을 생성해야 하는지 를 찾고, 그 World에 소속된 위젯으로 만들어 주기 위해서 사용된다.