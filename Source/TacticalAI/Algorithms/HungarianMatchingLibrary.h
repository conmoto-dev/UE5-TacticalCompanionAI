// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "HungarianMatchingLibrary.generated.h"

/**
 * Wrapper for a single row of the cost matrix.
 * UE Reflection doesn't support nested TArray as UPROPERTY, so we wrap each row.
 * UEのReflectionは多次元TArrayをUPROPERTYで扱えないため、行ごとに構造体でラップ。
 */
USTRUCT(BlueprintType)
struct FCostMatrixRow
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Matching")
	TArray<float> Values;
};

/**
 * Hungarian algorithm utilities for optimal bipartite matching.
 * Used for assigning N agents to N slots with minimum total cost.
 *
 * 古典的なハンガリアン法。エージェントとスロットの最適マッチング問題を解く。
 * 隊形変更時の仲間-スロット再割り当て、停止時の自然な位置取り等で使用。
 */
UCLASS()
class TACTICALAI_API UHungarianMatchingLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Solve the assignment problem to minimize total cost.
	 *
	 * @param CostMatrix N×N matrix where CostMatrix[i].Values[j] is the cost of assigning agent i to slot j.
	 * @return Assignment array where Assignment[i] = slot index for agent i. Empty on invalid input.
	 */
	UFUNCTION(BlueprintCallable, Category = "Algorithms|Matching",
			  meta = (Tooltip = "Hungarian algorithm: minimum-cost bipartite matching"))
	static TArray<int32> SolveAssignment(const TArray<FCostMatrixRow>& CostMatrix);
};