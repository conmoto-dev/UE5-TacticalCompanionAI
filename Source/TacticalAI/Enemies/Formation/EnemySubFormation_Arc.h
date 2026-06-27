#pragma once

#include "CoreMinimal.h"
#include "Enemies/Formation/EnemySubFormationStrategy.h"
#include "EnemySubFormation_Arc.generated.h"

// =========================================================================
// Enemy Arc Facing Mode
//
// Arcスロットの向き。
// =========================================================================
UENUM(BlueprintType)
enum class EEnemyArcFacingMode : uint8
{
	FaceFormationForward UMETA(DisplayName = "Formation Forward"),
	FaceOutward UMETA(DisplayName = "Outward"),
	FaceCenter UMETA(DisplayName = "Center"),
	FaceTangent UMETA(DisplayName = "Tangent")
};

// =========================================================================
// Enemy SubFormation - Arc
//
// SubFormation 기준점을 중심으로 지정 각도 범위 안에 슬롯을 호 형태로 배치한다.
// SlotCount가 1이면 Center Angle 위치에 하나만 배치한다.
//
// SubFormation基準点を中心として、指定角度範囲内にスロットを弧状に配置する。
// SlotCountが1の場合はCenter Angleの位置に1つだけ配置する。
// =========================================================================
UCLASS(BlueprintType, Blueprintable, EditInlineNew, DefaultToInstanced, meta = (DisplayName = "Arc"))
class TACTICALAI_API UEnemySubFormation_Arc : public UEnemySubFormationStrategy
{
	GENERATED_BODY()

public:
	virtual TArray<FEnemyFormationSlot> BuildSlots_Implementation(
		const FTransform& SubFormationWorldTransform,
		int32 SlotCount) const override;

private:
	// 호의 반지름.
	// 弧の半径。
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Setting",
		meta = (AllowPrivateAccess = "true", ClampMin = "0.0", DisplayName = "Radius"))
	float Radius = 400.0f;

	// 호의 중앙 각도. 0도는 SubFormation 기준 +X 방향.
	// 弧の中央角度。0度はSubFormation基準の+X方向。
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Setting",
		meta = (AllowPrivateAccess = "true", DisplayName = "Center Angle"))
	float CenterAngleDegrees = 0.0f;

	// 슬롯을 펼칠 전체 각도.
	// スロットを広げる全体角度。
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Setting",
		meta = (AllowPrivateAccess = "true", ClampMin = "0.0", ClampMax = "360.0", DisplayName = "Arc Angle"))
	float ArcAngleDegrees = 90.0f;

	// true면 슬롯 순서를 반대 방향으로 배치한다.
	// trueの場合、スロットの配置順を逆方向にする。
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Setting",
		meta = (AllowPrivateAccess = "true", DisplayName = "Clockwise"))
	bool bClockwise = false;

	// 슬롯이 바라볼 방향.
	// スロットが向く方向。
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Setting",
		meta = (AllowPrivateAccess = "true", DisplayName = "Facing Mode"))
	EEnemyArcFacingMode FacingMode = EEnemyArcFacingMode::FaceFormationForward;
};