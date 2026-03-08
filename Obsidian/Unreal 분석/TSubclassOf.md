**TSubclassOf** 는 UClass 유형의 안전성을 보장해 주는 템플릿 클래스입니다. 예를 들어 디자이너가 대미지 유형을 지정하도록 해주는 발사체 클래스를 제작중이라 가정합시다. 그냥 UPROPERTY 유형의 UClass 를 만든 다음 디자이너가 항상 UDamageType 파생 클래스만 할당하기를 바라거나, TSubclassOf 템플릿을 사용하여 선택지를 제한시킬 수도 있습니다. 그 차이점은 아래 코드와 같습니다

``` c++
	/** type of damage */
	UPROPERTY(EditDefaultsOnly, Category=Damage)
	UClass* DamageType;
```

``` c++
		/** type of damage */
	UPROPERTY(EditDefaultsOnly, Category=Damage)
	TSubclassOf<UDamageType> DamageType;
```

두 번째 선언에서, 템플릿 클래스는 에디터의 프로퍼티 창에 UDamageType 파생 클래스만 선택되도록 합니다. 첫 번째 선언에서는 아무 UClass 나 선택할 수 있습니다.

