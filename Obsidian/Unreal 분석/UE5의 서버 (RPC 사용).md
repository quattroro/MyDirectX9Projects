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
RPC