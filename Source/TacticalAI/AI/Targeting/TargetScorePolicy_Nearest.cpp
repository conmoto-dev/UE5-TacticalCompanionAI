#include "AI/Targeting/TargetScorePolicy_Nearest.h"

float UTargetScorePolicy_Nearest::ScoreTarget_Implementation(
	const FTargetingContext& Context, const AActor* Candidate) const
{
	if (!Candidate) return 0.f;
	
	// 距離→0〜1減衰。近距離の差を大きく、遠距離の差を圧縮して評価。
	const float Dist = FVector::Dist(Context.SelfLocation, Candidate->GetActorLocation());
	return ScoreHalfDistance / (ScoreHalfDistance + Dist);
}