#include "AI/Strategies/SlotGeneratorStrategy.h"

#include "GameFramework/Actor.h"

bool USlotGeneratorStrategy::ShouldReposition(const FSlotGenContext& Context,
	const FVector& CommittedSlot, float TimeSinceCommit) const
{
	// 하드 트리거: 커밋 슬롯에서 타겟을 못 때리면 그 자리는 쓸모없음 → 재배치.
	// 타겟 없으면 판정 불가 — 상위(anchor 획득 단계)가 이미 파이프라인을 세웠을 상황.
	// ハード：コミットスロットからターゲットを撃てないなら無価値 → 再配置。
	const AActor* Target = Context.PrimaryTarget.Get();
	if (!Target) return false;

	return FVector::Dist2D(CommittedSlot, Target->GetActorLocation()) > Context.AttackRange;
}