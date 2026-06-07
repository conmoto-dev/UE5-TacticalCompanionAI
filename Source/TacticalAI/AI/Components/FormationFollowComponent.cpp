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
#include "Interfaces/TacticalAvoidanceController.h"

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

	if (LastCalculatedSlotLocations.Num() != SlotCount)
	{
		LastCalculatedSlotLocations.Init(FVector(MAX_flt, MAX_flt, MAX_flt), SlotCount);
	}

	// [a] 기준 프레임 1회 계산 (전 슬롯 공유).
	const FTransform Anchor = GetFormationAnchor(LeaderFootLoc);

	// [b] 로컬 슬롯 1회 계산 (전 슬롯 한꺼번에 — 미래 절차적 생성 대비).
	CalculateRawSlots(CachedLocalSlots);

	const APawn* Player = GetPlayerPawn();
	const bool bHasPlayer = (Player != nullptr);
	const FVector PlayerLoc = bHasPlayer ? Player->GetActorLocation() : FVector::ZeroVector;

	// 슬롯별 거리 트리거 캐시는 그대로 유지.
	// raw 슬롯은 위에서 전부 뽑았지만, 환경보정+반영은 기존처럼 슬롯별 선택적.
	for (int32 i = 0; i < SlotCount && i < CachedLocalSlots.Num(); ++i)
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
				bShouldUpdate = true;
			}
		}

		if (bShouldUpdate)
		{
			// [c] local → world 변환. 이 한 줄이 평시/전투 공통 기하.
			const FVector IdealLoc = Anchor.TransformPosition(CachedLocalSlots[i]);
			CachedSlotLocations[i] = AdjustLocationForEnvironment(IdealLoc, LeaderFootLoc, CurrentLeader);
			LastCalculatedSlotLocations[i] = CachedSlotLocations[i];
		}
	}
}

FTransform UFormationFollowComponent::GetFormationAnchor(const FVector& LeaderFootLoc) const
{
	// [a] 기준 프레임 = 리더 발밑(원점) + 평활화된 회전(방향).
	// 리더 즉시 회전이 아니라 CachedFormationRotation을 쓰는 건 급선회 시 "무게감" 연출.
	// 전투에서는 이 함수만 target 기준으로 바뀐다 (호출부·변환·이후 파이프라인 불변).
	return FTransform(CachedFormationRotation, LeaderFootLoc);
}

void UFormationFollowComponent::CalculateRawSlots(TArray<FVector>& OutLocalOffsets) const
{
	OutLocalOffsets.Reset();
	if (!CurrentFormation) return;

	// [b] 기준 프레임 로컬 공간의 슬롯 오프셋.
	// 평시 = DataAsset 정적 offset × gap scale.
	// 전투에서는 이 부분이 절차적 생성(각도 분배 Strategy)으로 갈린다.
	OutLocalOffsets.Reserve(CurrentFormation->Slots.Num());
	for (const FFormationSlotData& Slot : CurrentFormation->Slots)
	{
		OutLocalOffsets.Add(Slot.LocalOffset * CurrentGapScale);
	}
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

	// SlotAssignment(멤버)와 슬롯 좌표를 부모 순수 함수에 넘기고 결과를 받음.
	// 비용행렬 구축·헝가리안 호출은 부모가 담당 (Battle과 공유).
	TArray<APartyCharacter*> Occupants;
	Occupants.Reserve(N);
	for (const TObjectPtr<APartyCharacter>& C : SlotAssignment)
	{
		Occupants.Add(C);
	}

	const TArray<APartyCharacter*> NewAssignment = SolveSlotAssignment(Occupants, CachedSlotLocations);
	
	SlotAssignment.Empty(N);
	for (APartyCharacter* C : NewAssignment)
	{
		SlotAssignment.Add(C);
	}
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
						
						SyncAvoidanceRoleForSlot(SlotIdx, ECrowdAvoidanceRole::Yielding);
						
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
				
				SyncAvoidanceRoleForSlot(SlotIdx, ECrowdAvoidanceRole::Normal);
				
				UE_LOG(LogTemp, Warning, TEXT("[Yield] Slot %d EXIT Yielding -> Following"), SlotIdx);
			}
			break;
		}
	}
}

// =========================================================================
// DetourCrowd implementation
// =========================================================================
void UFormationFollowComponent::SyncAvoidanceRoleForSlot(int32 SlotIdx, ECrowdAvoidanceRole Role)
{
	APartyCharacter* Occupant = SlotAssignment.IsValidIndex(SlotIdx) ? SlotAssignment[SlotIdx] : nullptr;
	if (!Occupant) return;

	// 컨트롤러가 Player/AI/Enemy 무엇이든, 인터페이스 구현했으면 잡힌다.
	if (auto* Avoid = Cast<ITacticalAvoidanceController>(Occupant->GetController()))
	{
		Avoid->SetAvoidanceRole(Role);
	}
}

// =========================================================================
// Traversal(JumpDetect) implementation
// =========================================================================
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