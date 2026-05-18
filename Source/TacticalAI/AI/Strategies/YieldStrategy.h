// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "UObject/ScriptInterface.h"
#include "YieldContextProvider.h"
#include "YieldStrategy.generated.h"

/**
 * Abstract Yield strategy. Defines the contract for how a formation handles
 * companion yielding to player passage. Concrete implementations (e.g.,
 * UYieldStrategy_Standard, UYieldStrategy_None) live in separate files.
 *
 * Stateless: per-slot state (timers, current yield location) is owned by
 * the Component side. Strategy methods receive context via interface and
 * never hold references between calls.
 *
 * Asset公有時の衝突を避けるためStrategyは状態を持たない。
 * スロット別の状態（タイマー、Yield座標）はComponent側で管理。
 * Strategyは判定と座標算出のロジックのみ提供。
 */
UCLASS(Abstract, EditInlineNew, Blueprintable, BlueprintType)
class TACTICALAI_API UYieldStrategy : public UObject
{
	GENERATED_BODY()

public:
	/** Returns true if the occupant of this slot should enter (or stay in) Yielding. */
	/** このスロットの占有者がYielding状態に入る／留まるべきか判定。 */
	virtual bool ShouldYieldForSlot(
		const TScriptInterface<IYieldContextProvider>& Context, int32 SlotIdx) const
	{
		return false;
	}

	/** Returns true if the occupant should exit Yielding back to Following. */
	/** Yielding状態を抜けてFollowingに戻るべきか判定。 */
	virtual bool ShouldExitYieldForSlot(
		const TScriptInterface<IYieldContextProvider>& Context, int32 SlotIdx) const
	{
		return true;
	}

	/** Computes the world-space yield target with NavMesh validation.
	 *  Returns false if no valid spot found. */
	/** Yield目標を算出（NavMesh検証含む）。退避不可ならfalse。 */
	virtual bool TryCalculateYieldLocationForSlot(
		const TScriptInterface<IYieldContextProvider>& Context, int32 SlotIdx, FVector& OutLocation) const
	{
		return false;
	}

	/** Returns the entry delay (seconds) before transitioning to Yielding. */
	/** Yielding遷移までの進入遅延（秒）。 */
	virtual float GetEntryDelay() const { return 0.f; }
};