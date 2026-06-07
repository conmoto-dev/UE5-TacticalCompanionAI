#include "AI/Components/TacticalFormationComponent.h"
#include "NavigationSystem.h"
#include "Engine/World.h"
#include "Party/PartyManager.h"
#include "Characters/PartyCharacter.h"

APartyManager* UTacticalFormationComponent::GetOwningPartyManager() const
{
	return Cast<APartyManager>(GetOwner());
}

TArray<APartyCharacter*> UTacticalFormationComponent::GetPartyFollowers() const
{
	if (const APartyManager* Manager = GetOwningPartyManager())
	{
		return Manager->GetFollowers();
	}
	return TArray<APartyCharacter*>();
}

FVector UTacticalFormationComponent::AdjustLocationForEnvironment(const FVector& IdealLocation, const FVector& AnchorOrigin, const AActor* IgnoreActor) const
{
	// [1] 슬로프 Z 보정. 경사면이면 슬롯 높이를 지면에 맞춤.
	FVector AdjustedIdeal = IdealLocation;
	float GroundZ;
	if (TryFindGroundZ(IdealLocation, GroundZ, IgnoreActor))
	{
		AdjustedIdeal.Z = GroundZ;
	}

	// [2] NavMesh 투영. 이동 가능 영역으로 보정 — 가장 우선되는 보정 방법.
	// NavMesh投影が最優先の補正。
	FVector NavResult;
	if (TryProjectToNavMesh(AdjustedIdeal, NavResult))
	{
		return NavResult;
	}

	// [3] 벽 슬라이드. NavMesh 투영 실패 시 대체 — 벽에 막혔으면 벽 따라 미끄러뜨림.
	FVector SlidLocation;
	if (TryCalculateWallSlide(AnchorOrigin, AdjustedIdeal, IgnoreActor, SlidLocation))
	{
		if (TryProjectToNavMesh(SlidLocation, NavResult))
		{
			return NavResult;
		}
	}

	// [4] 전부 실패 → anchor 쪽으로 끌어당김 (최후 fallback).
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

	// [1] 법선 Z가 크면 바닥/천장이라 벽이 아님. 벽일 때만 슬라이드 진행.
	// 法線Zが大きければ床/天井なので壁ではない。
	const float WallThreshold = 0.7f;
	if (FMath::Abs(HitResult.ImpactNormal.Z) >= WallThreshold) return false;

	// [2] 남은 이동 방향을 벽면에 투영 → 벽을 따라 미끄러지는 방향 산출.
	// 残りの移動を壁面に投影し、壁に沿ってスライドさせる。
	FVector HitLoc2D = HitResult.Location; HitLoc2D.Z = 0.f;
	FVector To2D = To; To2D.Z = 0.f;
	FVector Normal2D = HitResult.ImpactNormal; Normal2D.Z = 0.f; Normal2D.Normalize();

	const FVector RemainingDir = To2D - HitLoc2D;
	const FVector SlidingDir = FVector::VectorPlaneProject(RemainingDir, Normal2D);

	// [3] 벽에서 살짝 떨어뜨려(SafeMargin) 끼임 방지.
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

TArray<APartyCharacter*> UTacticalFormationComponent::SolveSlotAssignment(
	const TArray<APartyCharacter*>& Occupants,
	const TArray<FVector>& SlotLocations) const
{
	const int32 N = Occupants.Num();

	// [1] 과도기 안전: 크기 불일치 또는 빈 occupant가 있으면 배정 스킵하고 입력 그대로 반환.
	// 過渡状態では割当をスキップ。
	if (N == 0 || N != SlotLocations.Num())
	{
		return Occupants;
	}
	for (int32 i = 0; i < N; ++i)
	{
		if (!Occupants[i]) return Occupants;
	}

	// [2] 비용 행렬 구성: 각 occupant → 각 슬롯 거리. 거리 비용은 교차 이동 최소화가 목적.
	// コスト=仲間からスロットへの距離。交差移動の最小化が狙い。
	TArray<FCostMatrixRow> CostMatrix;
	CostMatrix.Reserve(N);
	for (int32 OccupantIdx = 0; OccupantIdx < N; ++OccupantIdx)
	{
		FCostMatrixRow Row;
		Row.Values.Reserve(N);

		const FVector OccupantLoc = Occupants[OccupantIdx]->GetActorLocation();
		for (int32 SlotIdx = 0; SlotIdx < N; ++SlotIdx)
		{
			Row.Values.Add(FVector::Dist(OccupantLoc, SlotLocations[SlotIdx]));
		}
		CostMatrix.Add(Row);
	}

	// [3] 순수 헝가리안 호출 (라이브러리).
	const TArray<int32> Assignment = UHungarianMatchingLibrary::SolveAssignment(CostMatrix);
	if (Assignment.Num() != N) return Occupants;

	// [4] 배정 적용: Assignment[i]=j → occupant i가 슬롯 j로.
	TArray<APartyCharacter*> Result;
	Result.Init(nullptr, N);
	for (int32 OccupantIdx = 0; OccupantIdx < N; ++OccupantIdx)
	{
		const int32 NewSlotIdx = Assignment[OccupantIdx];
		if (Result.IsValidIndex(NewSlotIdx))
		{
			Result[NewSlotIdx] = Occupants[OccupantIdx];
		}
	}
	return Result;
}