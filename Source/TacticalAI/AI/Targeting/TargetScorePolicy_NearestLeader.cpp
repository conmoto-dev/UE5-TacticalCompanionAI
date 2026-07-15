#include "AI/Targeting/TargetScorePolicy_NearestLeader.h"

float UTargetScorePolicy_NearestLeader::ScoreTarget_Implementation(
	const FTargetingContext& Context, const AActor* Candidate) const
{
	// 리더 정보가 없는 컨텍스트(미래의 적 측)에서는 기여하지 않는다.
	// リーダー不在のコンテキスト（将来の敵側）では寄与しない。
	if (!Context.bHasLeader || !Candidate) return 0.f;

	const float Dist = FVector::Dist(Context.LeaderLocation, Candidate->GetActorLocation());
	return ScoreHalfDistance / (ScoreHalfDistance + Dist);
}