변수 리플리케이션은 매우 유용한 기능이지만, 원격 게임 인스턴스에 있는 커스텀 코드의 실행을 허용한다는 측면에서 제한적이다. 다음 두 가지 이유를 들 수 있다.

* 첫 번째 이유는 변수 리플리케이션은 서버에서 클라이언트로 통신하는 한 형태이기 때문이다. 따라서 클라이언트가 변수 리플리케이션을 사용해 변수 값을 변경하고 서버에 커스텀 로직을 실행하도록 지시할 수 있는 방법이 없다.
* 두 번째 이유는 그 이름에서 알 수 있듯이 변수 리플리케이션은 변수의 값을 통해 이뤄지기 때문이다. 따라서 변수 리플리케이션이 클라이언트에서 서버로의 통신을 허용하더라도 클라이언트의 값을 변경하고 서버에서 RepNotify 기능을 트리거해 커스텀 로직을 실행해야 하는데 이 방법은 실용적이지 않다.

이 문제를 해결하기 위해 언리얼 엔진을 RPC를 지원한다. RPC는 정의하고 호출할 수 있는 일반 함수처럼 동작한다. 하지만 로컬에서 실행되는 대신, 원격 게임 인스턴스에서 실행된다. 변수와 직접적인 연관이 없는 특정 로직을 원격 게임 인스턴스에서 실행하는 것이다. RPC를 사용 가능하려면, 유효한 연결을 갖고 있으며 리플리케이션을 활성화한 액터에서 RPC를 정의해야 한다.

RPC에는 세 가지 유형이 있으며, 각각 다른 용도로 사용한다.

* 서버 RPC
* 멀티캐스트 RPC
* 클라이언트 RPC

# 서버 RPC

RPC를 정의한 액터의 함수를 서버에서 실행해야 할 때마다. 서버 RPC를 사용한다. 서버 RPC가 필요한 주요 이유는 두 가지다.

* 첫 번째는 보안 때문이다. 멀티 플레이어 게임을 만들 때, 특히 경쟁적인 게임을 만들 때는 항상 클라이언트가 속임수를 쓰려 한다고 가정해야 한다. 속임수를 없애는 방법은 클라이언트가 게임 플레이에 중요한 함수를 서버에서 실행하도록 만드는 것이다.

* 두 번째 이유는 동기화다. 중요한 게임 플레이 로직은 서버에서만 실행된다. 즉, 중요한 변수가 서버에서만 변경되므로 변수가 업데이트될 때마다 클라이언트에 변수를 복제하는 로직을 실행해야 한다.

### 서버 RPC 선언

서버 RPC를 선언할 때는 UFUNCTION 매크로에서 Server 지정자를 사용한다. 

```
UFUNCTION(Server, Reliable, WithValidation)
void ServerRPCFunction(int32 IntegerParameter, float FloatParameter, AActor* ActorParameter);
```

### 서버 RPC 실행

서버 RPC를 실행하려면 서버 RPC를 정의한 액터 인스턴스를 가진 클라이언트에서 호출해야 한다.

```
void ARPCTest::CallMyOwnServerRPC(int32 IntegerParameter)
{
	ServerMyOwnRPC(IntegerParameter);
}
```


# 멀티캐스트 RPC

서버가 RPC를 정의한 액터의 어떤 함수를 모든 클라이언트에서 호출하도록 지시할 때 멀티캐스트 RPC를 사용한다.

클라이언트 캐릭터가 무기를 발사할 때를 예로 들 수 있다. 클라이언트가 서버 RPC의 호출을 통해 무기 발사를 위한 권한을 요청하면, 서버가 이 요청을 처리한다. 무기 발사를 요청한 해당 캐릭터의 인스턴스에서 발사 애니메이션을 재생할 수 있도록 멀티캐스트 RPC를 수행해야 한다.

### 멀티캐스트 RPC 선언

멀티캐스트 RPC를 선언할 때는 UFUNCTION 매크로에서 NetMulticast 지정자를 사용한다. 다음의 예제를 살펴보자.

```
UFUNCTION(NetMulticast)
void MulticastRPCFunction(int32 IntegerParameter, float FloatParameter, AActor* ActorParameter);
```

위의 코드를 살펴보면, 이 함수를 멀티캐스트 RPC로 선언하기 위해 FUNCTION 매크로에 NetMulticast 지정자를 사용했다. 일반 함수와 마찬가지로 멀티캐스트 RPC에서 파라미터를 사용할 수는 있지만, 서버 RPC와 마찬가지로 동일한 주의 사항이 있다.


