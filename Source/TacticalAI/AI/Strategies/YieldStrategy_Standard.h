// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "YieldStrategy.h"
#include "YieldStrategy_Standard.generated.h"

/**
 * Standard yield: cone-based detection + side-step + projected backward offset.
 * Designed for wide formations (V/Y/etc) with enough space for occupants to
 * step aside laterally.
 *
 * 標準Yield: コーン判定 + 横方向退避 + 速度射影による後方ブレンド。
 * 横にスペースのある広い隊形（V/Y等）向け。
 */
UCLASS(BlueprintType, DisplayName = "Yield - Standard (Cone + Backward)")
class TACTICALAI_API UYieldStrategy_Standard : public UYieldStrategy
{
	GENERATED_BODY()
	
public:
	// ───── Detection Parameters ─────

	UPROPERTY(EditAnywhere, Category = "Detection", meta = (ClampMin = "0.0"))
	float YieldEnterRadius = 500.f;

	/** Must be >= YieldEnterRadius (auto-corrected if not).
	 *  Forms hysteresis to prevent immediate re-exit after Yielding entry. */
	UPROPERTY(EditAnywhere, Category = "Detection", meta = (ClampMin = "0.0"))
	float YieldExitRadius = 700.f;

	UPROPERTY(EditAnywhere, Category = "Detection", meta = (ClampMin = "0.0", ClampMax = "180.0"))
	float YieldConeHalfAngleDeg = 30.f;

	/** Reaction time delay before triggering Yield. */
	UPROPERTY(EditAnywhere, Category = "Detection", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float YieldEntryDelay = 0.1f;

	// ───── Geometry Parameters ─────

	UPROPERTY(EditAnywhere, Category = "Geometry", meta = (ClampMin = "0.0"))
	float YieldSideDistance = 200.f;

	UPROPERTY(EditAnywhere, Category = "Geometry", meta = (ClampMin = "0.0"))
	float YieldBackwardFactor = 0.25f;

	// ───── Strategy Interface ─────

	virtual bool ShouldYieldForSlot(const TScriptInterface<IYieldContextProvider>& Context, int32 SlotIdx) const override;
	
	virtual bool ShouldExitYieldForSlot(const TScriptInterface<IYieldContextProvider>& Context, int32 SlotIdx) const override;
	
	virtual bool TryCalculateYieldLocationForSlot(const TScriptInterface<IYieldContextProvider>& Context, int32 SlotIdx, FVector& OutLocation) const override;
	
	virtual float GetEntryDelay() const override { return YieldEntryDelay; }

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
	
private:
	bool TryProjectToNavMesh(UWorld* World, const FVector& Point, FVector& OutResult) const;
};