#pragma once

#include "CoreMinimal.h"
#include "Enemies/Formation/EnemySubFormationBase.h"
#include "EnemySubFormation_Circle.generated.h"

// =========================================================================
// Enemy Circle Facing Mode
//
// Circleスロットの向き。
// =========================================================================
UENUM(BlueprintType)
enum class EEnemyCircleFacingMode : uint8
{
	FaceFormationForward UMETA(DisplayName = "Formation Forward"),
	FaceOutward UMETA(DisplayName = "Outward"),
	FaceCenter UMETA(DisplayName = "Center")
};

// =========================================================================
// Enemy SubFormation - Circle
//
// SubFormation基準点を中心として円形スロットを生成する。
// デフォルトでは中心から外側を向く。
// =========================================================================
UCLASS(BlueprintType, Blueprintable, EditInlineNew, DefaultToInstanced, meta = (DisplayName = "Circle"))
class TACTICALAI_API UEnemySubFormation_Circle : public UEnemySubFormationBase
{
	GENERATED_BODY()

public:
	virtual TArray<FEnemyFormationSlot> BuildSlots_Implementation(
		const FTransform& SubFormationWorldTransform,
		int32 SlotCount) const override;

private:
	// 円の半径。
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Setting",
		meta = (AllowPrivateAccess = "true", ClampMin = "0.0", DisplayName = "Radius"))
	float Radius = 300.0f;
	
	// 最初のスロットの開始角度。0度はSubFormation基準の+X方向。
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Setting",
		meta = (AllowPrivateAccess = "true", DisplayName = "Start Angle"))
	float StartAngleDegrees = 0.0f;
	
	// trueの場合、時計回りに配置する。
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Setting",
		meta = (AllowPrivateAccess = "true", DisplayName = "Clockwise"))
	bool bClockwise = false;
	
	// スロットが向く方向。
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Setting",
		meta = (AllowPrivateAccess = "true", DisplayName = "Facing Mode"))
	EEnemyCircleFacingMode FacingMode = EEnemyCircleFacingMode::FaceOutward;
};