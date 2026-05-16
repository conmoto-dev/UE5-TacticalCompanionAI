// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Kismet/KismetMathLibrary.h"
#include "FormationFollowComponent.generated.h"

class UFormationDataAsset;
class APartyCharacter;
class APartyManager;

/**
 * Per-slot Yield state. Following = normal slot tracking, Yielding = stepped aside for player.
 * スロット単位のYield状態。Following=通常追従、Yielding=プレイヤー退避中。
 */
UENUM(BlueprintType)
enum class ESlotYieldState : uint8
{
	Following  UMETA(DisplayName = "Following"),
	Yielding   UMETA(DisplayName = "Yielding"),
};

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class TACTICALAI_API UFormationFollowComponent : public UActorComponent
{
	GENERATED_BODY()

	// =========================================================================
	// Lifecycle
	// =========================================================================
public:
	UFormationFollowComponent();
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// Set new formation data and reset cache. Called by PartyManager.
	// PartyManagerからの呼び出し専用。隊形変更時のキャッシュリセット。
	void ApplyFormation(class UFormationDataAsset* NewFormation);

protected:
	virtual void BeginPlay() override;


	// =========================================================================
	// Formation Data
	// =========================================================================
protected:
	/** Active formation data. Designers assign different DataAssets for different shapes. */
	/** 現在使用中の隊形データ。デザイナーが用途に応じて差し替え可能。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Formation")
	TObjectPtr<UFormationDataAsset> CurrentFormation;


	// =========================================================================
	// Locomotion (Gap Scale, Spring Physics, Rotation)
	// =========================================================================
protected:
	// Speed mapping: X = MinSpeed (Idle/Walk), Y = MaxSpeed (Sprint).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Formation|Locomotion")
	FVector2D LeaderSpeedRange = FVector2D(0.f, 500.f);

	// Scale mapping: X = base gap (1.0), Y = max expanded gap (e.g., 1.5).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Formation|Locomotion")
	FVector2D GapScaleRange = FVector2D(1.0f, 1.5f);

	// Tension speed for formation expansion/contraction (rubber-band feel).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Formation|Locomotion")
	float GapInterpSpeed = 3.0f;

	// Spring stiffness (k): higher = stronger pull toward target.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Formation|Locomotion")
	float SpringStiffness = 50.0f;

	// Spring damping (c): 1.0 = critical damping (no overshoot, precise settling).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Formation|Locomotion")
	float SpringDamping = 1.0f;

	// Formation rotation lerp speed. Lower = heavier/more sluggish feel on turns.
	// 低い値ほど旋回時の「重み」が強くなる。
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Formation|Locomotion")
	float RotationInterpSpeed = 5.0f;

private:
	// Currently interpolating gap scale; kept across ticks for spring state.
	float CurrentGapScale = 1.0f;

	// Spring velocity state for FloatSpringInterp (engine updates by reference).
	// FloatSpringInterpが参照渡しで毎Tick更新する内部状態。
	FFloatSpringState GapScaleSpringVelocity;

	// Virtual delayed-rotation space used as the reference frame for follower targets.
	// 仲間の目標座標の基準となる「仮想の遅延回転空間」。
	FQuat CachedFormationRotation = FQuat::Identity;

	void UpdateGapScale(float DeltaTime, AActor* CurrentLeader);
	void UpdateFormationRotation(float DeltaTime, AActor* CurrentLeader);


	// =========================================================================
	// Slot Position Cache & Calculation
	// =========================================================================
private:
	// Final computed slot world positions, cached per update cycle.
	TArray<FVector> CachedSlotLocations;

	// Integrates GapScale + Rotation into slot world positions.
	// bForceUpdate=true skips distance-threshold caching and forces recalculation.
	void UpdateFormationCache(const FVector& LeaderFootLoc, AActor* CurrentLeader, bool bForceUpdate = false);

	// Pure math: ideal slot world position from leader's foot location.
	FVector CalculateIdealLocation(int32 SlotIndex, const FVector& LeaderFootLoc) const;


	// =========================================================================
	// Environment Adjustment (NavMesh, Slope, Wall Slide)
	// Each helper is single-responsibility; AdjustLocation is a thin orchestrator.
	// 各ヘルパーは単一責任、メイン関数は流れの調整役。
	// =========================================================================
private:
	// 4-step orchestrator: ground Z → NavMesh project → wall slide → fallback.
	FVector AdjustLocationForEnvironment(const FVector& IdealLocation, const AActor* CurrentLeader, const FVector& LeaderFootLoc) const;

	// Project a point onto NavMesh. Writes OutResult only on success.
	bool TryProjectToNavMesh(const FVector& Point, FVector& OutResult) const;

	// Find actual ground Z at X,Y by vertical trace. Critical for slope-aware Z correction.
	// スロットのローカルZ=0仮定が傾斜面で破綻するため、垂直トレースで実地面Zを取得。
	bool TryFindGroundZ(const FVector& Point, float& OutZ, const AActor* IgnoreActor) const;

	// Sliding position when blocked by vertical wall. Returns false for slopes (normal.Z above threshold).
	// 法線で「壁」と「傾斜」を区別。傾斜面ではfalseを返しスライディング不要を通知。
	bool TryCalculateWallSlide(const FVector& From, const FVector& To, const AActor* IgnoreActor, FVector& OutSlidLocation) const;

	// Last-resort: tow slot toward leader so AIController->MoveTo doesn't break on invalid coords.
	// 全失敗時の最終手段。無効座標でMoveToが失敗しないようリーダー方向へ引き戻し。
	FVector CalculateFallbackLocation(const FVector& LeaderFootLoc, const FVector& IdealLocation) const;


	// =========================================================================
	// Auto Formation Switching (Corridor Width Measurement)
	// =========================================================================
public:
	// Two formations swapped per measured corridor width.
	// 通路幅で自動切り替えされる2隊形。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Formation|Switching")
	TObjectPtr<UFormationDataAsset> WideFormation;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Formation|Switching")
	TObjectPtr<UFormationDataAsset> NarrowFormation;

	// Hysteresis thresholds: switch to Narrow when below, Wide when above; between = keep current.
	// 二重しきい値：下回るとNarrow、上回るとWide、間は現状維持（境界点滅防止）。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Formation|Switching")
	float NarrowThreshold = 300.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Formation|Switching")
	float WideThreshold = 300.0f;

	// Probe distance for left/right NavMesh edge detection.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Formation|Switching")
	float CorridorProbeDistance = 500.0f;

private:
	// Total NavMesh-walkable width perpendicular to leader's facing.
	// Known limitation: leader-position based; edge cases may misfire.
	// リーダー位置基準の測定のため特定角ケースで誤発動の可能性あり。
	float MeasureCorridorWidth(const AActor* Leader) const;

	UFormationDataAsset* SelectFormationByWidth(float Width) const;


	// =========================================================================
	// Slot Matching (Hungarian Algorithm on Sustained Stop)
	// =========================================================================
protected:
	// Speed below this counts as "stopped" for matching trigger.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Formation|Matching")
	float StopSpeedThreshold = 50.0f;

	// Sustained stop duration required before firing matching (prevents misfire on brief slowdowns).
	// 短時間減速での誤発動を防ぐための必要持続時間。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Formation|Matching")
	float StopDurationToTrigger = 0.3f;

private:
	/**
	 * Slot-to-character mapping owned by the formation system.
	 * SlotAssignment[slot_idx] = character occupying that slot.
	 * Updated by Hungarian matching; PartyManager.Members order stays untouched.
	 * 隊形システム内部のスロット-キャラクター割り当て。
	 * ハンガリアン法で更新、PartyManager.Members順序は不変。
	 */
	UPROPERTY()
	TArray<TObjectPtr<APartyCharacter>> SlotAssignment;

	// Accumulated time below StopSpeedThreshold; resets on movement resume.
	float CurrentStopDuration = 0.f;

	// True once matching fired for the current stop event; resets on resume.
	bool bMatchingAppliedOnStop = false;

	void SyncSlotAssignmentWithManager(APartyManager* Manager);
	void ApplyHungarianMatching();
	void HandleStopMatching(float DeltaTime, AActor* CurrentLeader);


	// =========================================================================
	// Slot Query (External access for character-side logic)
	// =========================================================================
public:
	/** Returns the world-space slot location assigned to the given character. */
	/** 指定キャラクターに割り当てられたスロットのワールド座標を返す。 */
	bool TryGetSlotLocationForCharacter(const APartyCharacter* Character, FVector& OutLocation) const;


	// =========================================================================
	// Yield Behavior (Per-slot path-clearing for player passage)
	//
	// Design notes:
	// - Yield is a FORMATION-level policy (not a character-intrinsic behavior).
	//   Different formations may need different yield logic (e.g., Flock vs Phalanx).
	// - State is per-slot, not per-character: one yielding slot doesn't disturb others.
	// - Characters remain passive: they receive coordinates, don't decide them.
	//
	// 設計メモ：
	// - Yieldは隊形単位の方針（キャラクター本来の行動ではない）。
	//   FlockやPhalanxなど将来の隊形では別ロジックが必要になり得る。
	// - 状態はスロット単位、キャラ単位ではない（1スロットのYieldが他に影響しない）。
	// - キャラクターは受動：座標を受け取るだけで決定はしない。
	// =========================================================================
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
protected:
	// ───── Detection Parameters ─────
	UPROPERTY(EditDefaultsOnly, Category = "Formation|Yield|Detection", meta = (ClampMin = "0.0"))
	float YieldEnterRadius = 500.f;

	UPROPERTY(EditDefaultsOnly, Category = "Formation|Yield|Detection", meta = (
	ClampMin = "0.0",
	ToolTip = "Must be greater than or equal to YieldEnterRadius (auto-corrected if not). Forms hysteresis to prevent immediate re-exit after Yielding entry."))
	float YieldExitRadius = 700.f;

	UPROPERTY(EditDefaultsOnly, Category = "Formation|Yield|Detection", meta = (ClampMin = "0.0", ClampMax = "180.0"))
	float YieldConeHalfAngleDeg = 50.f;

	// ───── Geometry Parameters ─────

	// Side-step distance from occupant's current position.
	// occupant現在位置から横方向に退避する距離。
	UPROPERTY(EditDefaultsOnly, Category = "Formation|Yield|Geometry", meta = (ClampMin = "0.0"))
	float YieldSideDistance = 200.f;

private:
	// ───── State (per slot, parallel to SlotAssignment) ─────

	/** Per-slot yield state. Index synced with SlotAssignment. */
	/** スロット別Yield状態。インデックスはSlotAssignmentと同期。 */
	TArray<ESlotYieldState> SlotYieldStates;

	/** Cached yield coordinate per slot. Only meaningful when Yielding. */
	/** スロット別Yield座標キャッシュ。Yielding時のみ有効。 */
	TArray<FVector> CachedYieldLocations;

	// ───── Logic ─────

	bool ShouldYieldForSlot(int32 SlotIdx) const;
	bool ShouldExitYieldForSlot(int32 SlotIdx) const;

	/** Computes yield target with NavMesh validation. Returns false if no valid spot. */
	/** Yield目標を算出（NavMesh検証含む）。退避不可ならfalse。 */
	bool TryCalculateYieldLocationForSlot(int32 SlotIdx, FVector& OutLocation) const;

	/** Per-tick state machine update for all slots. */
	/** 全スロットのステートマシン更新。 */
	void UpdateYieldStates(float DeltaTime);

	APawn* GetPlayerPawn() const;

	// Reaction time delay before triggering Yield. Mimics human perception-action
	// delay so 3 occupants entering the cone don't all yield to the same direction
	// simultaneously (the main cause of deadlock).
	// Reaction Timeモデルでcone進入から発動までの遅延。
	// 同時反応の不自然さとデッドロックを軽減。
	UPROPERTY(EditDefaultsOnly, Category = "Formation|Yield|Detection", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float YieldEntryDelay = 0.2f;
	
	/** Per-slot delay timer. Accumulates while ShouldYield is true; reset on condition break.
	When >= YieldEntryDelay, re-evaluate and transition to Yielding. */
	/** スロット別の遅延タイマー。ShouldYield成立中累積、不成立でリセット。
		YieldEntryDelay到達時に再評価しYieldingへ遷移。 */
	TArray<float> SlotYieldDelayTimers;
	
	UPROPERTY(EditDefaultsOnly, Category = "Formation|Yield|Geometry", meta = (ClampMin = "0.0"))
	float YieldBackwardFactor = 0.25f;
	
	// Per-slot last-calculated location for distance-from-player based caching.
	// Used by the new (per-slot) cache trigger; replaces single LastCalculatedLocation.
	// 各スロットの最終算出位置。スロット別キャッシュトリガー用、単一基準を置換。
	TArray<FVector> LastCalculatedSlotLocations;

	// Player-distance threshold: a slot only recalculates when the player has moved
	// at least this far from its last cached position.
	// プレイヤーがスロットからこの距離以上離れた時のみ再算出。
	UPROPERTY(EditDefaultsOnly, Category = "Formation|Cache", meta = (ClampMin = "0.0"))
	float SlotCacheUpdateDistance = 500.f;
	
	// =========================================================================
	// Debug
	// =========================================================================
public:
	// Randomly shuffle SlotAssignment so next matching has visible effect.
	// SlotAssignmentをランダム化し、次マッチング効果を可視化。
	void DebugShuffleSlotAssignment();
};