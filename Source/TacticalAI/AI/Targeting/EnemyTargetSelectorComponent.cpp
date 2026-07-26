#include "AI/Targeting/EnemyTargetSelectorComponent.h"
#include "Characters/EnemyCharacter.h"
#include "Enemies/Group/EnemyGroup.h"

FTargetingContext UEnemyTargetSelectorComponent::BuildContext() const
{
	FTargetingContext Context;
	Context.SelfLocation = GetOwner()->GetActorLocation();
	// bHasLeaderは基本的にfalseのまま。

	const AEnemyCharacter* OwnerCharacter = Cast<AEnemyCharacter>(GetOwner());
	const AEnemyGroup* Group = OwnerCharacter ? OwnerCharacter->GetEnemyGroup() : nullptr;
	if (!Group)
	{
		return Context;
	}

	// 동료 타겟 수집: 자신 제외 그룹원. 파티와 같은 이유로 항상 "커밋된" 값만 읽힌다
	// (평가 시차) — _AllyFocus가 적 협공 성향으로 그대로 작동하는 지점.
	// 味方ターゲット収集：自分以外のグループ員。_AllyFocusが敵の挟撃性向としてそのまま機能。
	for (const AEnemyCharacter* GroupMember : Group->GetAliveMembers())
	{
		if (!GroupMember || GroupMember == OwnerCharacter) continue;

		if (const UTargetSelectorComponent* Selector = GroupMember->GetTargetSelector())
		{
			if (const AActor* GroupMemberTarget = Selector->GetCurrentTarget())
			{
				Context.AllyTargets.Add(GroupMemberTarget);
			}
		}
	}
	return Context;
}

TArray<AActor*> UEnemyTargetSelectorComponent::GatherCandidates() const
{
	// 후보는 그룹의 알려진 적 목록. Engaged 게이팅도 그룹이 담당 —
	// 인지·교전 판단은 공급 층의 일이지 셀렉터·정책의 일이 아니다.
	// 候補はグループの既知敵リスト。Engagedゲートもグループ側の責務。
	const AEnemyCharacter* OwnerCharacter = Cast<AEnemyCharacter>(GetOwner());
	const AEnemyGroup* Group = OwnerCharacter ? OwnerCharacter->GetEnemyGroup() : nullptr;
	return Group ? Group->GetKnownHostiles() : TArray<AActor*>();
}