#pragma once

#include "CoreMinimal.h"
#include "Enemies/Formation/EnemySubFormationBase.h"
#include "EnemySubFormation_Line.generated.h"

// =========================================================================
// Enemy SubFormation - Line
//
// SubFormation 기준 Y축 방향으로 슬롯을 일렬 배치한다.
// SubFormation基準のY軸方向にスロットを一列で配置する。
// =========================================================================
UCLASS(BlueprintType, Blueprintable, EditInlineNew, DefaultToInstanced, meta = (DisplayName = "Line"))
class TACTICALAI_API UEnemySubFormation_Line : public UEnemySubFormationBase
{
	GENERATED_BODY()

public:
	virtual TArray<FEnemyFormationSlot> BuildSlots_Implementation(
		const FTransform& SubFormationWorldTransform,
		int32 SlotCount) const override;

private:
	// 슬롯 사이 간격.
	// スロット同士の間隔。
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Setting",
		meta = (AllowPrivateAccess = "true", ClampMin = "0.0", DisplayName = "Slot Spacing"))
	float SlotSpacing = 150.0f;

	// true면 기준점을 중심으로 좌우 배치한다.
	// trueの場合、基準点を中心に左右へ配置する。
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Setting",
		meta = (AllowPrivateAccess = "true", DisplayName = "Center Aligned"))
	bool bCenterAligned = true;
};