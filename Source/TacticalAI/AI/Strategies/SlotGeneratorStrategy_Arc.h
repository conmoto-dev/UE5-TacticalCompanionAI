#pragma once

#include "CoreMinimal.h"
#include "AI/Strategies/SlotGeneratorStrategy.h"
#include "SlotGeneratorStrategy_Arc.generated.h"

/**
 * 협공 호(arc) 배치. anchor 로컬 +X를 호 중심으로, ±ArcAngle/2 구간을 N등분.
 * 적 1마리면 전원 동일 타겟 → 협공, 여럿이면 타겟별로 N이 갈려 분산.
 * (밀링턴 Scalable Formation: 모양 유지 + 인원에 따라 동적 분할.)
 *
 * 囲み込みのアーク配置。ローカル+Xを中心に±ArcAngle/2をN等分。
 */
UCLASS(BlueprintType, DisplayName = "Slot Gen - Arc (협공 호)")
class TACTICALAI_API USlotGeneratorStrategy_Arc : public USlotGeneratorStrategy
{
	GENERATED_BODY()

public:
	/** 호가 펼쳐지는 총 각도(도). 좁으면 뭉치고 넓으면 펼쳐짐. */
	UPROPERTY(EditAnywhere, Category = "Arc", meta = (ClampMin = "0.0", ClampMax = "360.0"))
	float ArcAngleDeg = 120.f;

	virtual void GenerateSlots(int32 NumSlots, float BaseRadius, TArray<FVector>& OutLocalOffsets) const override;
};