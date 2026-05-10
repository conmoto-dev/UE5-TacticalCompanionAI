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


UFormationFollowComponent::UFormationFollowComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UFormationFollowComponent::BeginPlay()
{
	Super::BeginPlay();
	
	// Initialize cache size based on assigned formation.
	// 割り当てられた隊形のスロット数に応じてキャッシュサイズを初期化。
	if (CurrentFormation)
	{
		CachedSlotLocations.SetNum(CurrentFormation->Slots.Num());
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("FormationFollowComponent: CurrentFormation is not assigned."));
	}
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
	if (CurrentFormation)
	{
		for (int32 i = 0; i < CurrentFormation->Slots.Num(); ++i)
		{
			DrawDebugSphere(GetWorld(), CachedSlotLocations[i], 30.0f, 16, FColor::Green, false, -1.0f, 0, 2.0f);
			DrawDebugLine(GetWorld(), LeaderFootLoc, CachedSlotLocations[i], FColor::Yellow, false, -1.0f, 0, 1.0f);
		}
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
	if (!CurrentFormation) return;
	
	const bool bHasMovedEnough = FVector::DistSquared(LeaderFootLoc, LastCalculatedLocation) > FMath::Square(50.0f);
	
	// Also recalculate when only rotation changed (e.g., leader spinning in place).
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

	// [Architecture note]
	// We do NOT copy the leader's transform directly (which would jerk on sharp turns).
	// Instead we build a virtual transform from the smoothed CachedFormationRotation
	// and the leader's foot position. This gives the formation a heavy, intentional feel.
	// リーダーの即時回転ではなく、平滑化されたCachedFormationRotationを基準にする。
	FTransform VirtualFormationTransform(CachedFormationRotation, LeaderFootLoc);
	// Apply tension scale to the local offset.
	FVector ScaledOffset = CurrentFormation->Slots[SlotIndex].LocalOffset * CurrentGapScale;
	// Convert local offset to world coordinates via the virtual transform.
	return VirtualFormationTransform.TransformPosition(ScaledOffset);
}

FVector UFormationFollowComponent::AdjustLocationForEnvironment(const FVector& IdealLocation, const AActor* CurrentLeader, const FVector& LeaderFootLoc) const
{
	// [Step 1] Slope-aware Z correction
	// The ideal location uses the leader's Z, which is wrong on inclines.
	// Trace vertically at the slot's X,Y to find the actual ground height.
	// 傾斜面ではリーダーZをそのまま使うと床にめり込んだり浮いたりするため、
	// スロットのX,Y位置で垂直トレースして実際の地面Zを取得する。
	FVector AdjustedIdeal = IdealLocation;
	float GroundZ;
	if (TryFindGroundZ(IdealLocation, GroundZ, CurrentLeader))
	{
		AdjustedIdeal.Z = GroundZ;
	}

	// [Step 2] NavMesh projection as primary truth source
	// If the corrected ideal lands on NavMesh, that's our answer. Cleanest path.
	// NavMesh上に乗っていれば、それが最も信頼できる答え。即返却。
	FVector NavResult;
	if (TryProjectToNavMesh(AdjustedIdeal, NavResult))
	{
		return NavResult;
	}

	// [Step 3] NavMesh failed. Try wall sliding to find an alternative valid position.
	// Wall sliding is now a *recovery tool* for when the ideal slot is unreachable,
	// not the primary path-correction mechanism.
	// 壁スライディングは「主な経路補正」ではなく「NavMesh投影失敗時の代替座標探索ツール」として再定義。
	FVector SlidLocation;
	if (TryCalculateWallSlide(LeaderFootLoc, AdjustedIdeal, CurrentLeader, SlidLocation))
	{
		if (TryProjectToNavMesh(SlidLocation, NavResult))
		{
			return NavResult;
		}
	}

	// [Step 4] All attempts failed. Tow toward leader as the last resort.
	// 全ての試みが失敗。リーダー方向へ強制引き戻し。
	return CalculateFallbackLocation(LeaderFootLoc, IdealLocation);
}

