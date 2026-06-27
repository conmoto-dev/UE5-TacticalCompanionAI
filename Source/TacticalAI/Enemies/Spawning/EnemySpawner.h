#pragma once

#include "CoreMinimal.h"
#include "Enemies/Formation/EnemyFormationTypes.h"
#include "Enemies/Formation/EnemySubFormationStrategy.h"
#include "GameFramework/Actor.h"
#include "EnemySpawner.generated.h"

class AEnemyCharacter;
class UArrowComponent;
class USceneComponent;

// =========================================================================
// Enemy Spawner
//
// 적 그룹의 초기 스폰과 Formation 배치를 관리하는 Actor.
// 敵グループの初期スポーンとFormation配置を管理するActor。
// =========================================================================
UCLASS(Blueprintable)
class TACTICALAI_API AEnemySpawner : public AActor
{
	GENERATED_BODY()

public:
	AEnemySpawner();

	virtual void BeginPlay() override;

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Enemy Spawn")
	void SpawnEnemies();

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Enemy Spawn")
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
		const FEnemyFormationSlot& Slot,
		FTransform& OutResolvedSpawnTransform);

	AEnemyCharacter* TrySpawnEnemyAtTransform(
		TSubclassOf<AEnemyCharacter> EnemyClass,
		const FTransform& SpawnTransform,
		ESpawnActorCollisionHandlingMethod CollisionHandlingMethod);

	bool TryProjectSpawnLocationToNavigation(
		const FVector& SourceLocation,
		FVector& OutProjectedLocation) const;

	TArray<FTransform> BuildSpawnResolveCandidates(
		const FTransform& DesiredTransform) const;

	void DrawDebugSlot(
		const FTransform& SlotTransform,
		int32 SlotIndex) const;

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
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy Spawn",
		meta = (AllowPrivateAccess = "true", DisplayName = "Spawn On BeginPlay"))
	bool bSpawnOnBeginPlay = true;

	// 적의 초기 Formation 배치 모드.
	// 敵の初期Formation配置モード。
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy Spawn",
		meta = (AllowPrivateAccess = "true", DisplayName = "Formation Mode"))
	EEnemyFormationMode FormationMode = EEnemyFormationMode::ByEnemyClass;

	// ByEnemyClass 모드에서 사용할 기본 스폰 목록.
	// ByEnemyClassモードで使用する基本スポーンリスト。
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy Spawn",
		meta = (
			AllowPrivateAccess = "true",
			EditCondition = "FormationMode == EEnemyFormationMode::ByEnemyClass",
			EditConditionHides,
			DisplayName = "Spawn Entries"))
	TArray<FEnemySpawnEntry> SpawnEntries;

	// Spawner 안에서 직접 편집하는 복합 Formation.
	// Spawner内で直接編集する複合Formation。
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy Formation",
		meta = (
			AllowPrivateAccess = "true",
			EditCondition = "FormationMode == EEnemyFormationMode::CompositeFormation",
			EditConditionHides,
			DisplayName = "Composite Formation"))
	FEnemyCompositeFormation CompositeFormation;

	// =========================================================================
	// Spawn Resolve
	// =========================================================================
	// 벽이나 장애물에 겹친 스폰 위치를 주변 후보 위치로 보정할지 여부.
	// 壁や障害物と重なったスポーン位置を周辺候補へ補正するか。
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy Spawn",
		meta = (AllowPrivateAccess = "true", DisplayName = "Resolve Spawn Collision"))
	bool bResolveSpawnCollision = true;

	// 스폰 후보 위치를 NavMesh 위로 투영할지 여부.
	// スポーン候補位置をNavMesh上へ投影するか。
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy Spawn",
		meta = (AllowPrivateAccess = "true", DisplayName = "Project Spawn To NavMesh"))
	bool bProjectSpawnToNavMesh = true;

	// NavMesh 투영 시 사용할 탐색 범위.
	// NavMesh投影時に使用する探索範囲。
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy Spawn",
		meta = (AllowPrivateAccess = "true", DisplayName = "Nav Projection Extent"))
	FVector NavProjectionExtent = FVector(300.0f, 300.0f, 500.0f);

	// 스폰 보정 시 탐색할 최대 반경.
	// スポーン補正時に探索する最大半径。
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy Spawn",
		meta = (AllowPrivateAccess = "true", ClampMin = "0.0", DisplayName = "Resolve Max Radius"))
	float SpawnResolveMaxRadius = 600.0f;

	// 스폰 보정 시 반경을 넓혀가는 간격.
	// スポーン補正時に半径を広げる間隔。
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy Spawn",
		meta = (AllowPrivateAccess = "true", ClampMin = "1.0", DisplayName = "Resolve Radius Step"))
	float SpawnResolveRadiusStep = 100.0f;

	// 각 반경에서 검사할 방향 수.
	// 各半径で検査する方向数。
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy Spawn",
		meta = (AllowPrivateAccess = "true", ClampMin = "4", ClampMax = "32", DisplayName = "Resolve Direction Count"))
	int32 SpawnResolveDirectionCount = 12;

	// =========================================================================
	// Debug
	// =========================================================================
	// SpawnEnemies 재실행 시 이전에 생성한 적을 삭제할지 여부.
	// SpawnEnemies再実行時に前回生成した敵を削除するか。
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy Debug",
		meta = (AllowPrivateAccess = "true", DisplayName = "Destroy Previous Spawned Enemies"))
	bool bDestroyPreviousSpawnedEnemies = true;

	// ByEnemyClass 모드에서 임시 배치 슬롯 사이에 둘 간격.
	// ByEnemyClassモードで仮配置スロット同士の間に置く間隔。
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy Debug",
		meta = (AllowPrivateAccess = "true", ClampMin = "0.0", DisplayName = "Fallback Spawn Spacing"))
	float FallbackSpawnSpacing = 150.0f;

	// 슬롯 디버그 표시를 활성화할지 여부.
	// スロットのデバッグ表示を有効にするか。
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy Debug",
		meta = (AllowPrivateAccess = "true", DisplayName = "Draw Debug Slots"))
	bool bDrawDebugSlots = true;

	// 슬롯 디버그 표시 시간.
	// スロットのデバッグ表示時間。
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy Debug",
		meta = (AllowPrivateAccess = "true", ClampMin = "0.0", DisplayName = "Debug Draw Duration"))
	float DebugDrawDuration = 5.0f;

	// =========================================================================
	// Runtime
	// =========================================================================
	UPROPERTY(Transient)
	TArray<TObjectPtr<AEnemyCharacter>> SpawnedEnemies;
};