#pragma once

#include "CoreMinimal.h"
#include "AI/Targeting/TargetScorePolicy.h"
#include "TargetScorePolicy_NearestLeader.generated.h"

// ==================================================
// リーダー位置基準の近接優先。
// ==================================================
UCLASS(meta = (DisplayName = "Nearest Leader"))
class TACTICALAI_API UTargetScorePolicy_NearestLeader : public UTargetScorePolicy
{
	GENERATED_BODY()
	
public:
	UTargetScorePolicy_NearestLeader() { HoldDuration = 7.f; }
	
	virtual float ScoreTarget_Implementation(const FTargetingContext& Context, const AActor* Candidate) const override;

protected:
	// 리더에서 이 거리(cm)까지는 이 정책을 무시 (점수 0). 리더 근처 타겟 점수 반영이 시작되는 거리.
	// リーダーからこの距離(cm)まではこのポリシーを無視（スコア0）。リーダー付近ターゲットのスコア反映が始まる距離。
	UPROPERTY(EditAnywhere, Category = "Targeting", meta = (ClampMin = "0.0", Units = "cm"))
	float ScoringStartDistanceFromLeader = 500.f;
	
	// この距離でリーダー付近ターゲットのスコア反映が最大。
	UPROPERTY(EditAnywhere, Category = "Targeting", meta = (ClampMin = "0.0", Units = "cm"))
	float ScoringFullDistanceFromLeader = 1500.f;

	// 후보→리더 거리가 이 값(cm)일 때 근접도 0.5 (반감 거리).
	// 候補→リーダー距離がこの値(cm)の時に近接度0.5（半減距離）。
	UPROPERTY(EditAnywhere, Category = "Targeting", meta = (ClampMin = "1.0", Units = "cm"))
	float ScoreHalfDistance = 800.f;
	
	// 상승 곡선 지수. 1=선형, 클수록 Start 근처에선 둔감하고 Full에 가까워질수록 급격히 상승.
	// "조금 벗어난 정도로는 안 흔들리고, 확실히 멀어졌을 때만 강하게 복귀"의 조절 다이얼.
	// 立ち上がり曲線の指数。1＝線形、大きいほどStart付近で鈍くFull付近で急峻。
	UPROPERTY(EditAnywhere, Category = "Targeting", meta = (ClampMin = "1.0", ClampMax = "8.0"))
	float ScoringCurveExponent = 2.f;

private:
	// 나→리더 거리 기반 이탈도 [0,1]. 저속 변수만 읽는 게이트 — 후보와 무관.
	// 自分→リーダー距離による離脱度[0,1]。低速変数のみ読むゲート。
	float ComputeScoringScaleByLeaderDistance(const FTargetingContext& Context) const;
};