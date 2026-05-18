// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "YieldContextProvider.generated.h"

class APartyCharacter;
class APawn;

/**
 * UInterface boilerplate for Blueprint/Reflection support.
 * Blueprint・リフレクション対応用のボイラープレート。
 */
UINTERFACE(MinimalAPI, BlueprintType)
class UYieldContextProvider : public UInterface
{
	GENERATED_BODY()
};

/**
 * Abstract data interface for Yield strategies.
 *
 * Any component using UYieldStrategy implements this interface to expose
 * the minimum data required for yield decisions and calculations.
 * Strategies receive this interface at call time and never hold references
 * (stateless design).
 *
 * Yield Strategyのための抽象データインターフェース。
 * UYieldStrategyを使う全Componentがこれを実装する。
 * Strategy側はComponentの具体型を知らず、Asset共有時の衝突も発生しない。
 */
class TACTICALAI_API IYieldContextProvider
{
	GENERATED_BODY()

public:
	/** Number of slots currently managed. */
	/** 現在管理中のスロット数。 */
	virtual int32 GetSlotCount() const = 0;

	/** Occupant character at the given slot index, or nullptr if vacant/invalid. */
	/** 指定スロットの占有キャラ。空席や不正indexはnullptr。 */
	virtual APartyCharacter* GetOccupantAt(int32 SlotIdx) const = 0;

	/** World-space cached slot location at the given index. */
	/** 指定スロットのワールド座標キャッシュ。 */
	virtual FVector GetSlotLocationAt(int32 SlotIdx) const = 0;

	/** The pawn to yield against (typically the player). */
	/** Yield対象のPawn（通常はプレイヤー）。 */
	virtual APawn* GetTargetPawn() const = 0;
};