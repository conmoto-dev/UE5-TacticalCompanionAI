#pragma once

#include "CoreMinimal.h"
#include "Enemies/Formations/EnemyFormationSource.h"
#include "EnemyEncounterTypes.generated.h"

class AEnemyCharacter;

// =========================================================================
// Enemy Encounter에서 스폰할 적 항목.
//
// Override Formation이 없을 때는 EnemyClass별로 기본 Formation 그룹을 만든다.
// Override Formation이 있을 때는 OverrideInputNumber를 사용해
// 이 항목의 적들을 특정 입력 Bucket으로 보낸다.
//
// 같은 EnemyClass를 여러 항목으로 나누면,
// 같은 종류의 적도 서로 다른 Override 입력으로 배치할 수 있다.
//
// Enemy Encounterでスポーンする敵項目。
//
// Override Formationが無い場合は、EnemyClassごとに
// デフォルトFormationグループを作る。
//
// Override Formationがある場合は、OverrideInputNumberを使って、
// この項目の敵を特定の入力Bucketへ送る。
//
// 同じEnemyClassを複数項目に分けることで、
// 同種の敵でも別々のOverride入力へ配置できる。
// =========================================================================
USTRUCT(BlueprintType)
struct TACTICALAI_API FEnemyEncounterSpawnEntry
{
	GENERATED_BODY()

public:
	// =========================================================================
	// Override 입력 번호를 0-based Bucket Index로 변환한다.
	//
	// 에디터에서는 디자이너가 읽기 쉬운 1번, 2번 입력으로 표시하고,
	// 내부 처리에서는 배열 접근에 맞춰 0-based Index를 사용한다.
	//
	// Override入力番号を0-based Bucket Indexへ変換する。
	//
	// Editor上では読みやすい1番・2番入力として扱い、
	// 内部処理では配列アクセス用の0-based Indexを使用する。
	// =========================================================================
	int32 GetOverrideBucketIndex() const
	{
		return FMath::Max(0, OverrideInputNumber - 1);
	}

public:
	// =========================================================================
	// 스폰할 적 BP 클래스.
	//
	// Override가 없을 때는 이 클래스가 기본 그룹 분리 기준이 된다.
	// 같은 BP 클래스는 같은 적 종류로 취급한다.
	//
	// スポーンする敵BPクラス。
	//
	// Overrideが無い場合、このクラスがデフォルトグループの
	// 分割基準となる。
	// 同じBPクラスは同じ敵種として扱う。
	// =========================================================================
	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		Category = "敵Encounter",
		meta = (
			DisplayName = "敵Class",
			AllowAbstract = "false"))
	TSubclassOf<AEnemyCharacter> EnemyClass = nullptr;

	// =========================================================================
	// 이 항목에서 스폰할 적 수.
	// 0개 항목은 의도 추적이 어려우므로 작성 단계에서 허용하지 않는다.
	//
	// この項目でスポーンする敵数。
	// 0体の項目は意図を追いにくいため、作成時点では許可しない。
	// =========================================================================
	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		Category = "敵Encounter",
		meta = (
			ClampMin = "1",
			UIMin = "1",
			DisplayName = "数"))
	int32 Count = 1;

	// =========================================================================
	// Override Formation이 있을 때 사용할 입력 번호.
	//
	// 1이면 첫 번째 입력 Bucket, 2이면 두 번째 입력 Bucket을 뜻한다.
	// Override가 없을 때는 이 값은 무시된다.
	//
	// 예:
	// - C의 1번 입력: 전열
	// - C의 2번 입력: 후열
	//
	// Override Formationがある場合に使用する入力番号。
	//
	// 1なら1番目の入力Bucket、2なら2番目の入力Bucketを意味する。
	// Overrideが無い場合、この値は無視される。
	// =========================================================================
	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		Category = "敵Encounter",
		meta = (
			ClampMin = "1",
			UIMin = "1",
			DisplayName = "Override入力番号"))
	int32 OverrideInputNumber = 1;
};

// =========================================================================
// Enemy Encounter의 스폰 및 Formation Override 작성 데이터.
//
// 이 구조체는 레벨에 배치된 Spawn/Encounter Actor가 소유할 데이터다.
// 실제 적 스폰, 그룹 분리, Coordinator 생성은 여기서 수행하지 않는다.
//
// FormationOverride가 None이면 각 EnemyClass의 기본 Formation을 사용한다.
// Profile 또는 Inline이면 SpawnEntries의 OverrideInputNumber에 따라
// 하나의 Override Formation 입력으로 조립한다.
//
// Enemy EncounterのスポーンおよびFormation Override作成データ。
//
// この構造体は、レベル上に配置されたSpawn/Encounter Actorが
// 所有するデータである。
// 実際の敵スポーン・グループ分割・Coordinator生成はここでは行わない。
//
// FormationOverrideがNoneの場合、各EnemyClassの
// デフォルトFormationを使用する。
// ProfileまたはInlineの場合、SpawnEntriesのOverrideInputNumberに従って
// 1つのOverride Formation入力として組み立てる。
// =========================================================================
USTRUCT(BlueprintType)
struct TACTICALAI_API FEnemyEncounterSpec
{
	GENERATED_BODY()

public:
	// =========================================================================
	// 이 Encounter가 Formation Override를 가지고 있는지 확인한다.
	//
	// false이면 Resolver는 몬스터 기본 Formation으로 fallback해야 한다.
	//
	// このEncounterがFormation Overrideを持つか確認する。
	//
	// falseの場合、Resolverは敵のデフォルトFormationへ
	// fallbackする必要がある。
	// =========================================================================
	bool HasFormationOverride() const
	{
		return FormationOverride.HasOverride();
	}

public:
	// =========================================================================
	// 이 Encounter에서 생성할 적 목록.
	//
	// Override가 없으면 EnemyClass별 그룹 분리의 입력이 된다.
	// Override가 있으면 OverrideInputNumber별 Bucket 구성의 입력이 된다.
	//
	// このEncounterで生成する敵一覧。
	//
	// Overrideが無い場合はEnemyClass別グループ分割の入力となる。
	// Overrideがある場合はOverrideInputNumber別Bucket構成の入力となる。
	// =========================================================================
	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		Category = "敵Encounter",
		meta = (
			DisplayName = "スポーン項目",
			TitleProperty = "EnemyClass"))
	TArray<FEnemyEncounterSpawnEntry> SpawnEntries;

	// =========================================================================
	// 이 Encounter 전용 Formation Override.
	//
	// None이면 몬스터 종류별 기본 Formation을 사용한다.
	// Profile이면 재사용 가능한 Formation Profile을 사용한다.
	// Inline이면 이 Encounter 안에서 직접 편집한 Strategy를 사용한다.
	//
	// このEncounter専用のFormation Override。
	//
	// Noneの場合は敵種ごとのデフォルトFormationを使用する。
	// Profileの場合は再利用可能なFormation Profileを使用する。
	// Inlineの場合はこのEncounter内で直接編集したStrategyを使用する。
	// =========================================================================
	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		Category = "敵Encounter",
		meta = (DisplayName = "Formation Override"))
	FEnemyFormationSource FormationOverride;
};