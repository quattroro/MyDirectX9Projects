``` C++
void AMonster_Usurper::Die()
{
    // 재생중인 몽타주가 슬롯에서 상태머신을 덮어쓰지 않도록 정지
    if (UAnimInstance* AnimInst = GetMesh()->GetAnimInstance())
    {
        AnimInst->StopAllMontages(0.2f);
    }

    // AI 정지
    if (AAIController* AI = Cast<AAIController>(GetController()))
    {
        AI->StopMovement();
        if (AI->BrainComponent)
        {
            // "Dead"라는 사유와 함께 AI 로직을 중지
            AI->BrainComponent->StopLogic(TEXT("Dead"));
        }
    }

    GetCharacterMovement()->DisableMovement();

    // 플레이어가 시체에 막히지 않게 Pawn 채널만 무시
    GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
}
```

이런식으로 몽타주 재생과 AI, 움직임을 정지시킬 수 있다.