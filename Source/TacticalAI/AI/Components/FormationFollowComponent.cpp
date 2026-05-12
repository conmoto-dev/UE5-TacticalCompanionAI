// Fill out your copyright notice in the Description page of Project Settings.

#include "AI/Components/FormationFollowComponent.h"
#include "GameFramework/Character.h"
#include "NavigationSystem.h"
#include "Engine/World.h"
#include "Kismet/KismetMathLibrary.h"
#include "DrawDebugHelpers.h"
#include "Characters/PartyCharacter.h"
#include "Party/PartyManager.h"
#include "Data/FormationDataAsset.h"
#include "Algorithms/HungarianMatchingLibrary.h"
#include "Algo/Reverse.h"
#include "EngineUtils.h"

UFormationFollowComponent::UFormationFollowComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UFormationFollowComponent::BeginPlay()
{
	Super::BeginPlay();

	// Default to WideFormation if CurrentFormation wasn't assigned in editor.
	if (!CurrentFormation && WideFormation)
	{
		ApplyFormation(WideFormation);
	}
	else if (CurrentFormation)
	{
		CachedSlotLocations.SetNum(CurrentFormation->Slots.Num());
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("FormationFollowComponent: No formation assigned."));
	}
}

void UFormationFollowComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// ===== [1] Owner + Leader validation =====
	APartyManager* Manager = Cast<APartyManager>(GetOwner());
	if (!Manager) return;

	APartyCharacter* CurrentLeader = Manager->GetLeader();
	if (!CurrentLeader) return;

	if (!CurrentFormation) return;

	// ===== [2] Formation decision (auto V/I switching) =====
	const float Width = MeasureCorridorWidth(CurrentLeader);
	if (UFormationDataAsset* DesiredFormation = SelectFormationByWidth(Width))
	{
		if (CurrentFormation != DesiredFormation)
		{
			ApplyFormation(DesiredFormation);
		}
	}

	// ===== [3] State update: gap scale + rotation =====
	UpdateGapScale(DeltaTime, CurrentLeader);
	UpdateFormationRotation(DeltaTime, CurrentLeader);

	// ===== [4] Spatial reference: leader's foot location =====
	// Foot (capsule bottom) instead of actor center to avoid Z-axis floating bugs.
	// 全演算の基準点をカプセル底にすることでZ軸浮遊バグを防ぐ。
	const float HalfHeight = CurrentLeader->GetSimpleCollisionHalfHeight();
	const FVector LeaderFootLoc = CurrentLeader->GetActorLocation() - FVector(0.f, 0.f, HalfHeight);

	// ===== [5] Slot world position cache =====
	UpdateFormationCache(LeaderFootLoc, CurrentLeader, false);

	// ===== [6] Slot assignment sync + Hungarian on stop =====
	if (SlotAssignment.Num() != CurrentFormation->Slots.Num())
	{
		SyncSlotAssignmentWithManager(Manager);
	}
	HandleStopMatching(DeltaTime, CurrentLeader);

	// ===== [7] Push slot positions to occupants =====
	for (int32 SlotIdx = 0; SlotIdx < SlotAssignment.Num() && SlotIdx < CachedSlotLocations.Num(); ++SlotIdx)
	{
		if (APartyCharacter* Occupant = SlotAssignment[SlotIdx])
		{
			Occupant->UpdateTargetSlotLocation(CachedSlotLocations[SlotIdx]);
		}
	}

	// ===== [8] Debug visualization =====
	for (int32 SlotIdx = 0; SlotIdx < SlotAssignment.Num() && SlotIdx < CachedSlotLocations.Num(); ++SlotIdx)
	{
		DrawDebugSphere(GetWorld(), CachedSlotLocations[SlotIdx], 30.0f, 16, FColor::Green, false, -1.0f, 0, 2.0f);
		DrawDebugLine(GetWorld(), LeaderFootLoc, CachedSlotLocations[SlotIdx], FColor::Yellow, false, -1.0f, 0, 1.0f);
		DrawDebugString(GetWorld(), CachedSlotLocations[SlotIdx] + FVector(0, 0, 50.f),
			FString::Printf(TEXT("Slot %d"), SlotIdx),
			nullptr, FColor::White, 0.0f, true);

		// Cyan line: occupant to its assigned slot (visualizes current matching).
		if (APartyCharacter* Occupant = SlotAssignment[SlotIdx])
		{
			DrawDebugLine(GetWorld(), Occupant->GetActorLocation(), CachedSlotLocations[SlotIdx], FColor::Cyan, false, -1.0f, 0, 1.5f);
		}
	}

	if (GEngine)
	{
		const FString Status = bMatchingAppliedOnStop ? TEXT("Matching: APPLIED") : TEXT("Matching: STANDBY");
		GEngine->AddOnScreenDebugMessage(2, 0.0f, bMatchingAppliedOnStop ? FColor::Green : FColor::White, Status);

		GEngine->AddOnScreenDebugMessage(3, 0.0f, FColor::White,
			FString::Printf(TEXT("Stop duration: %.2f / %.2f"), CurrentStopDuration, StopDurationToTrigger));
	}
}

