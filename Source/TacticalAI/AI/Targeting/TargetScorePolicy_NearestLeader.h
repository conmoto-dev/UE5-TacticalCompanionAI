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
	// スコア半減距離（リーダー基準）。
	UPROPERTY(EditAnywhere, Category = "Targeting", meta = (ClampMin = "1.0", Units = "cm"))
	float ScoreHalfDistance = 800.f;
};