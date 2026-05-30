#pragma once
#include "CoreMinimal.h"
#include "Navigation/CrowdFollowingComponent.h"
#include "AI/CrowdAvoidanceTypes.h"
#include "TacticalCrowdFollowingComponent.generated.h"

/**
 * Detour Crowd 기반으로 동료의 회피 우선순위를 동적으로 제어하는 커스텀 PathFollowing.
 */
UCLASS()
class TACTICALAI_API UTacticalCrowdFollowingComponent : public UCrowdFollowingComponent
{
	GENERATED_BODY()
public:
	UTacticalCrowdFollowingComponent(const FObjectInitializer& ObjectInitializer);

	/** 회피 역할 적용. 같은 role 재요청은 디바운싱으로 무시. */
	UFUNCTION(BlueprintCallable, Category = "Tactical AI|Crowd")
	void ApplyRole(ECrowdAvoidanceRole CrowdRole);

	UPROPERTY(EditDefaultsOnly, Category="Crowd", meta=(ClampMin="1.0"))
	float CrowdAvoidanceRangeMultiplier = 1.2f;
	
protected:
	virtual void BeginPlay() override;

private:
	// 디바운싱: role이 실제로 바뀔 때만 엔진 내부 dirty flag를 건드린다.
	ECrowdAvoidanceRole CachedRole = ECrowdAvoidanceRole::Normal;
	bool bHasInitializedState = false;
};