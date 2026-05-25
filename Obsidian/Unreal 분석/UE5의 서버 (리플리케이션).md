서버가 클라이언트를 동기화하는 방법 중 하나는 변수 리플리케이션을 사용하는 것이다. 리플리케이션은 서버의 변수 리플리케이션 시스템이 초당 특정 횟수마다 클라이언트에 최신 값으로 업데이트가 필요한 변수가 있는지 확인하는 방식으로 동작한다.
변수는 다음의 경우에만 업데이트되도록 클라이언트로 전송된다.

* 변수가 복제되도록 설정된 경우
* 서버에서 값이 변경된 경우
* 클라이언트의 값이 서버와 다른 경우
* 액터의 리플리케이션이 활성화된 경우
* 액터가 관련이 있고 모든 리플리케이션 조건을 만족하는 경우

여기서 고려해야 할 중요한 사항은 변수를 복제해야 하는지 여부를 결정하는 로직이 1초에 AActor::NetUpdateFrequency번만 실행된하는 것이다. 즉 서버는 서버의 변수 값을 변경한 후 곧바로 클라이언트에 업데이트 요청을 보내지 않는다.

언리얼 엔진에서 UPROPERTY 매크로를 사용할 수 있는 모든 변수는 복제하도록 설정할 수 있고, 이렇게 설정할 때는 2개의 지정자를 사용할 수 있다.

# Replicated

변수를 단순히 리플리케이션만 하고 싶은 경우에는 Replicated 지정자를 사용한다.

```
UPROPERTY(Replicated)
float Health = 100.0f
```


# ReplicatedUsing

어떤 변수의 리플리케이션을 원하면서 변수가 업데이트될 때마다 어떤 함수를 호출하고 싶다면 ReplicatedUsing=<함수 이름> 지정자를 사용한다.

```
UPROPERTY(ReplicatedUsing=OnRep_Health)
float Health = 100.0f

UFUNCTION()
void OnRep_Health()
{
	UpdateHUD();
}
```


# GetLifetimeReplicatedProps

변수를 Replicated로 표시하는 것 외에도 액터의 cpp 파일에서 GetLifetimeReplicatedProps 함수를 구현해야 한다. 여기서 주의해야 할 점은 Replicated로 선언된 변수가 1개 이상 있으면 이 함수가 내부적으로 선언되기 때문에 헤더 파일에 선언하면 안 된다는 점이다. 이 함수의 목적은 Replicated 선언된 각 변수를 어떻게 복제해야 하는지를 알려주는 데 있다. 복제하려는 모든 변수에 DOREPLIFETIME 매크로를 사용해 이를 수행할 수 있다.

# DOREPLIFETIME

이 매크로는 Replicated로 선언된 변수가 리플리케이션 조건 없이 모든 클라이언트에 복제된다는 것을 리플리케이션 시스템에 알려준다.

구문은 다음과 같다.
```
DOREPLIFETIME(<클래스 이름>, <Replicated로 선언된 변수 이름>)
```

```
void AvariableReplicationActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AvariableReplicationActor, Health);
}
```
위의 코드에서는 리플리케이션 시스템에 AVariableRoplicationActor 클래스의 Health변수를 추가 조건 없이 복제하기 위해 DOREPLIFETIME 매크로를 사용한다.

# DOREPLIFETIME_CONDITION

이 매크로는 Replicated로 선언된 변수가 조건을 만족했을 때 클라이언트에서만 복제된 것이라고 리플리케이션 시스템에 알려준다.
구문은 다음과 같다.

```
DOREPLIFETIME_CONDITION(<클래스 이름>, <Replicated 변수 이름>, <조건>);
```

조건 파라미터는 다음의 값들 중 하나를 설정할 수 있다.

* COND_InitialOnly : 이 변수는 초기 리플리케이션에서 한 번만 복제된다.
* COND_OwnerOnly : 이 변수는 액터의 소유자에게만 복제호니다.
* COND_SkipOwner : 이 변수는 액터의 소유자에게 복제되지 않는다.
* COND_SimulatedOnly : 변수가 시뮬레이션 중인 액터에만 복제된다.
* COND_AutonomousOnly : 변수가 Autonomous 상태인 액터에만 복제된다.
* COND_SimulatedOrPhysics : 변수가 시뮬레이션 중인 액터나 bRepPhysics 값이 true로 설정된 액터에만 복제된다.
* COND_InitialOrOwner : 변수가 초기 리플리케이션이나 액터 소유자에게 한번만 복제된다.
* COND_Custom : 변수가 SetCustomIsActiveOverride 불리언 조건이 true인 경우에만 복제된다.

```
void AVariableReplicationActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OurLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(AvariableReplicationActor, Health, COND_OwnerOnly);
}
```

위의 코드에서는 DOREPLIFETIME_CONDITION 매크로를 사용해 리플리케이션 시스템에 AVariableReplicationActor 클래스의 Health 변수가 이 액터의 소유자에게만 복제된다고 알려준다.




