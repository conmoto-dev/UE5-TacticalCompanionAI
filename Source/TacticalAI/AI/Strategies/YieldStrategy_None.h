// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AI/Strategies/YieldStrategy.h"
#include "YieldStrategy_None.generated.h"

/**
 * Explicit "no yield" strategy. Inherits base class defaults (all yield checks
 * return false / true-for-exit). Use this to clearly express "this formation
 * intentionally does not yield" rather than leaving YieldStrategy nullptr.
 *
 * 明示的な「Yieldなし」戦略。ベースクラスのデフォルト動作をそのまま使用。
 * nullptr放置ではなく「意図的にYieldさせない」を表現。
 */
UCLASS(BlueprintType, DisplayName = "Yield - None")
class TACTICALAI_API UYieldStrategy_None : public UYieldStrategy
{
	GENERATED_BODY()
};