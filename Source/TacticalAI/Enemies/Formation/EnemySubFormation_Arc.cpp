#include "Enemies/Formation/EnemySubFormation_Arc.h"

TArray<FEnemyFormationSlot> UEnemySubFormation_Arc::BuildSlots_Implementation(
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

	const float SafeArcAngleDegrees = FMath::Clamp(ArcAngleDegrees, 0.0f, 360.0f);
	const float DirectionSign = bClockwise ? -1.0f : 1.0f;

	const float AngleStepDegrees =
		SlotCount > 1
			? SafeArcAngleDegrees / static_cast<float>(SlotCount - 1)
			: 0.0f;

	const float FirstAngleOffsetDegrees =
		SlotCount > 1
			? -0.5f * SafeArcAngleDegrees
			: 0.0f;

	for (int32 SlotIndex = 0; SlotIndex < SlotCount; ++SlotIndex)
	{
		// [1] Arc Angleの範囲内で現在スロットの角度を計算する。
		const float AngleDegrees =
			CenterAngleDegrees
			+ DirectionSign * (FirstAngleOffsetDegrees + AngleStepDegrees * static_cast<float>(SlotIndex));

		const float AngleRadians = FMath::DegreesToRadians(AngleDegrees);

		const FVector RadialDirection =
			ForwardVector * FMath::Cos(AngleRadians)
			+ RightVector * FMath::Sin(AngleRadians);

		const FVector SlotLocation = Center + RadialDirection * Radius;
		
		// [2] Facing Modeに応じてスロットの向きを決める。
		FVector FacingDirection = ForwardVector;

		switch (FacingMode)
		{
		case EEnemyArcFacingMode::FaceFormationForward:
			FacingDirection = ForwardVector;
			break;

		case EEnemyArcFacingMode::FaceOutward:
			FacingDirection = RadialDirection.GetSafeNormal();

			if (FacingDirection.IsNearlyZero())
			{
				FacingDirection = ForwardVector;
			}
			break;

		case EEnemyArcFacingMode::FaceCenter:
			FacingDirection = -RadialDirection.GetSafeNormal();

			if (FacingDirection.IsNearlyZero())
			{
				FacingDirection = -ForwardVector;
			}
			break;

		case EEnemyArcFacingMode::FaceTangent:
			FacingDirection =
				DirectionSign
				* (-ForwardVector * FMath::Sin(AngleRadians)
					+ RightVector * FMath::Cos(AngleRadians));

			if (FacingDirection.IsNearlyZero())
			{
				FacingDirection = ForwardVector;
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