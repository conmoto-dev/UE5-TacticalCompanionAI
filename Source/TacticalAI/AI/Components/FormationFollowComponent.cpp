// Fill out your copyright notice in the Description page of Project Settings.

#include "AI/Components/FormationFollowComponent.h"
#include "GameFramework/Character.h"
#include "NavigationSystem.h"
#include "Engine/World.h"
#include "Kismet/KismetMathLibrary.h"
#include "Kismet/GameplayStatics.h"
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
	// エディタ未割当時はWideFormationにフォールバック。
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
	// カプセル底を基準にしてZ軸浮遊を防ぐ。
	const float HalfHeight = CurrentLeader->GetSimpleCollisionHalfHeight();
	const FVector LeaderFootLoc = CurrentLeader->GetActorLocation() - FVector(0.f, 0.f, HalfHeight);

	// ===== [5] Slot world position cache =====
	// Revisit during week 3 Detour Crowd integration.
	// bForceUpdate=trueは意図的か要確認。元はfalse（距離ベースキャッシュ）。
	UpdateFormationCache(LeaderFootLoc, CurrentLeader, false);

	// ===== [6] Slot assignment sync + Hungarian on stop =====
	if (SlotAssignment.Num() != CurrentFormation->Slots.Num())
	{
		SyncSlotAssignmentWithManager(Manager);
	}
	HandleStopMatching(DeltaTime, CurrentLeader);

	// ===== [6.5] Yield state evaluation (per-slot) =====
	UpdateYieldStates(DeltaTime);

	// ===== [7] Push positions to occupants (slot OR yield location based on state) =====
	// Yielding slots get yield coordinate, others get slot coordinate.
	// Yielding中はYield座標、それ以外はスロット座標をpush。
	for (int32 SlotIdx = 0; SlotIdx < SlotAssignment.Num() && SlotIdx < CachedSlotLocations.Num(); ++SlotIdx)
	{
		APartyCharacter* Occupant = SlotAssignment[SlotIdx];
		if (!Occupant) continue;

		const bool bIsYielding = SlotYieldStates.IsValidIndex(SlotIdx)
			&& SlotYieldStates[SlotIdx] == ESlotYieldState::Yielding;

		const FVector TargetLoc = bIsYielding ? CachedYieldLocations[SlotIdx] : CachedSlotLocations[SlotIdx];

		// bForceRefresh=true during Yielding bypasses UpdateThreshold caching in PartyCharacter.
		// Yielding時はforce push: UpdateThresholdキャッシュをバイパスしMoveTo再発行を保証。
		Occupant->UpdateTargetSlotLocation(TargetLoc, bIsYielding);
	}

	// ===== [8] Debug visualization =====
	for (int32 SlotIdx = 0; SlotIdx < SlotAssignment.Num() && SlotIdx < CachedSlotLocations.Num(); ++SlotIdx)
	{
		// Slot sphere: magenta if Yielding, green otherwise.
		// Yielding中はマゼンタ、Following中はグリーン。
		const bool bIsYielding = SlotYieldStates.IsValidIndex(SlotIdx)
			&& SlotYieldStates[SlotIdx] == ESlotYieldState::Yielding;
		const FColor SlotColor = bIsYielding ? FColor::Magenta : FColor::Green;

		DrawDebugSphere(GetWorld(), CachedSlotLocations[SlotIdx], 30.0f, 16, SlotColor, false, -1.0f, 0, 2.0f);
		DrawDebugLine(GetWorld(), LeaderFootLoc, CachedSlotLocations[SlotIdx], FColor::Yellow, false, -1.0f, 0, 1.0f);
		DrawDebugString(GetWorld(), CachedSlotLocations[SlotIdx] + FVector(0, 0, 50.f),
			FString::Printf(TEXT("Slot %d"), SlotIdx),
			nullptr, FColor::White, 0.0f, true);

		if (APartyCharacter* Occupant = SlotAssignment[SlotIdx])
		{
			// Cyan line: occupant → assigned slot (visualizes current matching).
			// 現在のマッチング可視化。
			DrawDebugLine(GetWorld(), Occupant->GetActorLocation(), CachedSlotLocations[SlotIdx], FColor::Cyan, false, -1.0f, 0, 1.5f);

			// Magenta arrow: occupant → yield target (only when Yielding).
			// Yielding中のみYield目標を矢印で表示。
			if (bIsYielding && CachedYieldLocations.IsValidIndex(SlotIdx))
			{
				DrawDebugDirectionalArrow(GetWorld(),
					Occupant->GetActorLocation(),
					CachedYieldLocations[SlotIdx],
					50.f, FColor::Magenta, false, -1.0f, 0, 3.0f);
			}
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
	// 新スロット数に合わせて次Tickで再同期。
	SlotAssignment.Empty();
	SlotYieldStates.Empty();
	CachedYieldLocations.Empty();
	SlotYieldDelayTimers.Empty();
}

void UFormationFollowComponent::UpdateGapScale(float DeltaTime, AActor* CurrentLeader)
{
	if (!CurrentLeader) return;

	// Size2D so vertical motion (falling) doesn't affect formation expansion.
	// 落下などの垂直運動が隊形伸縮に影響しないようSize2Dを使用。
	const float CurrentSpeed = CurrentLeader->GetVelocity().Size2D();
	const float TargetGapScale = FMath::GetMappedRangeValueClamped(LeaderSpeedRange, GapScaleRange, CurrentSpeed);

	// Spring interpolation for natural elastic tension with inertia (not simple lerp).
	// バネ補間で慣性のある自然な伸縮を表現。
	CurrentGapScale = UKismetMathLibrary::FloatSpringInterp(
		CurrentGapScale, TargetGapScale, GapScaleSpringVelocity,
		SpringStiffness, SpringDamping, DeltaTime, 1.f, 0.f
	);
}

void UFormationFollowComponent::UpdateFormationCache(const FVector& LeaderFootLoc, AActor* CurrentLeader, bool bForceUpdate)
{
	check(CurrentFormation);

	// Skip recalculation when leader is stable (distance + rotation threshold caching).
	// リーダーが安定中は再計算スキップ（距離+回転しきい値キャッシュ）。
	const bool bHasMovedEnough = FVector::DistSquared(LeaderFootLoc, LastCalculatedLocation) > FMath::Square(50.0f);
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

	// Use smoothed CachedFormationRotation (not leader's instant rotation) to give the formation
	// a heavy, intentional feel on sharp turns.
	// リーダー即時回転ではなく平滑化された回転を基準とし、急旋回時の「重み」を演出。
	const FTransform VirtualFormationTransform(CachedFormationRotation, LeaderFootLoc);
	const FVector ScaledOffset = CurrentFormation->Slots[SlotIndex].LocalOffset * CurrentGapScale;
	return VirtualFormationTransform.TransformPosition(ScaledOffset);
}

FVector UFormationFollowComponent::AdjustLocationForEnvironment(const FVector& IdealLocation, const AActor* CurrentLeader, const FVector& LeaderFootLoc) const
{
	// 4-step pipeline: ground Z → NavMesh project → wall slide → fallback.
	// NavMesh is the primary truth; sliding is a recovery tool for failed projections.
	// 4段階補正パイプライン。NavMesh投影が主、壁スライディングは投影失敗時の代替探索。

	// [1] Slope-aware Z correction (vertical trace finds actual ground).
	// 傾斜面のZ補正：垂直トレースで実地面を取得。
	FVector AdjustedIdeal = IdealLocation;
	float GroundZ;
	if (TryFindGroundZ(IdealLocation, GroundZ, CurrentLeader))
	{
		AdjustedIdeal.Z = GroundZ;
	}

	// [2] NavMesh projection as primary truth.
	// NavMesh投影が主。
	FVector NavResult;
	if (TryProjectToNavMesh(AdjustedIdeal, NavResult))
	{
		return NavResult;
	}

	// [3] NavMesh failed → try wall sliding as alternate coordinate search.
	// NavMesh失敗時の代替座標探索。
	FVector SlidLocation;
	if (TryCalculateWallSlide(LeaderFootLoc, AdjustedIdeal, CurrentLeader, SlidLocation))
	{
		if (TryProjectToNavMesh(SlidLocation, NavResult))
		{
			return NavResult;
		}
	}

	// [4] All failed → tow toward leader (last resort).
	// 全失敗時はリーダー方向へ引き戻し。
	return CalculateFallbackLocation(LeaderFootLoc, IdealLocation);
}

bool UFormationFollowComponent::TryProjectToNavMesh(const FVector& Point, FVector& OutResult) const
{
	UNavigationSystemV1* NavSys = UNavigationSystemV1::GetCurrent(GetWorld());
	if (!NavSys) return false;

	// Narrow XY to avoid snapping to adjacent NavMesh; wide Z to tolerate slope/stair gaps.
	// XYは狭く（誤投影防止）、Zは広く（傾斜・階段の高低差吸収）。
	FNavLocation ProjectedLoc;
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
	// 急傾斜地形対応のため上下500ずつ。
	const float TraceUpHeight = 500.0f;
	const float TraceDownDepth = 500.0f;

	const FVector TraceStart = Point + FVector(0.f, 0.f, TraceUpHeight);
	const FVector TraceEnd   = Point - FVector(0.f, 0.f, TraceDownDepth);

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
	// Sphere sweep at chest height; LineTrace's zero thickness gives false negatives in tight gaps.
	// 厚み0のLineTraceは狭い隙間を誤判定するためSphere Sweep使用。
	const float ChestHeight = 90.0f;
	const FVector TraceStart = From + FVector(0.f, 0.f, ChestHeight);
	const FVector TraceEnd   = To   + FVector(0.f, 0.f, ChestHeight);

	FHitResult HitResult(ForceInit);
	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(IgnoreActor);

	const FCollisionShape SphereShape = FCollisionShape::MakeSphere(40.0f);
	const bool bHit = GetWorld()->SweepSingleByChannel(HitResult, TraceStart, TraceEnd, FQuat::Identity, ECC_Visibility, SphereShape, QueryParams);

	if (!bHit) return false;

	// ImpactNormal.Z near 0 = vertical wall, near 1 = slope. Sliding only applies to walls.
	// 法線Z成分で壁/傾斜を区別。傾斜面ではスライディング不適用。
	const float WallThreshold = 0.7f;
	if (FMath::Abs(HitResult.ImpactNormal.Z) >= WallThreshold)
	{
		return false;
	}

	// Flatten to 2D for sliding math (prevents vertical drift), restore original Z afterward.
	// 計算は2D平面化（めり込み防止）、Zは最後に復元。
	FVector HitLoc2D = HitResult.Location;
	HitLoc2D.Z = 0.f;

	FVector To2D = To;
	To2D.Z = 0.f;

	FVector Normal2D = HitResult.ImpactNormal;
	Normal2D.Z = 0.f;
	Normal2D.Normalize();

	// Project the *remaining* distance (hit point to target) onto the wall plane.
	// ヒット地点から目標までの残距離を壁平面に射影。
	const FVector RemainingDir = To2D - HitLoc2D;
	const FVector SlidingDir = FVector::VectorPlaneProject(RemainingDir, Normal2D);

	const float SafeMargin = 70.0f;  // slightly larger than capsule radius (40)
	OutSlidLocation = HitLoc2D + SlidingDir + (Normal2D * SafeMargin);
	OutSlidLocation.Z = To.Z;

	return true;
}

FVector UFormationFollowComponent::CalculateFallbackLocation(const FVector& LeaderFootLoc, const FVector& IdealLocation) const
{
	// Tow toward leader to avoid invalid coordinates (would break AIController->MoveTo).
	// 無効座標でMoveToが失敗するのを防ぐためリーダー方向へ引き戻し。
	const float TowDistance = 150.0f;
	const FVector DirToLeader = (LeaderFootLoc - IdealLocation).GetSafeNormal2D();
	return IdealLocation + (DirToLeader * TowDistance);
}

float UFormationFollowComponent::MeasureCorridorWidth(const AActor* Leader) const
{
	UNavigationSystemV1* NavSys = UNavigationSystemV1::GetCurrent(GetWorld());
	if (!NavSys || !Leader) return FLT_MAX;

	const FVector LeaderLoc = Leader->GetActorLocation();
	const FVector RightDir = Leader->GetActorRightVector();

	// Raycast left/right perpendicular to leader's facing; sum of distances = corridor width.
	// Known limitation: measurement is leader-position based; edge cases may misfire.
	// リーダー正面に垂直な左右レイキャスト距離の合計で通路幅を測定。
	// 既知の限界: リーダー位置基準のため特定の角ケースで誤発動の可能性。
	FVector RightHit, LeftHit;
	const bool bRightBlocked = NavSys->NavigationRaycast(GetWorld(), LeaderLoc, LeaderLoc + RightDir * CorridorProbeDistance, RightHit);
	const bool bLeftBlocked  = NavSys->NavigationRaycast(GetWorld(), LeaderLoc, LeaderLoc - RightDir * CorridorProbeDistance, LeftHit);

	const float RightDist = bRightBlocked ? FVector::Dist(LeaderLoc, RightHit) : CorridorProbeDistance;
	const float LeftDist  = bLeftBlocked  ? FVector::Dist(LeaderLoc, LeftHit)  : CorridorProbeDistance;

	DrawDebugLine(GetWorld(), LeaderLoc, RightHit, FColor::Red, false, -1.0f, 0, 2.0f);
	DrawDebugLine(GetWorld(), LeaderLoc, LeftHit, FColor::Red, false, -1.0f, 0, 2.0f);

	return RightDist + LeftDist;
}

UFormationDataAsset* UFormationFollowComponent::SelectFormationByWidth(float Width) const
{
	// Hysteresis: cross opposite threshold to switch; stay between thresholds to prevent flicker.
	// 反対側のしきい値を越えた時のみ切替、境界では現状維持。
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

	// Quaternion interp (Slerp) to avoid -180/+180 reverse-rotation bug of FRotator (Euler).
	// FRotator(オイラー角)補間は-180/180境界で逆回転バグが起きるためQuaternionで補間。
	const FQuat TargetRotation = CurrentLeader->GetActorQuat();
	CachedFormationRotation = FMath::QInterpTo(CachedFormationRotation, TargetRotation, DeltaTime, RotationInterpSpeed);
}

void UFormationFollowComponent::SyncSlotAssignmentWithManager(APartyManager* Manager)
{
	if (!Manager || !CurrentFormation) return;

	// Initial assignment uses Manager's natural order; Hungarian matching reorders later.
	// 初期割り当てはManagerの並び順をそのまま反映。後でハンガリアン法で最適化。
	const TArray<APartyCharacter*> Followers = Manager->GetFollowers();
	const int32 SlotCount = CurrentFormation->Slots.Num();

	SlotAssignment.Empty();
	SlotAssignment.Reserve(SlotCount);

	for (int32 i = 0; i < SlotCount; ++i)
	{
		SlotAssignment.Add(i < Followers.Num() ? Followers[i] : nullptr);
	}
}

void UFormationFollowComponent::ApplyHungarianMatching()
{
	const int32 N = SlotAssignment.Num();
	if (N == 0 || N != CachedSlotLocations.Num()) return;

	// Skip if any slot is empty (mid-sync transitional state).
	// 過渡状態のスキップ。
	for (int32 i = 0; i < N; ++i)
	{
		if (!SlotAssignment[i]) return;
	}

	// [1] Build cost matrix: distance from each occupant to each slot.
	// コスト行列構築：各occupantから各スロットへの距離。
	TArray<FCostMatrixRow> CostMatrix;
	CostMatrix.Reserve(N);
	for (int32 OccupantIdx = 0; OccupantIdx < N; ++OccupantIdx)
	{
		FCostMatrixRow Row;
		Row.Values.Reserve(N);

		const FVector OccupantLoc = SlotAssignment[OccupantIdx]->GetActorLocation();
		for (int32 SlotIdx = 0; SlotIdx < N; ++SlotIdx)
		{
			Row.Values.Add(FVector::Dist(OccupantLoc, CachedSlotLocations[SlotIdx]));
		}
		CostMatrix.Add(Row);
	}

	// [2] Solve.
	const TArray<int32> Assignment = UHungarianMatchingLibrary::SolveAssignment(CostMatrix);
	if (Assignment.Num() != N) return;

	// [3] Apply: Assignment[i]=j means occupant i moves to slot j.
	// 割り当て適用：Assignment[i]=j ならoccupant iがスロットjへ。
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

	// Fire Hungarian only on sustained stop, not brief slowdowns (prevents misfire on deceleration).
	// 短時間の減速では発動せず、持続的停止のみでハンガリアン発動。
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

bool UFormationFollowComponent::TryGetSlotLocationForCharacter(
	const APartyCharacter* Character, FVector& OutLocation) const
{
	if (!Character) return false;

	// Reverse lookup: SlotAssignment is slot→character; we need character→slot.
	// N is small (<=10) so linear scan is fine.
	// 逆引き：小規模なので線形探索で十分。
	for (int32 SlotIdx = 0; SlotIdx < SlotAssignment.Num(); ++SlotIdx)
	{
		if (SlotAssignment[SlotIdx] == Character)
		{
			if (CachedSlotLocations.IsValidIndex(SlotIdx))
			{
				OutLocation = CachedSlotLocations[SlotIdx];
				return true;
			}
			return false;
		}
	}
	return false;
}

void UFormationFollowComponent::DebugShuffleSlotAssignment()
{
	if (SlotAssignment.Num() < 2) return;

	// Fisher-Yates shuffle so next matching has visible reordering effect.
	// 次のマッチング効果を可視化するためのシャッフル。
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

// =========================================================================
// Yield Behavior implementation
// =========================================================================

APawn* UFormationFollowComponent::GetPlayerPawn() const
{
	if (const UWorld* World = GetWorld())
	{
		return UGameplayStatics::GetPlayerPawn(World, 0);
	}
	return nullptr;
}

bool UFormationFollowComponent::ShouldYieldForSlot(int32 SlotIdx) const
{
	if (!SlotAssignment.IsValidIndex(SlotIdx)) return false;

	APartyCharacter* Occupant = SlotAssignment[SlotIdx];
	if (!Occupant) return false;

	const APawn* Player = GetPlayerPawn();
	if (!Player) return false;

	// [1] Player speed check (stationary player won't block anything).
	// プレイヤーが停止中なら道を塞ぐ概念がないので無効。
	const FVector PlayerVelocity = Player->GetVelocity();
	if (PlayerVelocity.SizeSquared() < FMath::Square(PlayerMovingSpeedThreshold)) return false;

	// [2] Distance check (3D; height-distant occupants get filtered here).
	// 距離チェック（3D）。高低差のあるoccupantはここでフィルタ。
	const FVector OccupantLoc = Occupant->GetActorLocation();
	const FVector PlayerLoc = Player->GetActorLocation();
	const FVector PlayerToOccupant = OccupantLoc - PlayerLoc;
	if (PlayerToOccupant.SizeSquared() > FMath::Square(YieldEnterRadius)) return false;

	// [3] Cone check on horizontal plane.
	// Z is ignored: same-corridor path blocking is a horizontal problem.
	// Distance check above acts as the implicit vertical filter.
	// コーンは水平面で判定。Z無視。垂直方向は距離チェックが暗黙のフィルタとして機能。
	const FVector PlayerDirFlat   = FVector(PlayerVelocity.X,    PlayerVelocity.Y,    0.f).GetSafeNormal();
	const FVector ToOccupantFlat  = FVector(PlayerToOccupant.X,  PlayerToOccupant.Y,  0.f).GetSafeNormal();

	// Compare cosines instead of angles to avoid acos() cost.
	// acos回避のためコサイン値で比較。
	const float CosAngle = FVector::DotProduct(PlayerDirFlat, ToOccupantFlat);
	const float CosThreshold = FMath::Cos(FMath::DegreesToRadians(YieldConeHalfAngleDeg));

	return CosAngle >= CosThreshold;
}

bool UFormationFollowComponent::ShouldExitYieldForSlot(int32 SlotIdx) const
{
	// Exit defaults to TRUE on missing data (recover to Following safely).
	// データ欠損時はTRUE（安全にFollowingへ復帰）。
	if (!SlotAssignment.IsValidIndex(SlotIdx)) return true;

	APartyCharacter* Occupant = SlotAssignment[SlotIdx];
	if (!Occupant) return true;

	const APawn* Player = GetPlayerPawn();
	if (!Player) return true;

	// Distance-only check (no cone). Asymmetric with Enter is the intended hysteresis:
	// stationary player nearby keeps occupant yielding (avoids re-blocking).
	// 距離のみ（コーン無し）。Enterとの非対称性が意図的なヒステリシス：
	// 近くで停止したプレイヤーの場合、Yielding維持で再ブロックを防ぐ。
	const FVector PlayerToOccupant = Occupant->GetActorLocation() - Player->GetActorLocation();
	return PlayerToOccupant.SizeSquared() > FMath::Square(YieldExitRadius);
}

bool UFormationFollowComponent::TryCalculateYieldLocationForSlot(int32 SlotIdx, FVector& OutLocation) const
{
	if (!SlotAssignment.IsValidIndex(SlotIdx)) return false;
	if (!CachedSlotLocations.IsValidIndex(SlotIdx)) return false;

	APartyCharacter* Occupant = SlotAssignment[SlotIdx];
	if (!Occupant) return false;

	const APawn* Player = GetPlayerPawn();
	if (!Player) return false;

	// [1] Player travel direction (horizontal only).
	// プレイヤー進行方向（水平のみ）。
	const FVector PlayerVelocity = Player->GetVelocity();
	const FVector PlayerDirFlat = FVector(PlayerVelocity.X, PlayerVelocity.Y, 0.f).GetSafeNormal();
	if (PlayerDirFlat.IsNearlyZero()) return false;
	
	// TryCalculateYieldLocationForSlot 의 후보 생성 부분
	const FVector OccupantLoc = Occupant->GetActorLocation();
	const FVector PlayerLoc = Player->GetActorLocation();

	// Correct right vector (UE: Right = Up × Forward).
	// UEは Up × Forward = Right。
	const FVector SideDir = FVector::CrossProduct(FVector::UpVector, PlayerDirFlat);

	// Project player velocity onto player→occupant direction.
	// Absolute scalar: walking vs running produces different backward intensity (intentional).
	// プレイヤー速度をプレイヤー→occupant方向に射影。
	// 絶対値で歩き/走りの強度差を意図的に演出。
	const FVector PlayerToOccupantDir = (OccupantLoc - PlayerLoc).GetSafeNormal();
	const float TowardSpeed = FVector::DotProduct(PlayerVelocity, PlayerToOccupantDir);

	// Backward offset along player's travel direction, scaled by projected speed.
	// プレイヤー進行方向への後方オフセット、射影速度に比例。
	const FVector BackwardOffset = PlayerDirFlat * TowardSpeed * YieldBackwardFactor;

	const FVector CandidateRight = OccupantLoc + SideDir * YieldSideDistance + BackwardOffset;
	const FVector CandidateLeft  = OccupantLoc - SideDir * YieldSideDistance + BackwardOffset;
	
	
	
	// [4] Choose the side that moves AWAY from player's path (not toward slot).
	// Early version preferred "closer to slot" for cheap recovery, but in playtest that
	// caused occupants to cross player's path when their slot was on the far side.
	// Player avoidance is the intent; slot return cost is secondary.
	// プレイヤー経路から「離れる」側を優先（元はスロット復帰コスト優先だったが、
	// スロットがプレイヤー反対側にある場合に経路を横切る副作用が判明したため変更）。
	const FVector PlayerToOccupant = OccupantLoc - Player->GetActorLocation();
	const float SideSign = FVector::DotProduct(PlayerToOccupant, SideDir);

	const FVector FirstChoice  = (SideSign >= 0) ? CandidateRight : CandidateLeft;
	const FVector SecondChoice = (SideSign >= 0) ? CandidateLeft  : CandidateRight;

	// Debug: candidates visualized as red/blue spheres.
	// デバッグ：候補を赤青球で可視化。
	DrawDebugSphere(GetWorld(), CandidateRight, 20.f, 8, FColor::Red,  false, 0.5f);
	DrawDebugSphere(GetWorld(), CandidateLeft,  20.f, 8, FColor::Blue, false, 0.5f);

	// [5] NavMesh validation (yield where reachable; give up if not).
	// Honest fallback: "no space to yield" → keep Following.
	// NavMesh検証。退避不可なら正直にFollowing維持。
	if (TryProjectToNavMesh(FirstChoice,  OutLocation)) return true;
	if (TryProjectToNavMesh(SecondChoice, OutLocation)) return true;

	return false;
}

void UFormationFollowComponent::UpdateYieldStates(float DeltaTime)
{
    const int32 N = SlotAssignment.Num();

    // Sync state arrays with slot count.
    // 状態配列のスロット数同期。
    if (SlotYieldStates.Num() != N)
    {
        SlotYieldStates.Init(ESlotYieldState::Following, N);
    }
    if (CachedYieldLocations.Num() != N)
    {
        CachedYieldLocations.Init(FVector::ZeroVector, N);
    }
    if (SlotYieldDelayTimers.Num() != N)
    {
        SlotYieldDelayTimers.Init(0.f, N);
    }

    // Per-slot state machine with entry delay.
    // 進入遅延付きスロット別ステートマシン。
    for (int32 SlotIdx = 0; SlotIdx < N; ++SlotIdx)
    {
        switch (SlotYieldStates[SlotIdx])
        {
        case ESlotYieldState::Following:
            if (ShouldYieldForSlot(SlotIdx))
            {
                // Accumulate delay while condition holds.
                // 条件成立中はタイマー累積。
                SlotYieldDelayTimers[SlotIdx] += DeltaTime;

                if (SlotYieldDelayTimers[SlotIdx] >= YieldEntryDelay)
                {
                    // Delay elapsed → re-evaluate by computing yield location.
                    // 遅延終了 → Yield座標算出による再評価。
                    FVector YieldLoc;
                    if (TryCalculateYieldLocationForSlot(SlotIdx, YieldLoc))
                    {
                        CachedYieldLocations[SlotIdx] = YieldLoc;
                        SlotYieldStates[SlotIdx] = ESlotYieldState::Yielding;
                    }
                    // Reset timer regardless of result (re-try from scratch if calc failed).
                    // 算出失敗時もタイマーリセット（次のチャンスを最初から）。
                    SlotYieldDelayTimers[SlotIdx] = 0.f;
                }
            }
            else
            {
                // Condition broken → reset timer.
                // 条件不成立 → タイマーリセット。
                SlotYieldDelayTimers[SlotIdx] = 0.f;
            }
            break;

        case ESlotYieldState::Yielding:
            if (ShouldExitYieldForSlot(SlotIdx))
            {
                SlotYieldStates[SlotIdx] = ESlotYieldState::Following;
                // Next Component Tick pushes slot coordinate → natural return.
                // 次のComponent Tickでスロット座標が自動push、自然復帰。
            }
            break;
        }
    }
}