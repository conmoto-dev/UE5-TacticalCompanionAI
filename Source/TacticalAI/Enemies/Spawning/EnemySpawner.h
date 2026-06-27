#pragma once

#include "CoreMinimal.h"
#include "Enemies/Formation/EnemySubFormationStrategy.h"
#include "GameFramework/Actor.h"
#include "EnemySpawner.generated.h"

class UArrowComponent;
class USceneComponent;

// =========================================================================
// Enemy Spawner
//
// 敵グループの初期スポーン位置を管理するActor。
// =========================================================================
UCLASS(Blueprintable)
class TACTICALAI_API AEnemySpawner : public AActor
{
	GENERATED_BODY()

public:
	AEnemySpawner();

	virtual void BeginPlay() override;

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Debug Test Spawn")
	void SpawnEnemies();

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Debug Test Spawn")
	void ClearSpawnedEnemies();

private:
	void SpawnByEnemyClass();
	void SpawnByCompositeFormation();

	TArray<FEnemyFormationSlot> BuildFallbackSlots(int32 SlotCount) const;

	void SpawnEntryListAtSlots(
		const TArray<FEnemySpawnEntry>& Entries,
		const TArray<FEnemyFormationSlot>& Slots,
		const FString& ContextName);

	AEnemyCharacter* SpawnEnemyAtSlot(
		TSubclassOf<AEnemyCharacter> EnemyClass,
		const FEnemyFormationSlot& Slot);

	void DrawDebugSlot(const FEnemyFormationSlot& Slot) const;

	int32 GetTotalSpawnCount(const TArray<FEnemySpawnEntry>& Entries) const;

private:
	// =========================================================================
	// Components
	// =========================================================================
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components",
		meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USceneComponent> SceneRoot = nullptr;

#if WITH_EDITORONLY_DATA
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components",
		meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UArrowComponent> EditorArrow = nullptr;
#endif

	// =========================================================================
	// Spawn
	// =========================================================================
	// BeginPlay時に自動スポーンするか。
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy AI|Spawn",
		meta = (AllowPrivateAccess = "true", DisplayName = "Spawn On BeginPlay"))
	bool bSpawnOnBeginPlay = true;

	// 敵の初期配置モード。
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy AI|Spawn",
		meta = (AllowPrivateAccess = "true", DisplayName = "Formation Mode"))
	EEnemyFormationMode FormationMode = EEnemyFormationMode::ByEnemyClass;

	// ByEnemyClassモードで使用する敵リスト。
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy AI|Spawn",
		meta = (
			AllowPrivateAccess = "true",
			EditCondition = "FormationMode == EEnemyFormationMode::ByEnemyClass",
			EditConditionHides,
			DisplayName = "Spawn Entries"))
	TArray<FEnemySpawnEntry> SpawnEntries;

	// Spawner内で編集する複合Formation。
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy AI|Spawn",
		meta = (
			AllowPrivateAccess = "true",
			EditCondition = "FormationMode == EEnemyFormationMode::CompositeFormation",
			EditConditionHides,
			DisplayName = "Composite Formation"))
	FEnemyCompositeFormation CompositeFormation;

	// SpawnEnemies再実行時に前回生成した敵を削除するか。
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy AI|Debug",
		meta = (AllowPrivateAccess = "true", DisplayName = "Destroy Previous Spawned Enemies"))
	bool bDestroyPreviousSpawnedEnemies = true;

	// ByEnemyClassモードの仮配置間隔。
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy AI|Debug",
		meta = (AllowPrivateAccess = "true", ClampMin = "0.0", DisplayName = "Fallback Spawn Spacing"))
	float FallbackSpawnSpacing = 150.0f;

	// スロットのデバッグ表示を有効にするか。
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy AI|Debug",
		meta = (AllowPrivateAccess = "true", DisplayName = "Draw Debug Slots"))
	bool bDrawDebugSlots = true;

	// デバッグ表示時間。
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy AI|Debug",
		meta = (AllowPrivateAccess = "true", ClampMin = "0.0", DisplayName = "Debug Draw Duration"))
	float DebugDrawDuration = 5.0f;

	UPROPERTY(Transient)
	TArray<TObjectPtr<AEnemyCharacter>> SpawnedEnemies;
};