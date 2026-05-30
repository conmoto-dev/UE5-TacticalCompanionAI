// TacticalCrowdFollowingComponent.cpp
#include "AI/Components/TacticalCrowdFollowingComponent.h"

UTacticalCrowdFollowingComponent::UTacticalCrowdFollowingComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UTacticalCrowdFollowingComponent::BeginPlay()
{
	Super::BeginPlay();
}

void UTacticalCrowdFollowingComponent::ApplyRole(ECrowdAvoidanceRole CrowdRole)
{
	// 상태가 안 바뀌었으면 일찍 반환 (불필요한 재등록 방지).
	if (bHasInitializedState && CachedRole == CrowdRole) return;
	CachedRole = CrowdRole;
	bHasInitializedState = true;
	
	SetCrowdAvoidanceRangeMultiplier(CrowdAvoidanceRangeMultiplier);
	
	UE_LOG(LogTemp, Warning, TEXT("[Crowd] ApplyRole called: role=%d on %s"),
		(int32)CrowdRole, *GetNameSafe(GetOwner()));
	
	using namespace CrowdGroupBits;
	
	// 그룹 비트:
	//  Leader(1)   : 절대 존엄. 아무도 안 피함.
	//  Normal(2)   : 일반 동료. 리더·동료·양보자 모두 회피.
	//  Yielding(4) : 양보 중. 리더·동료를 피함.
	switch (CrowdRole)
	{
	case ECrowdAvoidanceRole::Leader:
		SetAvoidanceGroup(Leader);
		SetGroupsToAvoid(0);
		SetGroupsToIgnore(Normal | Yielding);
		break;

	case ECrowdAvoidanceRole::Yielding:
		SetAvoidanceGroup(Yielding);
		SetGroupsToAvoid(Leader | Normal);
		SetGroupsToIgnore(0);
		break;

	case ECrowdAvoidanceRole::Normal:
	default:
		SetAvoidanceGroup(Normal);
		SetGroupsToAvoid(Leader | Normal | Yielding);
		SetGroupsToIgnore(0);
		break;
	}
}