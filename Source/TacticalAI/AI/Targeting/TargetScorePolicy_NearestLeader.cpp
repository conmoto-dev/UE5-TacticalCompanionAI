#include "AI/Targeting/TargetScorePolicy_NearestLeader.h"

float UTargetScorePolicy_NearestLeader::ScoreTarget_Implementation(
	const FTargetingContext& Context, const AActor* Candidate) const
{
	// 리더 정보가 없는 컨텍스트(미래의 적 측)에서는 기여하지 않는다.
	// リーダー不在のコンテキスト（将来の敵側）では寄与しない。
	if (!Context.bHasLeader || !Candidate) return 0.f;
	
	// ゲート：リーダー付近なら0で全体沈黙。乗算のため下の近接度の揺れが載らない。
	const float Urgency = ComputeScoringUrgency(Context);
	if (Urgency <= 0.f) return 0.f;
	
	// 候補判定は二値のみ。連続近接度は廃止 — 高速変数を大きさに載せると振動する。
	// 半径内は全員同点のため、最終勝者は組み合わせの他ポリシーが決める。
	const float DistToLeader = FVector::Dist(Context.LeaderLocation, Candidate->GetActorLocation());
	if (DistToLeader > LeaderAreaRadius) return 0.f;

	return Urgency;
}

float UTargetScorePolicy_NearestLeader::ComputeScoringUrgency(const FTargetingContext& Context) const
{
	const float SelfToLeader = FVector::Dist(Context.SelfLocation, Context.LeaderLocation);

	// [1] Start~Full 구간을 [0,1]로 정규화. Full ≤ Start 설정 실수 방어 (최소 1cm 폭 보장).
	// Start〜Full区間を[0,1]へ正規化。設定ミス防御で最低幅を保証。
	const float RampWidth = FMath::Max(ScoringFullDistance - ScoringStartDistance, 1.f);
	const float Normalized = FMath::Clamp((SelfToLeader - ScoringStartDistance) / RampWidth, 0.f, 1.f);

	// [2] 거듭제곱 곡선: Start 근처(소폭 이탈)는 둔감, Full 접근(확실한 이탈)에서 급상승.
	//     클램프 후 거듭제곱이므로 반환은 [0,1] 유지 — 점수 계약 보존.
	// べき乗カーブ：僅かな離脱には鈍く、確実な離脱で急上昇。返却は[0,1]維持。
	return FMath::Pow(Normalized, ScoringCurveExponent);
}