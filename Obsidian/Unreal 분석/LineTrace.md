
# LineTraceSingleByChannel(), LineTraceMultiByChannel()

해당 함수들은 직선상에 콜리전 트레이스를 수행하여 트레이스에 히트한 CollisionChannel에 해당하는 첫 번째 오브젝트를 반환합니다. 
Single과 Multi의 차이는 Single은 충돌이 감지된  속해있는 오브젝트의 콜리전 반응이 Block으로 되어있을때에만 진행을 멈추고 리턴하지만 Multi는 Block으로 되어있는 오브젝트를 만나면 진행을 멈추고 리턴하는건 같지만 진행경로에 존재하는 Overlap 오브젝트들을 모두 포함시켜서 리턴한다.
트레이스 채널 오버랩 또는 블록과 관계 없이 모든 항목을 받고자 하는 경우, MultiLineTraceByObject 노드를 사용해야 한다.

``` c++
{
	// 결과를 돌려줄 FHitResult 객체
	FHitResult Hit;
	// 시작점
	FVector start = GetActorLocation();
	// 끝점
	FVector end = TargetActor->GetActorLocation();
	
	// 충돌을 감지할 채널을 설정해준다.
	ECollisionChannel Channel = ECollisionChannel::ECC_Visibility;

	// 충돌 인자들을 설정해준다.
	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(this);
	QueryParams.AddIgnoredActor(TargetActor);

	// LineTrace 실행
	GetWorld()->LineTraceSingleByChannel(Hit, start, end, Channel, QueryParams);
}
```   



# SweepSingleByChannel()
lineTrace와 달리 광선을 이용하는게 아닌 구, 박스, 캡슐 중에서 선택된 Shape을 두 지점 사이에 직선으로 물체를 던지는 동작을 시뮬레이션 한다.
예를 들어 플레이어가 물체를 던질때 해당 물체가 떨어질 예상 지점을 시각적으로 보여줄 때 이용된다.

[[CustomChannel]]