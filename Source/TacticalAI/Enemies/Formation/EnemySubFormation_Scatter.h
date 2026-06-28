#pragma once

#include "CoreMinimal.h"
#include "Enemies/Formation/EnemySubFormationBase.h"
#include "EnemySubFormation_Scatter.generated.h"

// =========================================================================
// Enemy Scatter Facing Mode
//
// Scatterスロットの向き。
// =========================================================================
UENUM(BlueprintType)
enum class EEnemyScatterFacingMode : uint8
{
	FormationForward UMETA(DisplayName = "Formation Forward"),
	RandomYaw UMETA(DisplayName = "Random Yaw"),
	Outward UMETA(DisplayName = "Outward"),
	Center UMETA(DisplayName = "Center")
};

// =========================================================================
// Enemy SubFormation - Scatter
//
// SubFormation 기준점 주변에 슬롯을 랜덤하게 흩뿌린다.
// 후보 지점 중 기존 슬롯과 가장 멀리 떨어진 지점을 우선 사용한다.
//
// SubFormation基準点の周囲にスロットをランダムに散らす。
// 候補点の中から既存スロットと最も離れた位置を優先する。
// =========================================================================
UCLASS(BlueprintType, Blueprintable, EditInlineNew, DefaultToInstanced, meta = (DisplayName = "Scatter"))
class TACTICALAI_API UEnemySubFormation_Scatter : public UEnemySubFormationBase
{
	GENERATED_BODY()

public:
	virtual TArray<FEnemyFormationSlot> BuildSlots_Implementation(
		const FTransform& SubFormationWorldTransform,
		int32 SlotCount) const override;

private:
	FVector PickScatterLocation(
		FRandomStream& RandomStream,
		const TArray<FVector>& ExistingLocations,
		const FVector& Center,
		const FVector& ForwardVector,
		const FVector& RightVector) const;

	FVector MakeRandomPointInCircle(
		FRandomStream& RandomStream,
		const FVector& Center,
		const FVector& ForwardVector,
		const FVector& RightVector) const;

	float CalculateNearestDistanceSquared(
		const FVector& CandidateLocation,
		const TArray<FVector>& ExistingLocations) const;

	FQuat MakeSlotRotation(
		FRandomStream& RandomStream,
		const FTransform& SubFormationWorldTransform,
		const FVector& SlotLocation) const;

private:
	// 흩뿌릴 범위의 반경.
	// 散らす範囲の半径。
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Setting",
		meta = (AllowPrivateAccess = "true", ClampMin = "0.0", DisplayName = "Scatter Radius"))
	float ScatterRadius = 500.0f;

	// 슬롯 사이에 확보하고 싶은 최소 거리.
	// スロット同士で確保したい最小距離。
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Setting",
		meta = (AllowPrivateAccess = "true", ClampMin = "0.0", DisplayName = "Min Distance"))
	float MinDistance = 150.0f;

	// 슬롯 하나를 고를 때 검사할 후보 수. 높을수록 덜 뭉치지만 비용이 증가한다.
	// 1スロットを選ぶ際に試す候補数。高いほど偏りにくいがコストが増える。
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Setting",
		meta = (AllowPrivateAccess = "true", ClampMin = "1", ClampMax = "128", DisplayName = "Candidate Count"))
	int32 CandidateCount = 12;

	// true면 같은 Seed로 항상 같은 배치를 만든다.
	// trueの場合、同じSeedで常に同じ配置を作る。
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Setting",
		meta = (AllowPrivateAccess = "true", DisplayName = "Use Fixed Seed"))
	bool bUseFixedSeed = false;

	// 고정 랜덤 Seed.
	// 固定ランダムSeed。
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Setting",
		meta = (AllowPrivateAccess = "true", EditCondition = "bUseFixedSeed", DisplayName = "Random Seed"))
	int32 RandomSeed = 12345;

	// 슬롯이 바라볼 방향.
	// スロットが向く方向。
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Setting",
		meta = (AllowPrivateAccess = "true", DisplayName = "Facing Mode"))
	EEnemyScatterFacingMode FacingMode = EEnemyScatterFacingMode::FormationForward;
};