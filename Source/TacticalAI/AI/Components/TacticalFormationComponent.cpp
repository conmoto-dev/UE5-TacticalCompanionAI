#include "AI/Components/TacticalFormationComponent.h"
#include "NavigationSystem.h"
#include "Engine/World.h"

FVector UTacticalFormationComponent::AdjustLocationForEnvironment(const FVector& IdealLocation, const FVector& AnchorOrigin, const AActor* IgnoreActor) const
{
	// [1] 슬로프 Z 보정.
	FVector AdjustedIdeal = IdealLocation;
	float GroundZ;
	if (TryFindGroundZ(IdealLocation, GroundZ, IgnoreActor))
	{
		AdjustedIdeal.Z = GroundZ;
	}

	// [2] NavMesh 투영 (1차 진실).
	FVector NavResult;
	if (TryProjectToNavMesh(AdjustedIdeal, NavResult))
	{
		return NavResult;
	}

	// [3] 벽 슬라이드 (대체 탐색).
	FVector SlidLocation;
	if (TryCalculateWallSlide(AnchorOrigin, AdjustedIdeal, IgnoreActor, SlidLocation))
	{
		if (TryProjectToNavMesh(SlidLocation, NavResult))
		{
			return NavResult;
		}
	}

	// [4] 전부 실패 → anchor 쪽으로 끌어당김.
	return CalculateFallbackLocation(AnchorOrigin, IdealLocation);
}

bool UTacticalFormationComponent::TryProjectToNavMesh(const FVector& Point, FVector& OutResult) const
{
	UNavigationSystemV1* NavSys = UNavigationSystemV1::GetCurrent(GetWorld());
	if (!NavSys) return false;

	FNavLocation ProjectedLoc;
	if (NavSys->ProjectPointToNavigation(Point, ProjectedLoc, FVector(50.f, 50.f, 250.f)))
	{
		OutResult = ProjectedLoc.Location;
		return true;
	}
	return false;
}

bool UTacticalFormationComponent::TryFindGroundZ(const FVector& Point, float& OutZ, const AActor* IgnoreActor) const
{
	const FVector TraceStart = Point + FVector(0.f, 0.f, 500.f);
	const FVector TraceEnd   = Point - FVector(0.f, 0.f, 500.f);

	FHitResult Hit;
	FCollisionQueryParams Params;
	if (IgnoreActor) Params.AddIgnoredActor(IgnoreActor);

	if (GetWorld()->LineTraceSingleByChannel(Hit, TraceStart, TraceEnd, ECC_Visibility, Params))
	{
		OutZ = Hit.Location.Z;
		return true;
	}
	return false;
}

bool UTacticalFormationComponent::TryCalculateWallSlide(const FVector& From, const FVector& To, const AActor* IgnoreActor, FVector& OutSlidLocation) const
{
	const float ChestHeight = 90.0f;
	const FVector TraceStart = From + FVector(0.f, 0.f, ChestHeight);
	const FVector TraceEnd   = To   + FVector(0.f, 0.f, ChestHeight);

	FHitResult HitResult(ForceInit);
	FCollisionQueryParams QueryParams;
	if (IgnoreActor) QueryParams.AddIgnoredActor(IgnoreActor);

	const FCollisionShape SphereShape = FCollisionShape::MakeSphere(40.0f);
	const bool bHit = GetWorld()->SweepSingleByChannel(HitResult, TraceStart, TraceEnd, FQuat::Identity, ECC_Visibility, SphereShape, QueryParams);

	if (!bHit) return false;

	const float WallThreshold = 0.7f;
	if (FMath::Abs(HitResult.ImpactNormal.Z) >= WallThreshold) return false;

	FVector HitLoc2D = HitResult.Location; HitLoc2D.Z = 0.f;
	FVector To2D = To; To2D.Z = 0.f;
	FVector Normal2D = HitResult.ImpactNormal; Normal2D.Z = 0.f; Normal2D.Normalize();

	const FVector RemainingDir = To2D - HitLoc2D;
	const FVector SlidingDir = FVector::VectorPlaneProject(RemainingDir, Normal2D);

	const float SafeMargin = 70.0f;
	OutSlidLocation = HitLoc2D + SlidingDir + (Normal2D * SafeMargin);
	OutSlidLocation.Z = To.Z;
	return true;
}

FVector UTacticalFormationComponent::CalculateFallbackLocation(const FVector& AnchorOrigin, const FVector& IdealLocation) const
{
	const float TowDistance = 150.0f;
	const FVector DirToAnchor = (AnchorOrigin - IdealLocation).GetSafeNormal2D();
	return IdealLocation + (DirToAnchor * TowDistance);
}