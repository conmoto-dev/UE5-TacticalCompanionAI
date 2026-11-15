#pragma once

#include "CoreMinimal.h"
#include "Enemies/Formation/EnemyFormationTypes.h"
#include "Enemies/Formation/EnemySubFormationBase.h"
#include "Enemies/Encounter/EnemyEncounterSettings.h"
#include "GameFramework/Actor.h"
#include "EnemySpawner.generated.h"

class AEnemyCharacter;
class AEnemyEncounter;
class UArrowComponent;
class USceneComponent;

// ==================================================
// Enemy Spawn Mode
//
// 단일 적 생성 또는 그룹 생성을 선택한다.
// 単体の敵生成またはグループ生成を選択する。
// ==================================================
UENUM(BlueprintType)
enum class EEnemySpawnMode : uint8
{
	Group UMETA(DisplayName = "Group"),
	Single UMETA(DisplayName = "Single")
};

// =========================================================================
// Enemy Spawner
//
// 적의 단일·그룹 생성과 초기 배치를 관리하는 Actor.
// 敵の単体・グループ生成と初期配置を管理するActor。
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
	void SpawnSingleEnemy();
	void SpawnEnemyGroup();
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

	void RegisterSpawnedEnemiesToEncounter();

	
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

	// 단일 적 생성 또는 그룹 생성. Group은 기존 Formation 설정을 사용한다.
	// 単体生成またはグループ生成。Groupでは既存のFormation設定を使用する。
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy Spawn",
		meta = (AllowPrivateAccess = "true", DisplayName = "Spawn Mode"))
	EEnemySpawnMode SpawnMode = EEnemySpawnMode::Group;

	// Single 모드에서 생성할 적 클래스. 생성 요청마다 단일 적을 생성한다.
	// Singleモードで生成する敵クラス。生成要求ごとに敵単体生成する。
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy Spawn",
		meta = (
			AllowPrivateAccess = "true",
			EditCondition = "SpawnMode == EEnemySpawnMode::Single",
			EditConditionHides,
			DisplayName = "Enemy Class"))
	TSubclassOf<AEnemyCharacter> SingleEnemyClass = nullptr;

	// 그룹으로 생성할 적들의 초기 배치 방식.
	// グループとして生成する敵の初期配置方式。
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy Spawn",
		meta = (
			AllowPrivateAccess = "true",
			EditCondition = "SpawnMode == EEnemySpawnMode::Group",
			EditConditionHides,
			DisplayName = "Formation Mode"))
	EEnemyFormationMode FormationMode = EEnemyFormationMode::ByEnemyClass;

	// Group의 ByEnemyClass 방식에서 사용할 스폰 목록.
	// GroupのByEnemyClass方式で使用するスポーンリスト。
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy Spawn",
		meta = (
			AllowPrivateAccess = "true",
			EditCondition = "SpawnMode == EEnemySpawnMode::Group && FormationMode == EEnemyFormationMode::ByEnemyClass",
			EditConditionHides,
			DisplayName = "Spawn Entries"))
	TArray<FEnemySpawnEntry> SpawnEntries;

	// Group의 CompositeFormation 방식에서 직접 편집하는 복합 진형.
	// GroupのCompositeFormation方式で直接編集する複合陣形。
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy Spawn",
		meta = (
			AllowPrivateAccess = "true",
			EditCondition = "SpawnMode == EEnemySpawnMode::Group && FormationMode == EEnemyFormationMode::CompositeFormation",
			EditConditionHides,
			DisplayName = "Composite Formation"))
	FEnemyCompositeFormation CompositeFormation;

	// ==================================================
	// Encounter
	// ==================================================

	// 인카운터 생성 시 전달할 교전 설정. Single과 Group에 공통으로 적용한다.
	// エンカウンター生成時に渡す交戦設定。SingleとGroupに共通で適用する。
	UPROPERTY(EditAnywhere, Category = "Enemy Encounter",
		meta = (DisplayName = "Encounter Settings"))
	FEnemyEncounterSettings EncounterSettings;

	// 이 스포너가 생성한 인카운터. 추가 생성한 적도 같은 인카운터에 등록한다.
	// このSpawnerが生成したエンカウンター。追加生成した敵も同じエンカウンターに登録する。
	UPROPERTY(Transient, VisibleInstanceOnly, Category = "Enemy Encounter",
		meta = (AllowPrivateAccess = "true"))
	TObjectPtr<AEnemyEncounter> SpawnedEncounter = nullptr;
	
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

	// Group의 ByEnemyClass 방식에서 배치 슬롯 사이에 둘 간격.
	// GroupのByEnemyClass方式で配置スロット間に設ける間隔。
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy Debug",
		meta = (
			AllowPrivateAccess = "true",
			EditCondition = "SpawnMode == EEnemySpawnMode::Group && FormationMode == EEnemyFormationMode::ByEnemyClass",
			EditConditionHides,
			ClampMin = "0.0",
			DisplayName = "Fallback Spawn Spacing"))
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