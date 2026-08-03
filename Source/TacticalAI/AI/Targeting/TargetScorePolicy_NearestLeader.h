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
	virtual float ScoreTarget_Implementation(const FTargetingContext& Context, const AActor* Candidate) const override;

protected:
	
	UPROPERTY(EditAnywhere, Category = "Targeting", meta = (ClampMin = "0.0", Units = "cm"))
	float LeaderAreaRadius = 1000.f;
	
	// 리더에서 이 거리(cm)까지는 이 정책이 완전 침묵 (점수 0). 복귀 성향이 켜지기 시작하는 지점.
	// リーダーからこの距離まではポリシー完全沈黙。復帰性向が立ち上がり始める地点。
	UPROPERTY(EditAnywhere, Category = "Targeting", meta = (ClampMin = "0.0", Units = "cm"))
	float ScoringStartDistance = 500.f;

	// 이 거리(cm)에서 복귀 성향이 최대(×1).
	// この距離で復帰性向が最大。
	UPROPERTY(EditAnywhere, Category = "Targeting", meta = (ClampMin = "0.0", Units = "cm"))
	float ScoringFullDistance = 1500.f;

	// 상승 곡선 지수. 1=선형, 클수록 Start 근처에선 둔감하고 Full에 가까워질수록 급격히 상승.
	// "조금 벗어난 정도로는 안 흔들리고, 확실히 멀어졌을 때만 강하게 복귀"의 조절 다이얼.
	// 立ち上がり曲線の指数。1＝線形、大きいほどStart付近で鈍くFull付近で急峻。
	UPROPERTY(EditAnywhere, Category = "Targeting", meta = (ClampMin = "1.0", ClampMax = "8.0"))
	float ScoringCurveExponent = 2.f;

private:
	// 나→리더 거리 기반 이탈도 [0,1]. 저속 변수만 읽는 게이트 — 후보와 무관.
	// 自分→リーダー距離による離脱度[0,1]。低速変数のみ読むゲート。
	float ComputeScoringUrgency(const FTargetingContext& Context) const;
};