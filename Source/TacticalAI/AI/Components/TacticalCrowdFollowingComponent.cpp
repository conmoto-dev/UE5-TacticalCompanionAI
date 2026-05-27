#include "AI/Components/TacticalCrowdFollowingComponent.h"

UTacticalCrowdFollowingComponent::UTacticalCrowdFollowingComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UTacticalCrowdFollowingComponent::BeginPlay()
{
	Super::BeginPlay();
}

void UTacticalCrowdFollowingComponent::SetTacticalAvoidanceState(bool bIsLeader, bool bIsYielding)
{
	// [최적화] 상태가 변하지 않았다면 일찍 반환 (불필요한 재등록/오버헤드 방지)
	if (bHasInitializedState && bCachedIsLeader == bIsLeader && bCachedIsYielding == bIsYielding)
	{
		return;
	}

	bCachedIsLeader = bIsLeader;
	bCachedIsYielding = bIsYielding;
	bHasInitializedState = true;

	// 💡 Detour Crowd 회피 그룹 (비트마스크)
	// 1 (1<<0) : Leader (절대 존엄)
	// 2 (1<<1) : Normal Follower (일반 동료)
	// 4 (1<<2) : Yielding Follower (양보 중)

	if (bIsLeader)
	{
		SetAvoidanceGroup(1);
		SetGroupsToAvoid(0);
		SetGroupsToIgnore(2 | 4);
	}
	else if (bIsYielding)
	{
		SetAvoidanceGroup(4);
		SetGroupsToAvoid(1 | 2); // 리더(1)와 일반 동료(2)를 피함
		SetGroupsToIgnore(0);
	}
	else
	{
		SetAvoidanceGroup(2);
		SetGroupsToAvoid(1 | 2 | 4); // 리더(1) 최우선 회피, 동료(2)와 겹침 방지, Yield중인 동료(4) 회피
		SetGroupsToIgnore(0);
	}
}