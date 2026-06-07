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

void UFormationBattleComponent::Activate(bool bReset)
{
	Super::Activate(bReset);
	// 전투 진입 시 슬롯 재배정 예약. 다음 Tick에서 헝가리안 1회.
	// 戦闘進入時に再割当を予約。
	bNeedsReassignment = true;
}

void UFormationBattleComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// [1] anchor 획득. 타겟 소멸 시 파이프라인 정지.
	const TOptional<FTransform> AnchorOpt = GetFormationAnchor();
	if (!AnchorOpt.IsSet()) return;
	if (!SlotGenerator) return;

	const FTransform& Anchor = AnchorOpt.GetValue();
	const FVector AnchorOrigin = Anchor.GetLocation();

	TArray<APartyCharacter*> Followers = GetPartyFollowers();
	if (Followers.Num() == 0) return;

	// [2] 슬롯 로컬 생성 → 월드 변환 + 환경보정. (매 틱 — 타겟이 움직이므로)
	TArray<FVector> LocalOffsets;
	SlotGenerator->GenerateSlots(Followers.Num(), ComputeBaseRadius(), LocalOffsets);

	TArray<FVector> WorldSlots;
	WorldSlots.Reserve(LocalOffsets.Num());
	for (const FVector& Local : LocalOffsets)
	{
		const FVector World = Anchor.TransformPosition(Local);
		WorldSlots.Add(AdjustLocationForEnvironment(World, AnchorOrigin));
	}

	// [3] 배정: 진입 시 1회만 헝가리안. 이후엔 저장된 배정 유지.
	// 진입 직후엔 동료 현재 위치 기준 최적 배정 → 교차 이동 최소화.
	// 進入時のみ割当。以降は保持。
	if (bNeedsReassignment || SlotAssignment.Num() != Followers.Num())
	{
		const TArray<APartyCharacter*> Assigned = SolveSlotAssignment(Followers, WorldSlots);
		SlotAssignment.Empty(Assigned.Num());
		for (APartyCharacter* C : Assigned)
		{
			SlotAssignment.Add(C);
		}
		bNeedsReassignment = false;
	}

	// [4] 저장된 배정대로 push. SlotAssignment[i] = 슬롯 i에 갈 동료.
	for (int32 i = 0; i < WorldSlots.Num() && i < SlotAssignment.Num(); ++i)
	{
		if (SlotAssignment[i])
		{
			SlotAssignment[i]->UpdateTargetSlotLocation(WorldSlots[i], false);
		}

		DrawDebugSphere(GetWorld(), WorldSlots[i], 30.f, 12, FColor::Cyan, false, -1.f, 0, 2.f);
	}

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