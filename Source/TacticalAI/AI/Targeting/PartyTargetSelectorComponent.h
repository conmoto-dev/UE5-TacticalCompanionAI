#pragma once

#include "CoreMinimal.h"
#include "AI/Targeting/TargetSelectorComponent.h"
#include "PartyTargetSelectorComponent.generated.h"

// =========================================================================
// パーティ側ターゲットセレクタ。
// =========================================================================
UCLASS(ClassGroup=(TacticalAI), meta=(BlueprintSpawnableComponent))
class TACTICALAI_API UPartyTargetSelectorComponent : public UTargetSelectorComponent
{
	GENERATED_BODY()

protected:
	virtual FTargetingContext BuildContext() const override;
	virtual TArray<AActor*> GatherCandidates() const override;
};