### 멀티캐스트 RPC의 실행

멀티캐스트 RPC를 실행하려면 멀티캐스트 RPC를 정의한 액터 인스턴스의 서버에서 호출해야 한다.

```
void ARPCTest::CallMyOwnMulticastRPC(int32 IntegerParameter)
{
	MulticastMyOwnRPC(IntegerParameter);
}
```


# 클라이언트 RPC

RPC를 정의한 액터를 소유한 클라이언트에서만 함수를 실행하려는 경우에는 클라이언트 RPC를 사용한다. 소유 클라이언트를 설정하러면 서버에서 SetOwner을 호출하고 클라이언트의 플레이어 컨트롤러로 설정해야 한다.

이 예로 캐릭터가 발사체에 맞았을 때 해당 클라이언트에서만 들리는 피격 사운드를 재생하는 경우를 들 수 있다. 서버에서 클라이언트 RPC를 호출하면 사운드가 소유 클라이언트에서만 재생되므로 다른 클라이언트에서는 들을 수 없다.

### 클라이언트 RPC 선언

클라이언트 RPC를 선언할 때는 UFUNCTION 매크로에서 Client 지정자를 사용한다. 다음의 예제를 살펴보자.

```
UFUNCTION(Client)
void ClientRPCFunction(int32 IntegerParameter, float FloatParameter, AActor* ActorParameter);
```

### 실행
클라이언트 RPC를 실행하려면 클라이언트 RPC를 정의한 인스턴스의 서버에서 호출해야 한다.

```
void ARPCTest::CallMyClientRPC(int32 IntegerParameter)
{
	ClientMyOwnRPC(IntegerParameter);
}
```

# RPC를 사용할 때 주의할 사항

### 구현
RPC의 구현은 일반 함수와 조금 다르다. 평소와 같이 함수를 구현하는 대신, 헤더 파일에 선언하지 않았더라도 RPC 함수의 \_Implementation 버전만 구현해야 한다. 

### 서버 RPC

```
void ARPCTest::ServerRPCTest_Implementation(int32 IntegerParameter, flaot FloatParameter, AActor* ActorPrarmeter)
{
}
```

### 멀티캐스트 RPC

```
void ARPCTest::MulticastRPCTest_Implementation(int32 IntegerParameter, float FloatParameter, AActor* ActorParameter)
{
}
```

### 클라이언트 RPC

```
void ARPCTest::ClientRPCTest_Implementation(int32 IntegerParameter, float FloatParameter, AActor* ActorParameter)
{
}
```

위의 코드에서는 3개 파라미터를 사용하는 ClientRPCTest 함수의 \_Implementation 버전을 구현한다.
구현하는 RPC 유형에 관계없이 일반 함수에 다르게 \_Implementation 버전의 함수만 구현해야 한다.

### 이름 접두어

언리얼 엔진에서는 RPC에 해당 타입을 접두어로 지정하는 것이 좋다.

* RPCFunction을 호출하는 서버 RPC의 이름은 ServerRPCFunction으로 지정하는 것이 좋다.
* RPCFunction을 호출하는 멀티캐스트 RPC의 이름은 MulticastRPCFunction으로 지정하는 것이 좋다.
* RPCFunction을 호출하는 클라이언트의 RPC의 이름은 ClientRPCFunction으로 지정하는 것이 좋다.

### 반환 값
일반적으로 RPC의 실행은 다른 장치에서 비동기로 실행되기 때문에 반환 값을 가질 수 없다. 따라서 반환 값은 항상 void여야 한다.

### 오버라이딩
UFUNCTION 매크로 없이 자식 클래스에서 \_Implementation 함수를 선언하고 구현하면 RPC의 구현을 재정의해 부모의 기능을 확장하거나 추가할 수 있다.

다음과 같은 부모 클래스의 선언이 있으면 
```
UFUNCTION(Server)
void ServerRPCTest(int32 IntegerParameter);
```

자식 클래스에선 다음과 같이 재정의가 가능하다.
```
virtual void ServerRPCTest_Implementation(int32 IntegerParameter) override;
```

함수의 구현은 다름 재정의와 동일하며 상위 클래스의 기능을 계속 실행하려는 경우에는 Super::ServerRPCTest_implementation을 호출할 수 있다.

### 유효한 연결

액터가 RPC를 호출하기 위해서는 유효한 연결을 가져야 한다. 유효한 연결을 갖지 않은 액터에서 RPC의 호출을 시도하면 원격 인스턴스에서 아무 일도 일어나지 않는다. RPC를 호출하려는 액터가 플레이어 컨트롤러인지, 플레이어 컨트롤러가 소유하고 있는지 또는 액터가 유효한 연결을 가졌는지 확인해야 한다.

