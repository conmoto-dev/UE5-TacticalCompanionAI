#include "Enemies/Formation/EnemySubFormation_Scatter.h"


TArray<FEnemyFormationSlot> UEnemySubFormation_Scatter::BuildSlots_Implementation(
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

	const int32 StreamSeed = bUseFixedSeed ? RandomSeed : FMath::Rand();
	FRandomStream RandomStream(StreamSeed);

	TArray<FVector> ExistingLocations;
	ExistingLocations.Reserve(SlotCount);

	for (int32 SlotIndex = 0; SlotIndex < SlotCount; ++SlotIndex)
	{
		// [1] 기존 슬롯과 최대한 떨어진 랜덤 위치를 고른다.
		// [1] 既存スロットからできるだけ離れたランダム位置を選ぶ。
		const FVector SlotLocation = PickScatterLocation(
			RandomStream,
			ExistingLocations,
			Center,
			ForwardVector,
			RightVector);

		ExistingLocations.Add(SlotLocation);
		
		// [2] 設定された向きモードに合わせてスロット回転を作る。
		const FQuat SlotRotation = MakeSlotRotation(
			RandomStream,
			SubFormationWorldTransform,
			SlotLocation);

		const FTransform SlotTransform(SlotRotation, SlotLocation, FVector::OneVector);
		Slots.Emplace(SlotTransform, SlotIndex);
	}

	return Slots;
}

FVector UEnemySubFormation_Scatter::PickScatterLocation(
	FRandomStream& RandomStream,
	const TArray<FVector>& ExistingLocations,
	const FVector& Center,
	const FVector& ForwardVector,
	const FVector& RightVector) const
{
	if (ExistingLocations.Num() <= 0)
	{
		return MakeRandomPointInCircle(RandomStream, Center, ForwardVector, RightVector);
	}

	const int32 SafeCandidateCount = FMath::Max(1, CandidateCount);
	const float RequiredDistanceSquared = FMath::Square(MinDistance);

	FVector BestLocation = Center;
	float BestDistanceSquared = -1.0f;

	for (int32 CandidateIndex = 0; CandidateIndex < SafeCandidateCount; ++CandidateIndex)
	{
		// [1] 원 안에서 후보 위치를 하나 뽑는다.
		// [1] 円の中から候補位置を1つ選ぶ。
		const FVector CandidateLocation =
			MakeRandomPointInCircle(RandomStream, Center, ForwardVector, RightVector);

		const float NearestDistanceSquared =
			CalculateNearestDistanceSquared(CandidateLocation, ExistingLocations);

		if (NearestDistanceSquared > BestDistanceSquared)
		{
			BestDistanceSquared = NearestDistanceSquared;
			BestLocation = CandidateLocation;
		}

		// [2] 최소 거리 조건을 만족하면 즉시 사용한다.
		// [2] 最小距離条件を満たした場合はすぐ使用する。
		if (NearestDistanceSquared >= RequiredDistanceSquared)
		{
			return CandidateLocation;
		}
	}

	return BestLocation;
}

FVector UEnemySubFormation_Scatter::MakeRandomPointInCircle(
	FRandomStream& RandomStream,
	const FVector& Center,
	const FVector& ForwardVector,
	const FVector& RightVector) const
{
	// [1] sqrt를 사용해서 원 내부에 균일하게 분포시킨다.
	// [1] sqrtを使い、円内に均一分布させる。
	const float RandomRadius = FMath::Sqrt(RandomStream.FRand()) * ScatterRadius;
	const float RandomAngle = RandomStream.FRandRange(0.0f, 2.0f * PI);

	const FVector LocalDirection =
		ForwardVector * FMath::Cos(RandomAngle)
		+ RightVector * FMath::Sin(RandomAngle);

	return Center + LocalDirection * RandomRadius;
}

float UEnemySubFormation_Scatter::CalculateNearestDistanceSquared(
	const FVector& CandidateLocation,
	const TArray<FVector>& ExistingLocations) const
{
	float NearestDistanceSquared = TNumericLimits<float>::Max();

	for (const FVector& ExistingLocation : ExistingLocations)
	{
		const float DistanceSquared =
			FVector::DistSquared2D(CandidateLocation, ExistingLocation);

		NearestDistanceSquared = FMath::Min(NearestDistanceSquared, DistanceSquared);
	}

	return NearestDistanceSquared;
}

FQuat UEnemySubFormation_Scatter::MakeSlotRotation(
	FRandomStream& RandomStream,
	const FTransform& SubFormationWorldTransform,
	const FVector& SlotLocation) const
{
	const FVector Center = SubFormationWorldTransform.GetLocation();
	const FVector ForwardVector = SubFormationWorldTransform.GetUnitAxis(EAxis::X);
	const FVector RightVector = SubFormationWorldTransform.GetUnitAxis(EAxis::Y);
	const FVector UpVector = SubFormationWorldTransform.GetUnitAxis(EAxis::Z);

	FVector FacingDirection = ForwardVector;

	switch (FacingMode)
	{
	case EEnemyScatterFacingMode::FormationForward:
		FacingDirection = ForwardVector;
		break;

	case EEnemyScatterFacingMode::RandomYaw:
	{
		const float RandomAngle = RandomStream.FRandRange(0.0f, 2.0f * PI);

		FacingDirection =
			ForwardVector * FMath::Cos(RandomAngle)
			+ RightVector * FMath::Sin(RandomAngle);
		break;
	}

	case EEnemyScatterFacingMode::Outward:
		FacingDirection = (SlotLocation - Center).GetSafeNormal();

		if (FacingDirection.IsNearlyZero())
		{
			FacingDirection = ForwardVector;
		}
		break;

	case EEnemyScatterFacingMode::Center:
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

	return FRotationMatrix::MakeFromXZ(FacingDirection, UpVector).ToQuat();
}