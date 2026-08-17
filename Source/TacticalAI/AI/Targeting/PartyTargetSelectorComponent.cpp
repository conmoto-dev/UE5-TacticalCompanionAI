#include "AI/Targeting/PartyTargetSelectorComponent.h"
#include "Characters/PartyCharacter.h"
#include "Party/PartyManager.h"

UPartyTargetSelectorComponent::UPartyTargetSelectorComponent()
{
	// 味方のターゲットライン：シアン、低め。
	DebugTargetLineColor = FColor::Cyan;
	DebugTargetLineZOffset = 10.f;
}

FTargetingContext UPartyTargetSelectorComponent::BuildContext() const
{
	FTargetingContext Context;
	Context.SelfLocation = GetOwner()->GetActorLocation();

	const APartyCharacter* OwnerCharacter = Cast<APartyCharacter>(GetOwner());
	const APartyManager* Manager = OwnerCharacter ? OwnerCharacter->GetPartyManager() : nullptr;
	if (!Manager) return Context;

	const APartyCharacter* Leader = Manager->GetLeader();
	if (Leader)
	{
		Context.LeaderLocation = Leader->GetActorLocation();
		Context.bHasLeader = true;

		// [1] 리더 타겟 공급 — 단, 자신이 리더면 비워둔다 (자기 참조 루프 차단).
		// リーダーターゲット供給 — 自分がリーダーなら空のまま（自己参照遮断）。
		if (Leader != OwnerCharacter)
		{
			if (const UTargetSelectorComponent* LeaderSelector = Leader->GetTargetSelector())
			{
				Context.LeaderTarget = LeaderSelector->GetCurrentTarget();
			}
		}
	}

	// [2] 동료 타겟 수집: 자신 제외 전 파티원 (리더 포함). 평가 시차 덕에
	//     항상 남들의 "커밋된" 타겟을 읽는다 — 동시 평가 경합 없음.
	// 味方ターゲット収集：自分以外の全メンバー。常に他者の「コミット済み」を読む。
	TArray<APartyCharacter*> AllMembers = Manager->GetFollowers();
	if (Leader)
	{
		AllMembers.AddUnique(const_cast<APartyCharacter*>(Leader));
	}
	for (const APartyCharacter* Member : AllMembers)
	{
		if (!Member || Member == OwnerCharacter) continue;

		if (const UTargetSelectorComponent* Selector = Member->GetTargetSelector())
		{
			if (const AActor* AllyTarget = Selector->GetCurrentTarget())
			{
				Context.AllyTargets.Add(AllyTarget);
			}
		}
	}
	return Context;
}

TArray<AActor*> UPartyTargetSelectorComponent::GatherCandidates() const
{
	const APartyCharacter* OwnerCharacter = Cast<APartyCharacter>(GetOwner());
	const APartyManager* Manager = OwnerCharacter ? OwnerCharacter->GetPartyManager() : nullptr;
	return Manager ? Manager->GetPerceivedEnemies() : TArray<AActor*>();
}