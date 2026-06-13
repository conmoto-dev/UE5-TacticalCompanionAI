#include "AI/Strategies/SlotGeneratorStrategy_RangedSafe.h"

FVector USlotGeneratorStrategy_RangedSafe::GenerateSlot(const FSlotGenContext& Context) const
{
	return Context.RequesterLocation;
}