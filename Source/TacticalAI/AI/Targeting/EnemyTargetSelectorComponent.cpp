#include "AI/Targeting/EnemyTargetSelectorComponent.h"
#include "Characters/EnemyCharacter.h"
#include "Enemies/Encounter/EnemyEncounterBase.h"

UEnemyTargetSelectorComponent::UEnemyTargetSelectorComponent()
{
	// 敵のターゲットライン：赤、高め。
	DebugTargetLineColor = FColor::Red;
	DebugTargetLineZOffset = 20.f;
}

FTargetingContext UEnemyTargetSelectorComponent::BuildContext() const
{
	FTargetingContext Context;
	Context.SelfLocation = GetOwner()->GetActorLocation();

	const AEnemyCharacter* OwnerCharacter = Cast<AEnemyCharacter>(GetOwner());
	const AEnemyEncounterBase* Encounter = OwnerCharacter ? OwnerCharacter->GetEncounter() : nullptr;
	if (!Encounter)
	{
		return Context;
	}

	// 동료 타겟 수집: 자신 제외 인카운터 멤버. 파티와 같은 이유로 항상 "커밋된" 값만 읽힌다
	// (평가 시차) — _AllyFocus가 적 협공 성향으로 그대로 작동하는 지점.
	// 味方ターゲット収集：自分以外のエンカウンターメンバー。_AllyFocusが敵の挟撃性向としてそのまま機能。
	for (const AEnemyCharacter* EncounterMember : Encounter->GetAliveMembers())
	{
		if (!EncounterMember || EncounterMember == OwnerCharacter) continue;

		if (const UTargetSelectorComponent* Selector = EncounterMember->GetTargetSelector())
		{
			if (const AActor* EncounterMemberTarget = Selector->GetCurrentTarget())
			{
				Context.AllyTargets.Add(EncounterMemberTarget);
			}
		}
	}
	return Context;
}

TArray<AActor*> UEnemyTargetSelectorComponent::GatherCandidates() const
{
	// 후보는 인카운터의 알려진 적 목록. 
	// 공급 층 = 셀렉터에 후보를 넘기는 쪽. 적 측은 소속 인카운터, 파티 측은 PartyManager(파티 인지).
	// 셀렉터는 받은 후보 중 누구를 고를지만 정하고, 후보에 누가 들어가는지는 공급 층이 정한다.
	// 候補はエンカウンターの既知敵リスト。
	// 供給層＝セレクタへ候補を渡す側。敵側は所属エンカウンター、パーティ側はPartyManager（パーティ知覚）。
	// セレクタは受け取った候補から誰を選ぶかのみ決め、候補に誰が入るかは供給層が決める。
	const AEnemyCharacter* OwnerCharacter = Cast<AEnemyCharacter>(GetOwner());
	const AEnemyEncounterBase* Encounter = OwnerCharacter ? OwnerCharacter->GetEncounter() : nullptr;
	return Encounter ? Encounter->GetKnownHostiles() : TArray<AActor*>();
}