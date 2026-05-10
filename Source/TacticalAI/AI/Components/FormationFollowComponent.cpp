// Fill out your copyright notice in the Description page of Project Settings.

#include "AI/Components/FormationFollowComponent.h"
#include "GameFramework/Character.h"
#include "NavigationSystem.h"
#include "Engine/World.h"
#include "Kismet/KismetMathLibrary.h"
#include "DrawDebugHelpers.h"
#include "Characters/PartyCharacter.h"
#include "Party/PartyManager.h"


UFormationFollowComponent::UFormationFollowComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UFormationFollowComponent::BeginPlay()
{
	Super::BeginPlay();
	
	// ---------------------------------------------------------
	// [Initial Slot Offsets]
	// Defines local-space offsets for each formation slot.
	// To be replaced with UDataAsset later so designers can edit at runtime.
	// 将来的にはUDataAssetへ分離し、プランナーが調整可能にする。
	// ---------------------------------------------------------
	if (SlotOffsets.IsEmpty())
	{
		float FormationGap = 200.f; // Lateral gap between followers
		SlotOffsets.Add(FVector(-100.f, -FormationGap, 0.f));    // Slot 0: leader's rear-left
		SlotOffsets.Add(FVector(-100.f,  FormationGap, 0.f));    // Slot 1: leader's rear-right
		SlotOffsets.Add(FVector(-FormationGap * 1.5f, 0.f, 0.f)); // Slot 2: leader's far-rear center
	}
	CachedSlotLocations.SetNum(SlotOffsets.Num());
}

void UFormationFollowComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	
	// GetOwner() == APartyManager (this component is attached to the manager actor).
	APartyManager* Manager = Cast<APartyManager>(GetOwner());
	if (!Manager) return;

	APartyCharacter* CurrentLeader = Manager->GetLeader();
	if (!CurrentLeader) return;

	UpdateGapScale(DeltaTime, CurrentLeader);
	UpdateFormationRotation(DeltaTime, CurrentLeader);
	
	// ---------------------------------------------------------
	// Use foot location (capsule bottom) as the reference for all spatial math
	// to avoid Z-axis floating bugs caused by using actor center.
	// 全演算の基準点をカプセル底（足元）にすることでZ軸浮遊バグを防ぐ。
	// ---------------------------------------------------------
	float HalfHeight = CurrentLeader->GetSimpleCollisionHalfHeight();
	FVector LeaderFootLoc = CurrentLeader->GetActorLocation() - FVector(0.f, 0.f, HalfHeight);
	
	UpdateFormationCache(LeaderFootLoc, CurrentLeader, false);
	
	// Push computed slot positions to each follower.
	// 計算済みスロット座標を各仲間に伝達。
	TArray<APartyCharacter*> Followers = Manager->GetFollowers();
	for (int32 i = 0; i < Followers.Num() && i < CachedSlotLocations.Num(); ++i)
	{
		if (Followers[i])
		{
			Followers[i]->UpdateTargetSlotLocation(CachedSlotLocations[i]);
		}
	}
	
	// ---------------------------------------------------------
	// [Debug] Draw cached slot positions every frame (no compute cost).
	// ---------------------------------------------------------
	for (int32 i = 0; i < SlotOffsets.Num(); ++i)
	{
		DrawDebugSphere(GetWorld(), CachedSlotLocations[i], 30.0f, 16, FColor::Green, false, -1.0f, 0, 2.0f);
		DrawDebugLine(GetWorld(), LeaderFootLoc, CachedSlotLocations[i], FColor::Yellow, false, -1.0f, 0, 1.0f);
	}
}

void UFormationFollowComponent::UpdateGapScale(float DeltaTime, AActor* CurrentLeader)
{
	if (!CurrentLeader) return;

	// 1. Get the leader's 2D planar speed.
	// Use Size2D so vertical motion (falling, etc.) doesn't affect formation expansion.
	const float CurrentSpeed = CurrentLeader->GetVelocity().Size2D();

	// 2. Map current speed (0~500) to target gap scale (1.0~1.5) with linear interpolation and clamping.
	const float TargetGapScale = FMath::GetMappedRangeValueClamped(LeaderSpeedRange, GapScaleRange, CurrentSpeed);

	// 3. Spring-based interpolation for natural elastic tension.
	// Frame-rate independent and physically grounded, unlike simple Lerp.
	// FInterpTo --> FloatSpringInterp:
	// GapScaleSpringVelocity is passed by reference so the engine accumulates spring momentum across ticks.
	// バネ補間: 単純Lerpではなく、慣性を持った自然な伸縮を表現。
	CurrentGapScale = UKismetMathLibrary::FloatSpringInterp(
		CurrentGapScale,           // current scale
		TargetGapScale,            // target scale
		GapScaleSpringVelocity,    // [In/Out] spring velocity state (engine updates this directly)
		SpringStiffness,           // spring stiffness
		SpringDamping,             // critical damping (1.0)
		DeltaTime,                 
		1.f,                       // Mass (default 1)
		0.f                        // Target Velocity (target itself isn't moving, so 0)
	);
}

