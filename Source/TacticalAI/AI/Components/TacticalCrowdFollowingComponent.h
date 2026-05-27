#pragma once

#include "CoreMinimal.h"
#include "Navigation/CrowdFollowingComponent.h"
#include "TacticalCrowdFollowingComponent.generated.h"

/**
 * Detour Crowd를 기반으로 동료 간의 회피 및 양보(Yield) 시
 * 우선순위(Priority)를 동적으로 제어하기 위한 커스텀 패스 팔로잉 컴포넌트.
 */
UCLASS()
class TACTICALAI_API UTacticalCrowdFollowingComponent : public UCrowdFollowingComponent
{
	GENERATED_BODY()

public:
	UTacticalCrowdFollowingComponent(const FObjectInitializer& ObjectInitializer);

	/**
	 * 동적 회피 상태 설정 API
	 * @param bIsLeader 플레이어(또는 AI 리더) 여부. swap 시 PlayerController possessed 캐릭터에게 true로 호출 예정
	 * @param bIsYielding 양보(Yield) 상태 여부. 양보 중일 땐 다른 AI에게 길을 내어줌.
	 */
	UFUNCTION(BlueprintCallable, Category = "Tactical AI|Crowd")
	void SetTacticalAvoidanceState(bool bIsLeader, bool bIsYielding);

protected:
	virtual void BeginPlay() override;

private:
	// 💡 최적화(디바운싱): 상태가 변할 때만 엔진 내부의 Dirty Flag를 건드리기 위한 캐시
	bool bCachedIsLeader = false;
	bool bCachedIsYielding = false;
	bool bHasInitializedState = false; // 최초 1회 실행 보장
};