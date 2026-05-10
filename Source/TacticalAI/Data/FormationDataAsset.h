// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "FormationDataAsset.generated.h"

/**
 * Single slot definition within a formation.
 * Wrapped in a struct (instead of bare FVector) so we can extend with priority,
 * tags, or other per-slot metadata without breaking existing data assets.
 * 個別のスロットを構造体で包むことで、優先度やタグなどの追加情報を後から拡張可能。
 */
USTRUCT(BlueprintType)
struct FFormationSlotData
{
	GENERATED_BODY()

	/** Local-space offset from the leader (X = forward, Y = right, Z = up). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Formation")
	FVector LocalOffset = FVector::ZeroVector;
};

/**
 * Designer-authored formation definition.
 * Each formation (V-shape, I-shape, etc.) is a separate data asset instance.
 * Designers can create and tweak formations without touching code.
 * デザイナーがコードを触らずに新しい隊形を追加・調整できる。
 */
UCLASS(BlueprintType)
class TACTICALAI_API UFormationDataAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** Internal identifier (e.g., "VFormation", "IFormation"). Used in code lookups. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Formation")
	FName FormationName;

	/** All slot definitions for this formation. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Formation")
	TArray<FFormationSlotData> Slots;
};
