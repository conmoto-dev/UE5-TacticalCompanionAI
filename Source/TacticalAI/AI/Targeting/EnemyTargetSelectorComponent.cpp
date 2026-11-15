#include "AI/Targeting/EnemyTargetSelectorComponent.h"
#include "Characters/EnemyCharacter.h"
#include "Enemies/Encounter/EnemyEncounter.h"

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
	const AEnemyEncounter* Encounter = OwnerCharacter ? OwnerCharacter->GetEncounter() : nullptr;
	if (!Encounter)
	{
		return Context;
	}

	// 자신을 제외한 인카운터 멤버의 현재 타겟을 평가 정보에 추가한다.
	// 각 셀렉터의 GetCurrentTarget()으로 선택된 타겟을 조회한다.
	// 自分以外のエンカウンターメンバーの現在のターゲットを評価情報に追加する。
	// 各セレクタのGetCurrentTarget()で選択済みのターゲットを取得する。
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
	// 소속 인카운터가 관리하는 알려진 적 목록을 타겟 후보로 반환한다.
	// 인카운터가 없거나 교전 상태가 아니면 후보는 비어 있다.
	// 所属エンカウンターが管理する既知の敵一覧をターゲット候補として返す。
	// エンカウンターがない場合、または交戦状態でない場合、候補は空になる。
	const AEnemyCharacter* OwnerCharacter = Cast<AEnemyCharacter>(GetOwner());
	const AEnemyEncounter* Encounter = OwnerCharacter ? OwnerCharacter->GetEncounter() : nullptr;
	return Encounter ? Encounter->GetKnownHostiles() : TArray<AActor*>();
}