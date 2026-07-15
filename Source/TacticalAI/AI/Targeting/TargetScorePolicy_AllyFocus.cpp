#include "AI/Targeting/TargetScorePolicy_AllyFocus.h"

float UTargetScorePolicy_AllyFocus::ScoreTarget_Implementation(
	const FTargetingContext& Context, const AActor* Candidate) const
{
	if (!Candidate || Context.AllyTargets.Num() == 0) return 0.f;

	// 이 후보를 잡은 동료 비율 → 0~1. 많이 몰릴수록 점수가 올라 합류 성향이 강해진다.
	// この候補を狙う味方の割合→0〜1。集中するほど合流性向が強まる。
	int32 Count = 0;
	for (const AActor* AllyTarget : Context.AllyTargets)
	{
		if (AllyTarget == Candidate)
		{
			++Count;
		}
	}
	return static_cast<float>(Count) / static_cast<float>(Context.AllyTargets.Num());
}