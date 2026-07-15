#include "AI/Targeting/TargetScorePolicy_LeaderFocus.h"

float UTargetScorePolicy_LeaderFocus::ScoreTarget_Implementation(
	const FTargetingContext& Context, const AActor* Candidate) const
{
	// 이진 판정: 리더의 타겟이면 1, 아니면 0. 강도 조절은 가중치의 몫.
	// 리더 타겟 부재(자신이 리더 포함)면 기여 없음 — 입력 부재 = 퇴화.
	// 二値判定。強度調整は重みの役割。リーダーターゲット不在なら寄与なし。
	if (!Candidate) return 0.f;
	
	return (Context.LeaderTarget == Candidate) ? 1.f : 0.f;
}