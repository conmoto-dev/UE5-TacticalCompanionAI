#pragma once

#include "CoreMinimal.h"
#include "AI/Targeting/TargetScorePolicy.h"
#include "TargetScorePolicy_Nearest.generated.h"

// =========================================================================
// 自位置基準の近接優先。
// =========================================================================
UCLASS()
class TACTICALAI_API UTargetScorePolicy_Nearest : public UTargetScorePolicy
{
	GENERATED_BODY()

public:
	virtual float ScoreTarget_Implementation(const FTargetingContext& Context, const AActor* Candidate) const override;

protected:
	// 점수 반감 거리. 이 거리에서 0.5점.
	// スコア半減距離。この距離で0.5点。
	UPROPERTY(EditAnywhere, Category = "Targeting", meta = (ClampMin = "1.0", Units = "cm"))
	float ScoreHalfDistance = 1000.f;
};