bool UFormationFollowComponent::TryProjectToNavMesh(const FVector& Point, FVector& OutResult) const
{
    UNavigationSystemV1* NavSys = UNavigationSystemV1::GetCurrent(GetWorld());
    if (!NavSys) return false;

    FNavLocation ProjectedLoc;
    // Extent: narrow on X,Y to avoid snapping to unintended adjacent NavMesh,
    // wide on Z to tolerate vertical mismatch from slope/stair scenarios.
    // X,Yは狭く（隣接エリアへの誤投影防止）、Zは広く（傾斜・階段の高低差を吸収）。
    if (NavSys->ProjectPointToNavigation(Point, ProjectedLoc, FVector(50.f, 50.f, 250.f)))
    {
        OutResult = ProjectedLoc.Location;
        return true;
    }
    return false;
}

bool UFormationFollowComponent::TryFindGroundZ(const FVector& Point, float& OutZ, const AActor* IgnoreActor) const
{
    // Trace from well above the point straight down to find the actual ground.
    // Trace range is generous (2000 each direction) to handle steep terrain and tall environments.
    // 高低差の大きい地形にも対応するため、十分な範囲（上下2000）でトレース。
    const float TraceUpHeight = 2000.0f;
    const float TraceDownDepth = 2000.0f;

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
    // Sphere Sweep at chest height: matches character capsule footprint
    // (Line Trace's zero thickness produces false negatives in narrow gaps).
    // LineTraceは厚み0で狭い隙間を誤判定するため、Sphere Sweepでカプセル相当の判定を行う。
    const float ChestHeight = 90.0f;
    FVector TraceStart = From + FVector(0.f, 0.f, ChestHeight);
    FVector TraceEnd = To + FVector(0.f, 0.f, ChestHeight);

    FHitResult HitResult(ForceInit);
    FCollisionQueryParams QueryParams;
    QueryParams.AddIgnoredActor(IgnoreActor);

    FCollisionShape SphereShape = FCollisionShape::MakeSphere(40.0f);
    bool bHit = GetWorld()->SweepSingleByChannel(HitResult, TraceStart, TraceEnd, FQuat::Identity, ECC_Visibility, SphereShape, QueryParams);

    if (!bHit) return false;

    // Distinguish vertical wall from steep slope using normal's Z component.
    // ImpactNormal.Z near 0 = vertical wall, near 1 = mostly-horizontal floor/slope.
    // Skip sliding for slopes — they're not obstacles, just terrain.
    // 法線のZ成分で「壁」と「傾斜」を区別。傾斜面はそもそも障害物ではないのでスライディング対象外。
    const float WallThreshold = 0.7f;
    const float NormalZ = FMath::Abs(HitResult.ImpactNormal.Z);
    if (NormalZ >= WallThreshold)
    {
        return false;
    }

    // Compute sliding position using vector projection onto the wall plane.
    // Flatten to 2D to prevent vertical drift, then restore the original Z.
    // 高低差で地面にめり込むバグを防ぐため、計算は完全2D平面化して行う。
    FVector HitLoc2D = HitResult.Location;
    HitLoc2D.Z = 0.f;

    FVector To2D = To;
    To2D.Z = 0.f;

    FVector Normal2D = HitResult.ImpactNormal;
    Normal2D.Z = 0.f;
    Normal2D.Normalize();

    // Project only the REMAINING distance from hit point to target onto the wall plane.
    // 衝突点から目標までの「残り距離」のみを壁面に投影。
    FVector RemainingDir = To2D - HitLoc2D;
    FVector SlidingDir = FVector::VectorPlaneProject(RemainingDir, Normal2D);

    const float SafeMargin = 70.0f;  // slightly larger than capsule radius (40)
    OutSlidLocation = HitLoc2D + SlidingDir + (Normal2D * SafeMargin);
    OutSlidLocation.Z = To.Z;  // restore original Z

    return true;
}

FVector UFormationFollowComponent::CalculateFallbackLocation(const FVector& LeaderFootLoc, const FVector& IdealLocation) const
{
    // Companions are kept inside a known-safe radius rather than left at an invalid coordinate,
    // which would cause AIController->MoveTo to fail or produce visible glitches.
    // 仲間が無効な座標に取り残されAIControllerのMoveToが失敗するのを防ぐため、リーダー方向へ強制引き戻し。
    const float TowDistance = 150.0f;
    FVector DirToLeader = (LeaderFootLoc - IdealLocation).GetSafeNormal2D();
    return IdealLocation + (DirToLeader * TowDistance);
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