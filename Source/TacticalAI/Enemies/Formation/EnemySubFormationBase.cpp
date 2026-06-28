#include "Enemies/Formation/EnemySubFormationBase.h"


TArray<FEnemyFormationSlot> UEnemySubFormationBase::BuildSlots_Implementation(
	const FTransform& SubFormationWorldTransform,
	const int32 SlotCount) const
{
	return {};
}