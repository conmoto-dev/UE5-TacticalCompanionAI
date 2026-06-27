#include "Enemies/Formation/EnemySubFormationStrategy_Circle.h"

TArray<FEnemyFormationSlot> UEnemySubFormationStrategy_Circle::BuildSlots_Implementation(
	const FTransform& SubFormationWorldTransform,
	const int32 SlotCount) const
{
	TArray<FEnemyFormationSlot> Slots;

	if (SlotCount <= 0)
	{
		return Slots;
	}

	Slots.Reserve(SlotCount);

	const FVector Center = SubFormationWorldTransform.GetLocation();
	const FVector ForwardVector = SubFormationWorldTransform.GetUnitAxis(EAxis::X);
	const FVector RightVector = SubFormationWorldTransform.GetUnitAxis(EAxis::Y);
	const FVector UpVector = SubFormationWorldTransform.GetUnitAxis(EAxis::Z);

	const float DirectionSign = bClockwise ? -1.0f : 1.0f;
	const float AngleStepDegrees = 360.0f / static_cast<float>(SlotCount);

	for (int32 SlotIndex = 0; SlotIndex < SlotCount; ++SlotIndex)
	{
		// [1] SubFormation基準のX/Y平面上で円周上の位置を計算する。
		const float AngleDegrees =
			StartAngleDegrees + DirectionSign * AngleStepDegrees * static_cast<float>(SlotIndex);

		const float AngleRadians = FMath::DegreesToRadians(AngleDegrees);

		const FVector RadialDirection =
			ForwardVector * FMath::Cos(AngleRadians)
			+ RightVector * FMath::Sin(AngleRadians);

		const FVector SlotLocation = Center + RadialDirection * Radius;
		
		// [2] 設定されたモードに応じてスロットの向きを決める。
		FVector FacingDirection = ForwardVector;

		switch (FacingMode)
		{
		case EEnemyCircleFacingMode::FaceFormationForward:
			FacingDirection = ForwardVector;
			break;

		case EEnemyCircleFacingMode::FaceOutward:
			FacingDirection = (SlotLocation - Center).GetSafeNormal();

			if (FacingDirection.IsNearlyZero())
			{
				FacingDirection = ForwardVector;
			}
			break;

		case EEnemyCircleFacingMode::FaceCenter:
			FacingDirection = (Center - SlotLocation).GetSafeNormal();

			if (FacingDirection.IsNearlyZero())
			{
				FacingDirection = -ForwardVector;
			}
			break;

		default:
			FacingDirection = ForwardVector;
			break;
		}
		
		// [3] スロットのX軸が向き方向になるように回転を作る。
		const FQuat SlotRotation =
			FRotationMatrix::MakeFromXZ(FacingDirection, UpVector).ToQuat();

		const FTransform SlotTransform(SlotRotation, SlotLocation, FVector::OneVector);
		Slots.Emplace(SlotTransform, SlotIndex);
	}

	return Slots;
}