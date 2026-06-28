#pragma once

#include "CoreMinimal.h"
#include "Enemies/Formation/EnemyFormationTypes.h"
#include "UObject/Object.h"
#include "EnemySubFormationBase.generated.h"

// =========================================================================
// Enemy SubFormation Base
//
// SubFormation 안에서 필요한 수만큼 슬롯을 생성한다.
// 기준 Transform은 호출자가 정해서 넘긴다.
//
// SubFormation内で必要な数のスロットを生成する。
// 基準Transformは呼び出し側が決めて渡す。
// =========================================================================
UCLASS(Abstract, BlueprintType, Blueprintable, EditInlineNew, DefaultToInstanced)
class TACTICALAI_API UEnemySubFormationBase : public UObject
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Enemy AI|SubFormation")
	TArray<FEnemyFormationSlot> BuildSlots(
		const FTransform& SubFormationWorldTransform,
		int32 SlotCount) const;

	virtual TArray<FEnemyFormationSlot> BuildSlots_Implementation(
		const FTransform& SubFormationWorldTransform,
		int32 SlotCount) const;
};