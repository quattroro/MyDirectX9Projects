
* Inner Radius : 이 플로트 속성은 소리의 볼륨을 줄이기 시작할 거리를 지정할 수 있는 기능을 제공한다. 사운드가 이 값보다 작은 거리에서 재생되면 볼륨은 영향을 받지 않는다.

* Falloff Distance : 이 플로트 속성을 사용하면 소리가 들리지 않게 할 거리를 지정 할 수 있다. 사운드가 이 값보다 먼 거리에서 재생되면 들리지 않는다. 사운드의 볼륨은 듣는 플레이어와의 거리와 Inner Radius 또는 Falloff Distance에 더 가까운지 여부에 따라 달라진다.

사용할때는 UGameplayStatics::PlaySoundAtLocation()함수의 매개변수로 Sound Attenuation 객체를 넘겨주면 된다.