### 지원하는 파라미터 타입

RPC를 사용할 때는 다른 함수와 마찬가지로 파라미터를 추가할 수 있다. 이 책을 집필 하는 시점에서 대부분의 공용 타입을 지원하지만, 전부를 지원하지는 않는다. TSet과 TMap이 지원되지 않는 대표적인 타입에 속한다. 이들 중에서 더욱 주의를 기울여야 하는 타입은 UObject 클래스 또는 UObject의 하위 클래스 특히 액터에 대한 포인터다.

액터 파라미터를 갖는 RPC를 생성하면, 해당 액터 또한 원격 게임 인스턴스에 존재해야 한다. 그렇지 않으면 nullptr이 된다. 또 중요하게 고려해야 할 사항은 액터의 각 버전의 인스턴스 이름이 다를 수 있다는 접이다. 즉, 액터 파라미터를 갖는 RPC를 호출하면, RPC를 호출할 때 해당 액터의 인스턴스 이름은 원격 인스턴스에서 RPC를 실행할 때의 인스턴스 이름과 다를 수 있다.

### 타깃 머신에서 RPC 실행하기

대상 장치에서 직접 RPC를 호출할 수 있으며, 여전히 실행된다. 다시 말해, 서버에서 서버 RPC를 호출하면 그대로 실행된다.
마찬가지로 클라이언트에서 멀티캐스트/클라이언트 RPC를 호출할 수 있다. 하지만 후자의 경우 RPC를 호출한 클라이언트에서만 로직을 실행한다. 두 경우 모두 로직을 더 빨리 실행하려면 항상 \_Implementation 버전을 직접 호출해야 한다.
그 이유는 \_Implementation 버전이 실행할 로직만 보유하고 일반적인 RPC 호출이 갖는 네트워크를 통해 RPC 요청을 생성하고 전송하는 오버헤드를 갖지 않기 때문이다.

```
void ARPCTest::CallServerRPC(int32 ImtegerParameter)
{
	if(HasAuthority())
	{
		ServerRPCFunction_Implementation(IntegerParameter);
	}
	else
		ServerRPCFunction(ImtegerParameter);
}
```

앞의 예제에는 두 가지 다른 방식으로 ServerRPCFunction 함수를 호출하는 CallServerRPC함수가 있다. 액터가 서버에 이미 있으면, ServerRPCFunction_Implementation을 호출해 앞에서 설명한 오버헤드를 건너뛴다.
액터가 서버에 없으면 ServerRPCFunction을 사용해 일반 RPC호출을 실행한다. 그러면 네트워크를 통해 RPC요청을 생성하고 전송하는 데 필요한 오버헤드가 추가된다.

### 검증
RPC를 정의할 때는 RPC를 호출하기 전에 잘못된 입력이 있는지 확인하기 위한 추가 옵션을 사용할 수 있다. 이는 부정행위나 다른 이유로 입력이 잘못된 경우, RPC를 처리하지 않도록 하기 위해 사용한다.
검증을 사용하려면 UFUNCTION 매크로에 WithValidation 지정자를 추가해야 한다.
WithValidation 지정자를 사용하려면, RPC를 실행할 수 있는지 여부를 나타내는 불리언을 반환하는 함수의 \_Validate 버전을 구현해야만 한다.

```
UFUNCTION(Server, Reliable, WithValidation)
void ServerSetHealth(float NewHealth);
```
앞의 코드에서는 Health의 새로운 값에 대해 플로트 파라미터를 사용하는 ServerSetHealth라는 검증된 서버 RPC를 선언한다. 구현을 다음과 같다.

```
bool ARPCTest::ServerSetHealth_Validate(float NewHealth)
{
	return NetHealth >= 0.0f && <= MaxHealth;
}

void ARPCTest::ServerSetHealth_Implementation(float NewHealth)
{
	Health = NewHealth;
}
```

앞의 코드는 체력의 새로운 값이 0과 체력의 최댓값 사이에 있는지를 확인하는 \_Validate 함수를 구현한다. MaxHealth가 100일 때 클라이언트가 ServerSetHealth를 해킹해 200으로 호출하려고 시도하면, RPC가 호출되지 않기 때문에 클라이언트가 특정 범위를 벗어난 값으로 상태를 변경하는 것을 방지한다. \_Validate 함수가 true를 반환하면 \_Implementation 함수가 평소와 같이 호출되고 Health값을 NewHealth값으로 설정한다.

### 신뢰성
