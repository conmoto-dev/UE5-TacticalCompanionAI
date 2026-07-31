#include "AI/Strategies/SlotGeneratorStrategy.h"

#include "GameFramework/Actor.h"

bool USlotGeneratorStrategy::ShouldReposition(const FSlotGenContext& Context,
	const FVector& CommittedSlot, float TimeSinceCommit) const
{
	// 하드 트리거: 커밋 슬롯에서 타겟을 못 때리면 그 자리는 쓸모없음 → 재배치.
	// ハード：コミットスロットからターゲットを撃てないなら無価値 → 再配置。
	const AActor* Target = Context.PrimaryTarget.Get();
	if (!Target) return false;

	const float SurfaceDist =
		FVector::Dist2D(CommittedSlot, Target->GetActorLocation()) - Context.BaseRadius;
	
	return SurfaceDist > Context.AttackRange;
}