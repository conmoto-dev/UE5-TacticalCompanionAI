// Fill out your copyright notice in the Description page of Project Settings.

#include "AI/Components/FormationFollowComponent.h"

#include "AIController.h"
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
#include "AI/Strategies/YieldStrategy.h"
#include "EngineUtils.h"
#include "TacticalCrowdFollowingComponent.h"
#include "TacticalTraversalComponent.h"
#include "GameFramework/CharacterMovementComponent.h"

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
	const float HalfHeight = CurrentLeader->GetSimpleCollisionHalfHeight();
	const FVector LeaderFootLoc = CurrentLeader->GetActorLocation() - FVector(0.f, 0.f, HalfHeight);

	// ===== [5] Slot world position cache (per-slot distance-based trigger) =====
	UpdateFormationCache(LeaderFootLoc, CurrentLeader, false);

	// ===== [6] Slot assignment sync + Hungarian on stop =====
	if (SlotAssignment.Num() != CurrentFormation->Slots.Num())
	{
		SyncSlotAssignmentWithManager(Manager);
	}
	HandleStopMatching(DeltaTime, CurrentLeader);

	// ===== [6.5] Yield state evaluation (per-slot) =====
	UpdateYieldStates(DeltaTime);

	// ===== [7] Push positions to occupants (slot OR yield OR traversal-aware) =====
	for (int32 SlotIdx = 0; SlotIdx < SlotAssignment.Num() && SlotIdx < CachedSlotLocations.Num(); ++SlotIdx)
	{
		APartyCharacter* Occupant = SlotAssignment[SlotIdx];
		if (!Occupant) continue;

		// 1. 현재 틱의 진짜 목표 위치 계산.
		const bool bIsYielding = SlotYieldStates.IsValidIndex(SlotIdx)
			&& SlotYieldStates[SlotIdx] == ESlotYieldState::Yielding;
		const FVector TargetLoc = bIsYielding ? CachedYieldLocations[SlotIdx] : CachedSlotLocations[SlotIdx];

		UTacticalTraversalComponent* TraversalComp = GetOrCacheTraversalComp(Occupant);

		// =====================================================================
		// [신규 추가] Detour Crowd 상태 주입 (Injection)
		// 전술 행동(점프) 중이 아닐 때만 Crowd Component를 제어하여 충돌을 방지합니다.
		// =====================================================================
		if (!TraversalComp || !TraversalComp->IsTraversing())
		{
			if (AAIController* AICon = Cast<AAIController>(Occupant->GetController()))
			{
				if (UTacticalCrowdFollowingComponent* CrowdComp = Cast<UTacticalCrowdFollowingComponent>(AICon->GetPathFollowingComponent()))
				{
					// 리더는 플레이어가 조종하므로 AIController 루프를 타지 않음 (false 고정)
					CrowdComp->SetTacticalAvoidanceState(false, bIsYielding);
				}
			}
		}

		// 2. 디커플링: 이미 전술적 행동 중이면 간섭 정책 분기.
		bool bForcedByAbort = false;
		if (TraversalComp && TraversalComp->IsTraversing())
		{
			const ETraversalState TState = TraversalComp->GetCurrentState();
			if (TState == ETraversalState::Airborne)
			{
				continue; // 공중에선 절대 간섭 금지
			}
			else if (TState == ETraversalState::MovingToTakeoff || TState == ETraversalState::SteeringToTakeoff)
			{
				// 목표가 너무 멀리 표류 → abort 후 4번 분기에서 재명령.
				const float DriftSq = FVector::DistSquared(TargetLoc, TraversalComp->GetCachedFinalTarget());
				if (DriftSq > FMath::Square(TraversalTargetDriftThreshold))
				{
					TraversalComp->AbortTraversal();
					bForcedByAbort = true;
				}
				else
				{
					continue;
				}
			}
		}

		// 3. 전술 이동 판단: 점프가 필요한 단차인지 검사 (Yield 중 점프 금지).
		if (!bIsYielding && TraversalComp && !TraversalComp->IsTraversing())
		{
			if (UCharacterMovementComponent* MoveComp = Occupant->GetCharacterMovement())
			{
				const float ZDiff = TargetLoc.Z - Occupant->GetActorLocation().Z;
				const float MaxStepHeight = MoveComp->MaxStepHeight;
				if (ZDiff > (MaxStepHeight + JumpZThresholdMargin))
				{
					if (TraversalComp->RequestTacticalTraversal(TargetLoc))
					{
						continue; // 점프 명령 성공 → 보행 명령 스킵.
					}
				}
			}
		}
		
		// 4. 일반 이동 명령. Yield 중 또는 abort 직후엔 force refresh.
		Occupant->UpdateTargetSlotLocation(TargetLoc, bIsYielding || bForcedByAbort);
	}

	// ===== [8] Debug visualization =====
	for (int32 SlotIdx = 0; SlotIdx < SlotAssignment.Num() && SlotIdx < CachedSlotLocations.Num(); ++SlotIdx)
	{
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
			DrawDebugLine(GetWorld(), Occupant->GetActorLocation(), CachedSlotLocations[SlotIdx], FColor::Cyan, false, -1.0f, 0, 1.5f);

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
	LastCalculatedSlotLocations.Empty();
	
	// Invalidate so it gets re-synced on next tick with the new slot count.
	// 新スロット数に合わせて次Tickで再同期。
	SlotAssignment.Empty();
	SlotYieldStates.Empty();
	CachedYieldLocations.Empty();
	SlotYieldDelayTimers.Empty();
}

