#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Enemies/Encounters/EnemyEncounterTypes.h"
#include "Enemies/Encounters/EnemyTacticalGroupSeed.h"
#include "EnemyEncounterMarker.generated.h"

class USceneComponent;

// =========================================================================
// 레벨에 배치하는 Enemy Encounter 작성용 Marker.
//
// 이 Actor는 Encounter 작성 데이터를 소유하고,
// 해당 데이터가 어떤 전술 그룹 Seed로 해석되는지 검증한다.
//
// 실제 Enemy 스폰, Tactical Coordinator 생성, Target 결정,
// Slot Assignment, 이동 명령은 아직 수행하지 않는다.
//
// レベル上に配置するEnemy Encounter作成用Marker。
//
// このActorはEncounter作成データを保持し、
// そのデータがどの戦術グループSeedへ解釈されるかを検証する。
//
// 実際のEnemy Spawn・Tactical Coordinator生成・Target決定・
// Slot Assignment・移動命令はまだ行わない。
// =========================================================================
UCLASS()
class TACTICALAI_API AEnemyEncounterMarker : public AActor
{
	GENERATED_BODY()

public:
	AEnemyEncounterMarker();

	// =========================================================================
	// 현재 Encounter 설정을 GroupSeed 배열로 해석한다.
	//
	// 이 함수는 실제 Actor를 스폰하지 않는다.
	// Spawner/Coordinator가 사용할 사전 해석 결과만 만든다.
	//
	// 現在のEncounter設定をGroupSeed配列へ解釈する。
	//
	// この関数は実際のActorをスポーンしない。
	// Spawner/Coordinatorが使用する事前解釈結果だけを生成する。
	// =========================================================================
	bool TryResolveGroupSeeds(
		TArray<FEnemyTacticalGroupSeed>& OutGroupSeeds,
		FString* OutError = nullptr) const;

	// =========================================================================
	// 에디터 Details 패널에서 Encounter 설정을 검증한다.
	// 성공 시 생성될 GroupSeed 요약을 로그로 출력한다.
	//
	// Editor DetailsからEncounter設定を検証する。
	// 成功時は生成されるGroupSeedの概要をログ出力する。
	// 失敗時はエラーメッセージをログ出力する。
	// =========================================================================
	UFUNCTION(
		CallInEditor,
		Category = "敵Encounter",
		meta = (DisplayName = "Encounter検証"))
	void ValidateEncounter();

	// =========================================================================
	// Encounter 작성 데이터 조회.
	//
	// 外部のSpawner/Debug表示がEncounter作成データを読むためのGetter。
	// =========================================================================
	const FEnemyEncounterSpec& GetEncounterSpec() const
	{
		return EncounterSpec;
	}

protected:
	virtual void OnConstruction(
		const FTransform& Transform) override;

private:
	void LogResolvedGroupSeeds(
		const TArray<FEnemyTacticalGroupSeed>& GroupSeeds) const;

	FString DescribeFormationSource(
		const FEnemyFormationSource& Source) const;

private:
	// =========================================================================
	// Marker의 루트 컴포넌트.
	//
	// 지금은 시각적 표현 없이 Transform 기준점만 제공한다.
	// 이후 Formation Anchor의 기본 기준점으로 사용할 수 있다.
	//
	// MarkerのRoot Component。
	//
	// 現時点では視覚表現を持たず、Transform基準点だけを提供する。
	// 将来的にFormation Anchorの基本基準点として利用できる。
	// =========================================================================
	UPROPERTY(
		VisibleAnywhere,
		BlueprintReadOnly,
		Category = "敵Encounter",
		meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USceneComponent> SceneRoot = nullptr;

	// =========================================================================
	// 이 Marker가 표현하는 Encounter 작성 데이터.
	//
	// SpawnEntries와 Formation Override를 포함한다.
	// 실제 그룹 분리와 검증은 Resolver가 수행한다.
	//
	// このMarkerが表すEncounter作成データ。
	//
	// SpawnEntriesとFormation Overrideを含む。
	// 実際のグループ分割と検証はResolverが行う。
	// =========================================================================
	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		Category = "敵Encounter",
		meta = (
			AllowPrivateAccess = "true",
			DisplayName = "Encounter設定"))
	FEnemyEncounterSpec EncounterSpec;

	// =========================================================================
	// Construction Script 단계에서 Encounter 설정을 자동 검증할지 여부.
	//
	// Details 편집 중 로그가 자주 찍힐 수 있으므로 기본값은 false다.
	//
	// Construction Script段階でEncounter設定を自動検証するかどうか。
	//
	// Details編集中にログが多く出る可能性があるため、デフォルトはfalse。
	// =========================================================================
	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		Category = "敵Encounter|Debug",
		meta = (
			AllowPrivateAccess = "true",
			DisplayName = "Construction時に自動検証"))
	bool bValidateOnConstruction = false;

	// =========================================================================
	// Validate 실행 시 GroupSeed 상세 로그를 출력할지 여부.
	//
	// Resolver가 어떤 Bucket과 EnemyClass 수량을 만들었는지 확인할 때 사용한다.
	//
	// Validate実行時にGroupSeed詳細ログを出力するかどうか。
	//
	// Resolverが生成したBucketとEnemyClass数を確認するために使用する。
	// =========================================================================
	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		Category = "敵Encounter|Debug",
		meta = (
			AllowPrivateAccess = "true",
			DisplayName = "GroupSeed詳細ログ"))
	bool bLogResolvedGroupsOnValidation = true;
};