void UFormationFollowComponent::ApplyFormation(class UFormationDataAsset* NewFormation)
{
	if (!NewFormation) return;

	CurrentFormation = NewFormation;
	CachedSlotLocations.SetNum(NewFormation->Slots.Num());
	LastCalculatedLocation = FVector(FLT_MAX);

	// Invalidate so it gets re-synced on next tick with the new slot count.
	// 新しいスロット数に合わせて次のTickで再同期。
	SlotAssignment.Empty();
}

void UFormationFollowComponent::UpdateGapScale(float DeltaTime, AActor* CurrentLeader)
{
	if (!CurrentLeader) return;

	// Size2D so vertical motion (falling) doesn't affect formation expansion.
	const float CurrentSpeed = CurrentLeader->GetVelocity().Size2D();
	const float TargetGapScale = FMath::GetMappedRangeValueClamped(LeaderSpeedRange, GapScaleRange, CurrentSpeed);

	// Spring interpolation instead of FInterpTo for natural elastic tension with inertia.
	// バネ補間: 単純Lerpではなく、慣性を持った自然な伸縮を表現。
	CurrentGapScale = UKismetMathLibrary::FloatSpringInterp(
		CurrentGapScale,
		TargetGapScale,
		GapScaleSpringVelocity,
		SpringStiffness,
		SpringDamping,
		DeltaTime,
		1.f,
		0.f
	);
}

void UFormationFollowComponent::UpdateFormationCache(const FVector& LeaderFootLoc, AActor* CurrentLeader, bool bForceUpdate)
{
	const bool bHasMovedEnough = FVector::DistSquared(LeaderFootLoc, LastCalculatedLocation) > FMath::Square(50.0f);

	// Recalculate when leader is rotating in place (no positional change).
	const bool bIsRotating = !CachedFormationRotation.Equals(LastCalculatedRotation, 0.01f);

	if (bForceUpdate || bHasMovedEnough || bIsRotating)
	{
		for (int32 i = 0; i < CurrentFormation->Slots.Num(); ++i)
		{
			FVector IdealLoc = CalculateIdealLocation(i, LeaderFootLoc);
			CachedSlotLocations[i] = AdjustLocationForEnvironment(IdealLoc, CurrentLeader, LeaderFootLoc);
		}
		LastCalculatedLocation = LeaderFootLoc;
		LastCalculatedRotation = CachedFormationRotation;
	}
}

FVector UFormationFollowComponent::CalculateIdealLocation(int32 SlotIndex, const FVector& LeaderFootLoc) const
{
	if (!CurrentFormation || !CurrentFormation->Slots.IsValidIndex(SlotIndex)) return FVector::ZeroVector;

	// Build virtual transform from smoothed CachedFormationRotation, not leader's instant rotation.
	// This gives the formation a heavy, intentional feel on sharp turns.
	// リーダーの即時回転ではなく平滑化されたCachedFormationRotationを基準にする。
	FTransform VirtualFormationTransform(CachedFormationRotation, LeaderFootLoc);
	FVector ScaledOffset = CurrentFormation->Slots[SlotIndex].LocalOffset * CurrentGapScale;
	return VirtualFormationTransform.TransformPosition(ScaledOffset);
}

