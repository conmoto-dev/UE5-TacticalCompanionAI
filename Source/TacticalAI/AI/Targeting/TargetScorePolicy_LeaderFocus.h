#pragma once

#include "CoreMinimal.h"
#include "AI/Targeting/TargetScorePolicy.h"
#include "TargetScorePolicy_LeaderFocus.generated.h"

// ========================================
// 리더와 같은 타겟 우선 정책.
// リーダーと同一ターゲット優先。
// ========================================
UCLASS(meta = (DisplayName = "Leader Focus"))
class TACTICALAI_API UTargetScorePolicy_LeaderFocus : public UTargetScorePolicy
{
	GENERATED_BODY()

public:
	virtual float ScoreTarget_Implementation(const FTargetingContext& Context, const AActor* Candidate) const override;
};