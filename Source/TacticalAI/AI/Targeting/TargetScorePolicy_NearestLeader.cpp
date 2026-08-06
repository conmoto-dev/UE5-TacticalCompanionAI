#include "AI/Targeting/TargetScorePolicy_NearestLeader.h"

float UTargetScorePolicy_NearestLeader::ScoreTarget_Implementation(
	const FTargetingContext& Context, const AActor* Candidate) const
{
	// 리더 정보가 없는 컨텍스트(미래의 적 측)에서는 기여하지 않는다.
	// リーダー不在のコンテキスト（将来の敵側）では寄与しない。
	if (!Context.bHasLeader || !Candidate) return 0.f;

	// [1] 게이트: 나→리더 거리(저속 변수) 기반 배율. 리더 근처면 0 → 정책 통째로 침묵.
	//     연속 항의 출렁임은 홀드(HoldDuration)가 전환 빈도 상한으로 억제한다.
	// ゲート：自分→リーダー距離（低速変数）による倍率。付近なら沈黙。
	// 連続項の揺れはホールドが切替頻度の上限として抑制する。
	const float Scale = ComputeScoringScaleByLeaderDistance(Context);
	if (Scale <= 0.f) return 0.f;

	// [2] 후보→리더 근접도 (반감식). 리더 근처 적일수록 1에 접근.
	// 候補→リーダー近接度（半減式）。リーダー付近の敵ほど1へ。
	const float Dist = FVector::Dist(Context.LeaderLocation, Candidate->GetActorLocation());
	const float Proximity = ScoreHalfDistance / (ScoreHalfDistance + Dist);

	// [3] 곱 합성: 두 축 모두 [0,1]이므로 계약 유지.
	// 乗算合成。両軸[0,1]のため契約維持。
	return Scale * Proximity;
}

float UTargetScorePolicy_NearestLeader::ComputeScoringScaleByLeaderDistance(const FTargetingContext& Context) const
{
	const float SelfToLeader = FVector::Dist(Context.SelfLocation, Context.LeaderLocation);

	// [1] Start~Full 구간을 [0,1]로 정규화. Full ≤ Start 설정 실수 방어 (최소 1cm 폭 보장).
	// Start〜Full区間を[0,1]へ正規化。設定ミス防御で最低幅を保証。
	const float RampWidth = FMath::Max(ScoringFullDistanceFromLeader - ScoringStartDistanceFromLeader, 1.f);
	const float Normalized = FMath::Clamp((SelfToLeader - ScoringStartDistanceFromLeader) / RampWidth, 0.f, 1.f);

	// [2] 거듭제곱 곡선: Start 근처(소폭 이탈)는 둔감, Full 접근(확실한 이탈)에서 급상승.
	//     클램프 후 거듭제곱이므로 반환은 [0,1] 유지.
	// べき乗カーブ：僅かな離脱には鈍く、確実な離脱で急上昇。返却は[0,1]維持。
	return FMath::Pow(Normalized, ScoringCurveExponent);
}