FVector UFormationFollowComponent::AdjustLocationForEnvironment(const FVector& IdealLocation, const AActor* CurrentLeader, const FVector& LeaderFootLoc) const
{
	// [Step 1] Slope-aware Z correction
	// Ideal location uses leader's Z which is wrong on inclines.
	// Vertical trace at slot's X,Y finds the actual ground.
	FVector AdjustedIdeal = IdealLocation;
	float GroundZ;
	if (TryFindGroundZ(IdealLocation, GroundZ, CurrentLeader))
	{
		AdjustedIdeal.Z = GroundZ;
	}

	// [Step 2] NavMesh projection as primary truth.
	// If the corrected ideal lands on NavMesh, that's our answer.
	FVector NavResult;
	if (TryProjectToNavMesh(AdjustedIdeal, NavResult))
	{
		return NavResult;
	}

	// [Step 3] NavMesh failed. Try wall sliding as a recovery tool to find a reachable nearby spot.
	// 壁スライディングはNavMesh投影失敗時の代替座標探索ツール。
	FVector SlidLocation;
	if (TryCalculateWallSlide(LeaderFootLoc, AdjustedIdeal, CurrentLeader, SlidLocation))
	{
		if (TryProjectToNavMesh(SlidLocation, NavResult))
		{
			return NavResult;
		}
	}

	// [Step 4] All attempts failed. Tow toward leader as last resort.
	return CalculateFallbackLocation(LeaderFootLoc, IdealLocation);
}

bool UFormationFollowComponent::TryProjectToNavMesh(const FVector& Point, FVector& OutResult) const
{
	UNavigationSystemV1* NavSys = UNavigationSystemV1::GetCurrent(GetWorld());
	if (!NavSys) return false;

	FNavLocation ProjectedLoc;
	// Narrow XY to avoid snapping to adjacent NavMesh; wide Z to tolerate slope/stair gaps.
	// X,Yは狭く（誤投影防止）、Zは広く（傾斜・階段の高低差を吸収）。
	if (NavSys->ProjectPointToNavigation(Point, ProjectedLoc, FVector(50.f, 50.f, 250.f)))
	{
		OutResult = ProjectedLoc.Location;
		return true;
	}
	return false;
}

bool UFormationFollowComponent::TryFindGroundZ(const FVector& Point, float& OutZ, const AActor* IgnoreActor) const
{
	// Generous 500 each direction for steep terrain.
	const float TraceUpHeight = 500.0f;
	const float TraceDownDepth = 500.0f;

	FVector TraceStart = Point + FVector(0.f, 0.f, TraceUpHeight);
	FVector TraceEnd = Point - FVector(0.f, 0.f, TraceDownDepth);

	FHitResult Hit;
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(IgnoreActor);

	if (GetWorld()->LineTraceSingleByChannel(Hit, TraceStart, TraceEnd, ECC_Visibility, Params))
	{
		OutZ = Hit.Location.Z;
		return true;
	}
	return false;
}

bool UFormationFollowComponent::TryCalculateWallSlide(const FVector& From, const FVector& To, const AActor* IgnoreActor, FVector& OutSlidLocation) const
{
	// Sphere sweep at chest height (LineTrace's zero thickness gives false negatives in tight gaps).
	// LineTraceは厚み0で狭い隙間を誤判定するためSphere Sweepを使用。
	const float ChestHeight = 90.0f;
	FVector TraceStart = From + FVector(0.f, 0.f, ChestHeight);
	FVector TraceEnd = To + FVector(0.f, 0.f, ChestHeight);

	FHitResult HitResult(ForceInit);
	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(IgnoreActor);

	FCollisionShape SphereShape = FCollisionShape::MakeSphere(40.0f);
	bool bHit = GetWorld()->SweepSingleByChannel(HitResult, TraceStart, TraceEnd, FQuat::Identity, ECC_Visibility, SphereShape, QueryParams);

	if (!bHit) return false;

	// ImpactNormal.Z near 0 = vertical wall, near 1 = slope. Skip sliding for slopes.
	// 法線Z成分で壁と傾斜を区別。傾斜面ではスライディング不要。
	const float WallThreshold = 0.7f;
	const float NormalZ = FMath::Abs(HitResult.ImpactNormal.Z);
	if (NormalZ >= WallThreshold)
	{
		return false;
	}

	// Flatten to 2D for the sliding math to prevent vertical drift, then restore original Z.
	// 計算は完全2D平面化して地面めり込みバグを防ぐ。
	FVector HitLoc2D = HitResult.Location;
	HitLoc2D.Z = 0.f;

	FVector To2D = To;
	To2D.Z = 0.f;

	FVector Normal2D = HitResult.ImpactNormal;
	Normal2D.Z = 0.f;
	Normal2D.Normalize();

	// Project only the remaining distance (hit point to target) onto the wall plane.
	FVector RemainingDir = To2D - HitLoc2D;
	FVector SlidingDir = FVector::VectorPlaneProject(RemainingDir, Normal2D);

	const float SafeMargin = 70.0f;  // slightly larger than capsule radius (40)
	OutSlidLocation = HitLoc2D + SlidingDir + (Normal2D * SafeMargin);
	OutSlidLocation.Z = To.Z;

	return true;
}

