// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Kismet/KismetMathLibrary.h"
#include "FormationFollowComponent.generated.h"

class UFormationDataAsset;

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class TACTICALAI_API UFormationFollowComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	UFormationFollowComponent();

protected:
	virtual void BeginPlay() override;

public:	
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// Internal: Set new formation data and reset cache. Called by PartyManager.
	// PartyManagerからの呼び出し専用。隊形変更時のキャッシュリセット。
	void ApplyFormation(class UFormationDataAsset* NewFormation);
	
protected:
	/** Active formation data. Designers assign different DataAssets for different shapes. */
	/** 現在使用中の隊形データ。デザイナーが用途に応じて差し替え可能。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Formation")
	TObjectPtr<UFormationDataAsset> CurrentFormation;
	
	// Speed mapping: X = MinSpeed (Idle/Walk), Y = MaxSpeed (Sprint)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Formation|Locomotion")
	FVector2D LeaderSpeedRange = FVector2D(0.f, 500.f); 

	// Scale mapping: X = base gap (1.0), Y = max expanded gap (e.g., 1.5)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Formation|Locomotion")
	FVector2D GapScaleRange = FVector2D(1.0f, 1.5f);

	// Tension speed for formation expansion/contraction (rubber-band feel)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Formation|Locomotion")
	float GapInterpSpeed = 3.0f;
	
	// [Locomotion - Spring Physics Parameters]
	// Replaced FInterpTo with FloatSpringInterp for natural elastic motion.
	// k value: spring stiffness (higher = stronger pull toward target)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Formation|Locomotion")
	float SpringStiffness = 50.0f; 

	// c value: 1.0 = critical damping (no overshoot, settles precisely on target)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Formation|Locomotion")
	float SpringDamping = 1.0f; 
	
	// How quickly the formation rotation follows the leader's rotation.
	// Lower values feel heavier and more sluggish.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Formation|Locomotion")
	float RotationInterpSpeed = 5.0f;
	
private:
	// Cached values for distance-based polling. Initialized to unreachable max so the first tick always updates.
	// 距離ベースポーリング用のキャッシュ。初回Tickで必ず更新されるよう最大値で初期化。
	FVector LastCalculatedLocation = FVector(MAX_flt, MAX_flt, MAX_flt);
	FQuat LastCalculatedRotation = FQuat::Identity;
	
	// Final computed slot world positions, cached per update cycle.
	TArray<FVector> CachedSlotLocations;
	
	// Currently interpolating gap scale, kept as a member to maintain spring state across ticks.
	float CurrentGapScale = 1.0f;
	
	// Spring velocity state for the gap scale's secondary spring system.
	// The engine updates this by reference each tick.
	// FloatSpringInterpが内部状態として参照渡しで更新する変数。
	FFloatSpringState GapScaleSpringVelocity;
	
	// 1. State update: called per tick to mathematically update CurrentGapScale.
	void UpdateGapScale(float DeltaTime, AActor* CurrentLeader);
	
	// 2. Cache update: integrates the result of UpdateGapScale into slot positions.
	// bForceUpdate=true skips the distance-threshold check and forces recalculation.
	void UpdateFormationCache(const FVector& LeaderFootLoc, AActor* CurrentLeader, bool bForceUpdate = false);
	
	// Calculates the ideal slot world position from the leader's foot location, using pure math.
	FVector CalculateIdealLocation(int32 SlotIndex, const FVector& LeaderFootLoc) const;
	
	// Virtual delayed-rotation space used as the reference frame for follower target positions.
	// Initialized as identity quaternion.
	// 仲間の目標座標の基準となる「仮想の遅延回転空間」。
	FQuat CachedFormationRotation = FQuat::Identity;

	// Smoothly updates the virtual rotation space each tick.
	void UpdateFormationRotation(float DeltaTime, AActor* CurrentLeader);
	
	// Adjusts the slot position for environmental obstacles (walls, ledges) using sweeps and NavMesh projection.
	// 障害物（壁・段差）を考慮した最終的な補正座標を算出。
	FVector AdjustLocationForEnvironment(const FVector& IdealLocation,const AActor* CurrentLeader, const FVector& LeaderFootLoc) const;

	// ----- Environment adjustment helpers -----
	// Each helper has a single responsibility, so AdjustLocationForEnvironment can act as a thin orchestrator.
	// 各ヘルパーは単一責任を持ち、メイン関数は流れの調整役に徹する設計。

	// Project a point onto NavMesh. Returns true on success.
	// NavMeshへの投影を試みる。成功時のみOutResultを書き換える。
	bool TryProjectToNavMesh(const FVector& Point, FVector& OutResult) const;

	// Find the actual ground Z at the given X,Y by tracing vertically.
	// Critical for slope handling: slot's local-space Z=0 assumption breaks on inclines.
	// スロットのローカルZ=0仮定が傾斜面で破綻するため、垂直トレースで実際の地面Zを取得。
	bool TryFindGroundZ(const FVector& Point, float& OutZ, const AActor* IgnoreActor) const;

	// Calculate sliding position when blocked by a vertical wall.
	// Returns false for slopes (ImpactNormal.Z above threshold) so caller knows not to use sliding.
	// 法線で「壁」と「傾斜」を区別。傾斜面の場合はfalseを返し、呼び出し側にスライディング不要を伝える。
	bool TryCalculateWallSlide(const FVector& From, const FVector& To, const AActor* IgnoreActor, FVector& OutSlidLocation) const;

	// Last-resort fallback when all NavMesh queries and sliding attempts fail.
	// Tows the slot toward the leader to keep companions in a valid area.
	// 全ての試みが失敗した時の最終手段。仲間が無効な領域に取り残されないようリーダー方向へ引き戻す。
	FVector CalculateFallbackLocation(const FVector& LeaderFootLoc, const FVector& IdealLocation) const;
	
public:
	// [Environment-based formation switching]
	// Two formations for environment-driven swap. Component selects between them
	// each tick based on measured corridor width.
	// 環境に応じて自動切り替えする2つの隊形。毎Tick通路幅を測定して選択。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Formation|Switching")
	TObjectPtr<UFormationDataAsset> WideFormation;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Formation|Switching")
	TObjectPtr<UFormationDataAsset> NarrowFormation;

	// Hysteresis thresholds: switch to Narrow when below, Wide when above.
	// Between values: keep current formation (prevents flickering at boundary).
	// 二重閾値: 下回るとNarrow、上回るとWide、間は現状維持（境界での点滅防止）。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Formation|Switching")
	float NarrowThreshold = 300.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Formation|Switching")
	float WideThreshold = 300.0f;

	// Probe distance for left/right NavMesh edge detection.
	// 左右のNavMesh境界探索距離。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Formation|Switching")
	float CorridorProbeDistance = 500.0f;
	
private:
	// Measure total NavMesh-walkable width perpendicular to leader's facing.
	// リーダーの正面に垂直な方向のNavMesh通行可能幅を測定。
	float MeasureCorridorWidth(const AActor* Leader) const;

	// Select target formation by measured width (hysteresis-aware).
	// 測定幅とヒステリシスを考慮した目標隊形の選択。
	UFormationDataAsset* SelectFormationByWidth(float Width) const;
};