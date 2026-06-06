#include "AI/Components/FormationBattleComponent.h"
#include "AI/Strategies/SlotGeneratorStrategy.h"
#include "Enemies/TargetDummy.h"   // TODO: 인터페이스로 분리 (구현체 2개째 생기면)
#include "DrawDebugHelpers.h"
#include "NavigationSystem.h"
#include "Characters/PartyCharacter.h"

UFormationBattleComponent::UFormationBattleComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}
void UFormationBattleComponent::BeginPlay()
{
	Super::BeginPlay();
	CurrentTarget = DebugTargetActor;
}

void UFormationBattleComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// [a] anchor 획득. 타겟 소멸 시 미반환 → 파이프라인 정지 (크래시 방지).
	const TOptional<FTransform> AnchorOpt = GetFormationAnchor();
	if (!AnchorOpt.IsSet()) return;
	if (!SlotGenerator) return;

	const FTransform& Anchor = AnchorOpt.GetValue();
	const FVector AnchorOrigin = Anchor.GetLocation();
	
	// [b] 로컬 슬롯 생성 (Strategy — 월드·타겟 무지).
	TArray<FVector> LocalOffsets;
	SlotGenerator->GenerateSlots(NumSlots, ComputeBaseRadius(), LocalOffsets);

	// [c] 로컬→월드 + 환경보정 + push.
	for (int32 i = 0; i < LocalOffsets.Num(); ++i)
	{
		const FVector World = Anchor.TransformPosition(LocalOffsets[i]);
		const FVector Adjusted = AdjustLocationForEnvironment(World, AnchorOrigin);

		// push: 인덱스순 단순 배정 (Hungarian은 스텝 9). 동료 부족하면 스킵.
		if (DebugCompanions.IsValidIndex(i) && DebugCompanions[i])
		{
			DebugCompanions[i]->UpdateTargetSlotLocation(Adjusted, false);
		}

		// [임시 드로우] 스텝 후반에 제거.
		DrawDebugSphere(GetWorld(), Adjusted, 30.f, 12, FColor::Cyan, false, -1.f, 0, 2.f);
	}

	// [임시] anchor forward 시각화.
	DrawDebugDirectionalArrow(GetWorld(), AnchorOrigin,
		AnchorOrigin + Anchor.GetRotation().GetForwardVector() * 150.f,
		60.f, FColor::Red, false, -1.f, 0, 3.f);
}



TOptional<FTransform> UFormationBattleComponent::GetFormationAnchor() const
{
	// 약참조 유효성 = 타겟 살아있음. 죽었으면 빈 Optional → 호출부가 파이프라인 정지.
	if (const AActor* Target = CurrentTarget.Get())
	{
		return Target->GetActorTransform();
	}
	return TOptional<FTransform>();
}

float UFormationBattleComponent::ComputeBaseRadius() const
{
	float TargetRadius = 0.f;

	// TODO: ATargetDummy 직접 캐스트 = 결합. 진짜 적 추가 시 IEncircleTarget 인터페이스로 분리.
	//       지금은 구현체 하나라 조기 추상화 회피 (캐스트 유지).
	if (const ATargetDummy* Dummy = Cast<ATargetDummy>(CurrentTarget.Get()))
	{
		TargetRadius = Dummy->GetEncircleRadius();
	}

	return DesignerBaseRadius + TargetRadius;
}