FVector UFormationFollowComponent::CalculateFallbackLocation(const FVector& LeaderFootLoc, const FVector& IdealLocation) const
{
	// Tow toward leader instead of leaving the slot at an invalid coordinate (would break AIController->MoveTo).
	// 無効座標でMoveToが失敗するのを防ぐためリーダー方向へ引き戻し。
	const float TowDistance = 150.0f;
	FVector DirToLeader = (LeaderFootLoc - IdealLocation).GetSafeNormal2D();
	return IdealLocation + (DirToLeader * TowDistance);
}

float UFormationFollowComponent::MeasureCorridorWidth(const AActor* Leader) const
{
	UNavigationSystemV1* NavSys = UNavigationSystemV1::GetCurrent(GetWorld());
	if (!NavSys || !Leader) return FLT_MAX;

	const FVector LeaderLoc = Leader->GetActorLocation();
	const FVector RightDir = Leader->GetActorRightVector();

	FVector RightHit, LeftHit;
	const bool bRightBlocked = NavSys->NavigationRaycast(
		GetWorld(),
		LeaderLoc,
		LeaderLoc + RightDir * CorridorProbeDistance,
		RightHit
	);
	const bool bLeftBlocked = NavSys->NavigationRaycast(
		GetWorld(),
		LeaderLoc,
		LeaderLoc - RightDir * CorridorProbeDistance,
		LeftHit
	);

	const float RightDist = bRightBlocked
		? FVector::Dist(LeaderLoc, RightHit)
		: CorridorProbeDistance;
	const float LeftDist = bLeftBlocked
		? FVector::Dist(LeaderLoc, LeftHit)
		: CorridorProbeDistance;

	DrawDebugLine(GetWorld(), LeaderLoc, RightHit, FColor::Red, false, -1.0f, 0, 2.0f);
	DrawDebugLine(GetWorld(), LeaderLoc, LeftHit, FColor::Red, false, -1.0f, 0, 2.0f);

	return RightDist + LeftDist;
}

UFormationDataAsset* UFormationFollowComponent::SelectFormationByWidth(float Width) const
{
	// Hysteresis: cross opposite threshold to switch; stay between thresholds to prevent flicker.
	// 反対側の閾値を越えた時のみ切替、境界では現状維持。
	if (Width < NarrowThreshold)
	{
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(1, 0.0f, FColor::Yellow,
				FString::Printf(TEXT("Narrow Width: %.1f"), Width));
		}
		return NarrowFormation;
	}
	else if (Width > WideThreshold)
	{
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(1, 0.0f, FColor::Yellow,
				FString::Printf(TEXT("Wide Width: %.1f"), Width));
		}
		return WideFormation;
	}
	return CurrentFormation;
}

void UFormationFollowComponent::UpdateFormationRotation(float DeltaTime, AActor* CurrentLeader)
{
	if (!CurrentLeader) return;

	const FQuat TargetRotation = CurrentLeader->GetActorQuat();

	// Quaternion interp (Slerp) instead of FRotator to avoid -180/+180 reverse-rotation bug.
	// FRotator(オイラー角)補間は-180/180境界で逆回転バグが起きるためQuaternionで補間。
	CachedFormationRotation = FMath::QInterpTo(CachedFormationRotation, TargetRotation, DeltaTime, RotationInterpSpeed);
}