void UFormationFollowComponent::UpdateFormationRotation(float DeltaTime, AActor* CurrentLeader)
{
	if (!CurrentLeader) return;

	// Quaternion interp (Slerp) to avoid -180/+180 reverse-rotation bug of FRotator (Euler).
	// FRotator(オイラー角)補間は-180/180境界で逆回転バグが起きるためQuaternionで補間。
	const FQuat TargetRotation = CurrentLeader->GetActorQuat();
	CachedFormationRotation = FMath::QInterpTo(CachedFormationRotation, TargetRotation, DeltaTime, RotationInterpSpeed);
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

    const int32 SlotCount = CurrentFormation->Slots.Num();

    // Sync per-slot cache array.
    // スロット別キャッシュ配列の同期。
    if (LastCalculatedSlotLocations.Num() != SlotCount)
    {
        LastCalculatedSlotLocations.Init(FVector(MAX_flt, MAX_flt, MAX_flt), SlotCount);
    }

    // Get player location once for per-slot distance check.
    // プレイヤー位置を1回取得。
    const APawn* Player = GetPlayerPawn();
    const bool bHasPlayer = (Player != nullptr);
    const FVector PlayerLoc = bHasPlayer ? Player->GetActorLocation() : FVector::ZeroVector;

    // Per-slot trigger: update only when the slot's last cached position is
    // far enough from the player. This prevents the slot from "chasing" the
    // player during passage/Yield, which previously caused occupants to
    // cross the player's path on Yield exit.
    // スロット別トリガー: プレイヤーがスロットから十分離れた時のみ再算出。
    // Yield/通過中にスロット座標がプレイヤーを追う現象を防ぐ。
    for (int32 i = 0; i < SlotCount; ++i)
    {
        bool bShouldUpdate = bForceUpdate;

        if (!bShouldUpdate)
        {
            if (bHasPlayer)
            {
                const float DistSq = FVector::DistSquared(LastCalculatedSlotLocations[i], PlayerLoc);
                bShouldUpdate = (DistSq > FMath::Square(SlotCacheUpdateDistance));
            }
            else
            {
                // No-player fallback: always update (degenerate case).
                // プレイヤー不在時は常に更新。
                bShouldUpdate = true;
            }
        }

        if (bShouldUpdate)
        {
            const FVector IdealLoc = CalculateIdealLocation(i, LeaderFootLoc);
            CachedSlotLocations[i] = AdjustLocationForEnvironment(IdealLoc, CurrentLeader, LeaderFootLoc);
            LastCalculatedSlotLocations[i] = CachedSlotLocations[i];
        }
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
			const bool bHasYieldingSlot = SlotYieldStates.Contains(ESlotYieldState::Yielding);
			if (!bHasYieldingSlot)
			{
				ApplyHungarianMatching();
				bMatchingAppliedOnStop = true;
			}
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

APawn* UFormationFollowComponent::GetPlayerPawn() const
{
	if (const UWorld* World = GetWorld())
	{
		return UGameplayStatics::GetPlayerPawn(World, 0);
	}
	return nullptr;
}

// =========================================================================
// IYieldContextProvider implementation
// =========================================================================

int32 UFormationFollowComponent::GetSlotCount() const
{
	return SlotAssignment.Num();
}

APartyCharacter* UFormationFollowComponent::GetOccupantAt(int32 SlotIdx) const
{
	if (!SlotAssignment.IsValidIndex(SlotIdx)) return nullptr;
	return SlotAssignment[SlotIdx];
}

FVector UFormationFollowComponent::GetSlotLocationAt(int32 SlotIdx) const
{
	if (!CachedSlotLocations.IsValidIndex(SlotIdx)) return FVector::ZeroVector;
	return CachedSlotLocations[SlotIdx];
}

APawn* UFormationFollowComponent::GetTargetPawn() const
{
	// Currently always the player. Future: could pick different target per context
	// (e.g., enemy aggressor in battle).
	// 現在は常にプレイヤー。将来的にコンテキスト別ターゲット選択可能。
	return GetPlayerPawn();
}

void UFormationFollowComponent::UpdateYieldStates(float DeltaTime)
{
	const int32 N = SlotAssignment.Num();

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
	
	if (!CurrentFormation || !CurrentFormation->YieldStrategy) return;
	
	UYieldStrategy* Strategy = CurrentFormation->YieldStrategy;
	const TScriptInterface<IYieldContextProvider> Context = this;

	for (int32 SlotIdx = 0; SlotIdx < N; ++SlotIdx)
	{
		APartyCharacter* Occupant = SlotAssignment[SlotIdx];
		UTacticalTraversalComponent* TraversalComp = GetOrCacheTraversalComp(Occupant);
		
		// =====================================================================
		// [신규 추가] 점프 등 전술 행동 중인 에이전트는 Yield 판단에서 완벽히 격리
		// =====================================================================
		if (TraversalComp && TraversalComp->IsTraversing()) 
		{
			SlotYieldStates[SlotIdx] = ESlotYieldState::Following;
			SlotYieldDelayTimers[SlotIdx] = 0.f;
			continue; 
		}

		switch (SlotYieldStates[SlotIdx])
		{
		case ESlotYieldState::Following:
			if (Strategy->ShouldYieldForSlot(Context, SlotIdx))
			{
				SlotYieldDelayTimers[SlotIdx] += DeltaTime;
				if (SlotYieldDelayTimers[SlotIdx] >= Strategy->GetEntryDelay())
				{
					FVector YieldLoc;
					if (Strategy->TryCalculateYieldLocationForSlot(Context, SlotIdx, YieldLoc))
					{
						CachedYieldLocations[SlotIdx] = YieldLoc;
						SlotYieldStates[SlotIdx] = ESlotYieldState::Yielding;
						UE_LOG(LogTemp, Warning, TEXT("[Yield] Slot %d ENTER Yielding at %s"),
							SlotIdx, *YieldLoc.ToString());
					}
					SlotYieldDelayTimers[SlotIdx] = 0.f;
				}
			}
			else
			{
				SlotYieldDelayTimers[SlotIdx] = 0.f;
			}
			break;

		case ESlotYieldState::Yielding:
			if (Strategy->ShouldExitYieldForSlot(Context, SlotIdx))
			{
				SlotYieldStates[SlotIdx] = ESlotYieldState::Following;
				UE_LOG(LogTemp, Warning, TEXT("[Yield] Slot %d EXIT Yielding -> Following"), SlotIdx);
			}
			break;
		}
	}
}

UTacticalTraversalComponent* UFormationFollowComponent::GetOrCacheTraversalComp(APartyCharacter* Character)
{
	if (!Character) return nullptr;

	if (TWeakObjectPtr<UTacticalTraversalComponent>* Found = TraversalCompCache.Find(Character))
	{
		if (Found->IsValid())
		{
			return Found->Get();
		}
	}

	UTacticalTraversalComponent* Comp = Character->FindComponentByClass<UTacticalTraversalComponent>();
	if (Comp)
	{
		TraversalCompCache.Add(Character, Comp);
	}
	return Comp;
}