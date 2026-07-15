#pragma once

#include "CoreMinimal.h"
#include "AI/Targeting/TargetScorePolicy.h"
#include "TargetScorePolicy_AllyFocus.generated.h"

// =========================================================================
// 동료들이 잡은 타겟 우선 정책.
// 味方の現在ターゲット優先。挟撃「性向」の実装。
// =========================================================================
UCLASS(meta = (DisplayName = "Ally Focus"))
class TACTICALAI_API UTargetScorePolicy_AllyFocus : public UTargetScorePolicy
{
	GENERATED_BODY()

public:
	virtual float ScoreTarget_Implementation(const FTargetingContext& Context, const AActor* Candidate) const override;
};