// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Kismet/KismetMathLibrary.h"
#include "FormationFollowComponent.generated.h"


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

protected:
	// Local-space offset of each formation slot. Adjustable by designers in the editor.
	// 各スロットのローカルオフセット。エディタで調整可能。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Formation")
	TArray<FVector> SlotOffsets;
	
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
	
	// Adjusts the slot position for environmental obstacles (walls, ledges) using sweeps and NavMesh projection.
	// 障害物（壁・段差）を考慮した最終的な補正座標を算出。
	FVector AdjustLocationForEnvironment(const FVector& IdealLocation,const AActor* CurrentLeader, const FVector& LeaderFootLoc) const;
	
	// Virtual delayed-rotation space used as the reference frame for follower target positions.
	// Initialized as identity quaternion.
	// 仲間の目標座標の基準となる「仮想の遅延回転空間」。
	FQuat CachedFormationRotation = FQuat::Identity;

	// Smoothly updates the virtual rotation space each tick.
	void UpdateFormationRotation(float DeltaTime, AActor* CurrentLeader);
};