void UFormationFollowComponent::UpdateFormationCache(const FVector& LeaderFootLoc, AActor* CurrentLeader, bool bForceUpdate)
{
	const bool bHasMovedEnough = FVector::DistSquared(LeaderFootLoc, LastCalculatedLocation) > FMath::Square(50.0f);
	
	// Also recalculate when only rotation changed (e.g., leader spinning in place).
	const bool bIsRotating = !CachedFormationRotation.Equals(LastCalculatedRotation, 0.01f);
	
	if (bForceUpdate || bHasMovedEnough || bIsRotating)
	{
		for (int32 i = 0; i < SlotOffsets.Num(); ++i)
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
	if (!SlotOffsets.IsValidIndex(SlotIndex)) return FVector::ZeroVector;

	// [Architecture note]
	// We do NOT copy the leader's transform directly (which would jerk on sharp turns).
	// Instead we build a virtual transform from the smoothed CachedFormationRotation
	// and the leader's foot position. This gives the formation a heavy, intentional feel.
	// リーダーの即時回転ではなく、平滑化されたCachedFormationRotationを基準にする。
	FTransform VirtualFormationTransform(CachedFormationRotation, LeaderFootLoc);

	// Apply tension scale to the local offset.
	FVector ScaledOffset = SlotOffsets[SlotIndex] * CurrentGapScale;

	// Convert local offset to world coordinates via the virtual transform.
	return VirtualFormationTransform.TransformPosition(ScaledOffset);
}

FVector UFormationFollowComponent::AdjustLocationForEnvironment(const FVector& IdealLocation,const AActor* CurrentLeader, const FVector& LeaderFootLoc) const
{
	FVector FinalTargetLocation = IdealLocation;

	FHitResult HitResult(ForceInit); 
	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(CurrentLeader);

	const float ChestHeight = 90.0f;
	FVector TraceStart = LeaderFootLoc + FVector(0.f, 0.f, ChestHeight);
	FVector TraceEnd = IdealLocation + FVector(0.f, 0.f, ChestHeight);
	
	// Sphere Sweep instead of Line Trace: matches character capsule radius (40)
	// to avoid false negatives where a thin line passes through a gap a capsule can't fit.
	// LineTraceは厚み0で狭い隙間を誤判定するため、Sphere Sweepでカプセル半径と同等の判定を行う。
	FCollisionShape SphereShape = FCollisionShape::MakeSphere(40.0f);
	bool bHitWall = GetWorld()->SweepSingleByChannel(HitResult, TraceStart, TraceEnd, FQuat::Identity, ECC_Visibility, SphereShape, QueryParams);

	if (bHitWall)
	{
		// Flatten everything to 2D to prevent the slot from sinking into the ground
		// when there's a height difference between hit point and ideal location.
		// 高低差で地面にめり込むバグを防ぐため、完全2D平面化して計算。
		FVector HitLoc2D = HitResult.Location;
		HitLoc2D.Z = 0.f;

		FVector IdealLoc2D = IdealLocation;
		IdealLoc2D.Z = 0.f;

		FVector Normal2D = HitResult.ImpactNormal;
		Normal2D.Z = 0.f;
		Normal2D.Normalize();

		// Project only the REMAINING distance from hit point to target onto the wall plane.
		// 衝突点から目標までの「残り距離」のみを壁面に投影。
		FVector RemainingDir = IdealLoc2D - HitLoc2D;
		FVector SlidingDir = FVector::VectorPlaneProject(RemainingDir, Normal2D);

		const float SafeMargin = 70.0f; // slightly larger than capsule radius (40)
		FinalTargetLocation = HitLoc2D + SlidingDir + (Normal2D * SafeMargin);
		FinalTargetLocation.Z = IdealLocation.Z; // restore original foot Z
	}

	// NavMesh projection: snap the slot to walkable navigation surface.
	// Acts as the final safety net - if the slot isn't on NavMesh, we tow it back toward the leader.
	UNavigationSystemV1* NavSys = UNavigationSystemV1::GetCurrent(GetWorld());
	if (NavSys)
	{
		FNavLocation ProjectedNavLoc;
		if (NavSys->ProjectPointToNavigation(FinalTargetLocation, ProjectedNavLoc, FVector(50.f, 50.f, 250.f)))
		{
			FinalTargetLocation = ProjectedNavLoc.Location; 
		}
		else if (bHitWall)
		{
			// Slid into an unreachable area. Tow back toward the leader as fallback.
			// NavMesh外に滑り出た場合、リーダー方向へ強制的に引き戻す。
			FVector DirToLeader = (LeaderFootLoc - FinalTargetLocation).GetSafeNormal2D();
			FinalTargetLocation += DirToLeader * 150.f;
		}
	}

	return FinalTargetLocation;
}

void UFormationFollowComponent::UpdateFormationRotation(float DeltaTime, AActor* CurrentLeader)
{
	if (!CurrentLeader) return;

	// Leader's actual current rotation as a quaternion.
	const FQuat TargetRotation = CurrentLeader->GetActorQuat();

	// Spherical linear interpolation (Slerp) via QInterpTo to avoid gimbal lock and the
	// -180 -> 180 reverse-rotation bug that occurs with Euler-based RInterpTo.
	// FRotator(オイラー角)補間は-180/180境界で逆回転バグが起きるため、Quaternionで補間する。
	CachedFormationRotation = FMath::QInterpTo(CachedFormationRotation, TargetRotation, DeltaTime, RotationInterpSpeed);
}