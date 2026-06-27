#include "Enemies/Formation/EnemySubFormationStrategy_Line.h"

// =========================================================================
// UEnemySubFormationStrategy_Line
// =========================================================================
TArray<FEnemyFormationSlot> UEnemySubFormationStrategy_Line::BuildSlots_Implementation(
	const FTransform& SubFormationWorldTransform,
	const int32 SlotCount) const
{
	TArray<FEnemyFormationSlot> Slots;

	if (SlotCount <= 0)
	{
		return Slots;
	}

	Slots.Reserve(SlotCount);

	const FVector Origin = SubFormationWorldTransform.GetLocation();
	const FQuat Rotation = SubFormationWorldTransform.GetRotation();
	const FVector RightVector = SubFormationWorldTransform.GetUnitAxis(EAxis::Y);

	const float FirstOffset =
		bCenterAligned
			? -0.5f * static_cast<float>(SlotCount - 1) * SlotSpacing
			: 0.0f;

	for (int32 SlotIndex = 0; SlotIndex < SlotCount; ++SlotIndex)
	{
		// [1] SubFormation 기준 Y축 방향으로 슬롯 위치를 만든다.
		// [1] SubFormation基準のY軸方向にスロット位置を作る。
		const float LocalOffsetY = FirstOffset + static_cast<float>(SlotIndex) * SlotSpacing;
		const FVector SlotLocation = Origin + RightVector * LocalOffsetY;

		// [2] 슬롯 방향은 SubFormation 기준 회전을 유지한다.
		// [2] スロットの向きはSubFormation基準の回転を維持する。
		const FTransform SlotTransform(Rotation, SlotLocation, FVector::OneVector);
		Slots.Emplace(SlotTransform, SlotIndex);
	}

	return Slots;
}