void UFormationFollowComponent::SyncSlotAssignmentWithManager(APartyManager* Manager)
{
	if (!Manager) return;
	if (!CurrentFormation) return;

	TArray<APartyCharacter*> Followers = Manager->GetFollowers();
	const int32 SlotCount = CurrentFormation->Slots.Num();

	SlotAssignment.Empty();
	SlotAssignment.Reserve(SlotCount);

	// Initial assignment uses Manager's natural order; Hungarian matching reorders later.
	// 初期割り当てはManagerの並びをそのまま反映。後でハンガリアンで最適化。
	for (int32 i = 0; i < SlotCount; ++i)
	{
		if (i < Followers.Num())
		{
			SlotAssignment.Add(Followers[i]);
		}
		else
		{
			SlotAssignment.Add(nullptr);
		}
	}
}

void UFormationFollowComponent::ApplyHungarianMatching()
{
	const int32 N = SlotAssignment.Num();
	if (N == 0 || N != CachedSlotLocations.Num()) return;

	// Skip if any slot is empty (mid-sync transitional state).
	for (int32 i = 0; i < N; ++i)
	{
		if (!SlotAssignment[i]) return;
	}

	// [Step 1] Build cost matrix: distance from each occupant to each slot.
	TArray<FCostMatrixRow> CostMatrix;
	CostMatrix.Reserve(N);
	for (int32 OccupantIdx = 0; OccupantIdx < N; ++OccupantIdx)
	{
		FCostMatrixRow Row;
		Row.Values.Reserve(N);

		const FVector OccupantLoc = SlotAssignment[OccupantIdx]->GetActorLocation();
		for (int32 SlotIdx = 0; SlotIdx < N; ++SlotIdx)
		{
			const float Distance = FVector::Dist(OccupantLoc, CachedSlotLocations[SlotIdx]);
			Row.Values.Add(Distance);
		}
		CostMatrix.Add(Row);
	}

	// [Step 2] Solve.
	TArray<int32> Assignment = UHungarianMatchingLibrary::SolveAssignment(CostMatrix);
	if (Assignment.Num() != N) return;

	// [Step 3] Apply: Assignment[i]=j means occupant i moves to slot j.
	TArray<TObjectPtr<APartyCharacter>> NewAssignment;
	NewAssignment.Init(nullptr, N);
	for (int32 OccupantIdx = 0; OccupantIdx < N; ++OccupantIdx)
	{
		const int32 NewSlotIdx = Assignment[OccupantIdx];
		if (NewAssignment.IsValidIndex(NewSlotIdx))
		{
			NewAssignment[NewSlotIdx] = SlotAssignment[OccupantIdx];
		}
	}
	SlotAssignment = NewAssignment;
}

void UFormationFollowComponent::HandleStopMatching(float DeltaTime, AActor* CurrentLeader)
{
	if (!CurrentLeader) return;

	const float Speed = CurrentLeader->GetVelocity().Size2D();
	const bool bIsStopped = (Speed < StopSpeedThreshold);

	if (bIsStopped)
	{
		CurrentStopDuration += DeltaTime;

		if (CurrentStopDuration > StopDurationToTrigger && !bMatchingAppliedOnStop)
		{
			ApplyHungarianMatching();
			bMatchingAppliedOnStop = true;
		}
	}
	else
	{
		CurrentStopDuration = 0.f;
		bMatchingAppliedOnStop = false;
	}
}

void UFormationFollowComponent::DebugShuffleSlotAssignment()
{
	if (SlotAssignment.Num() < 2) return;

	// Fisher-Yates shuffle for visible matching effect on test command.
	for (int32 i = SlotAssignment.Num() - 1; i > 0; --i)
	{
		const int32 j = FMath::RandRange(0, i);
		SlotAssignment.Swap(i, j);
	}

	UE_LOG(LogTemp, Log, TEXT("[Formation] SlotAssignment randomly shuffled. Next stop will trigger matching."));
}

// =========================================================================
// Console commands (debug only)
// =========================================================================

static FAutoConsoleCommandWithWorld GShuffleSlotsCommand(
	TEXT("formation.ShuffleSlots"),
	TEXT("Randomly shuffle current SlotAssignment so the next match has visible effect."),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
	{
		if (!World) return;

		for (TActorIterator<APartyManager> It(World); It; ++It)
		{
			if (APartyManager* Manager = *It)
			{
				if (UFormationFollowComponent* Comp = Manager->FindComponentByClass<UFormationFollowComponent>())
				{
					Comp->DebugShuffleSlotAssignment();
				}
			}
		}
	})
);