#pragma once

#include "CoreMinimal.h"
#include "Enemies/Formations/EnemyFormationSource.h"
#include "EnemyTacticalGroupSeed.generated.h"

class AEnemyCharacter;

// =========================================================================
// 전술 그룹 Bucket 안에 들어갈 적 종류와 수.
//
// 아직 실제 Actor를 가리키지 않는다.
// Encounter 작성 데이터가 Resolver를 통과한 뒤,
// 어떤 적 종류를 몇 마리 스폰해 이 Bucket에 넣을지 표현한다.
//
// 戦術グループBucketに入る敵種と数。
//
// まだ実際のActorは参照しない。
// Encounter作成データがResolverを通過した後、
// どの敵種を何体スポーンしてこのBucketへ入れるかを表す。
// =========================================================================
USTRUCT(BlueprintType)
struct TACTICALAI_API FEnemyTacticalGroupUnitSeed
{
	GENERATED_BODY()

public:
	// =========================================================================
	// 항목이 런타임 그룹 씨앗으로 유효한지 검사한다.
	//
	// 敵種Classがあり、数が1以上であることを確認する。
	// =========================================================================
	bool IsValidSeed(FString* OutError = nullptr) const;

public:
	// 스폰할 적 BP 클래스.
	// Spawnする敵BPクラス。
	UPROPERTY(
		BlueprintReadOnly,
		Category = "敵Encounter|GroupSeed")
	TSubclassOf<AEnemyCharacter> EnemyClass = nullptr;

	// 이 Bucket에 들어갈 해당 적 종류의 수.
	// このBucketに入る該当敵種の数。
	UPROPERTY(
		BlueprintReadOnly,
		Category = "敵Encounter|GroupSeed")
	int32 Count = 0;
};

// =========================================================================
// 전술 그룹의 입력 Bucket 하나.
//
// 하나의 Bucket 안에는 여러 EnemyClass가 섞일 수 있다.
// 예를 들어 C 진형의 전열 Bucket에 A와 B를 함께 넣을 수 있다.
//
// 배열 인덱스가 Formation 입력 Bucket 인덱스와 대응한다.
// Bucket 자체는 비어 있을 수 있다. 예: [0, 4]에서 Bucket 0.
//
// 戦術グループの入力Bucket 1つ。
//
// 1つのBucket内に複数のEnemyClassを混在させることができる。
// 例えばCフォーメーションの前列BucketへAとBを同時に入れられる。
//
// 配列インデックスがFormation入力Bucketインデックスに対応する。
// Bucket自体は空でもよい。例：[0, 4]のBucket 0。
// =========================================================================
USTRUCT(BlueprintType)
struct TACTICALAI_API FEnemyTacticalGroupBucketSeed
{
	GENERATED_BODY()

public:
	// =========================================================================
	// 같은 EnemyClass가 이미 있으면 Count를 누적하고,
	// 없으면 새 항목을 추가한다.
	//
	// Resolver가 SpawnEntry를 Bucket으로 모을 때 사용한다.
	//
	// 同じEnemyClassが既にあればCountを加算し、
	// 無ければ新しい項目として追加する。
	//
	// ResolverがSpawnEntryをBucketへ集約する際に使用する。
	// =========================================================================
	bool TryAddUnit(
		TSubclassOf<AEnemyCharacter> InEnemyClass,
		int32 InCount,
		FString* OutError = nullptr);

	// =========================================================================
	// 이 Bucket에 들어갈 전체 적 수를 계산한다.
	//
	// 각 UnitSeed도 함께 검증한다.
	//
	// このBucketに入る敵の総数を計算する。
	//
	// 各UnitSeedも同時に検証する。
	// =========================================================================
	bool TryGetMemberCount(
		int32& OutMemberCount,
		FString* OutError = nullptr) const;

public:
	// Bucket 안에 들어갈 적 종류별 항목.
	// Bucket内に入る敵種別項目。
	UPROPERTY(
		BlueprintReadOnly,
		Category = "敵Encounter|GroupSeed")
	TArray<FEnemyTacticalGroupUnitSeed> Units;
};

// =========================================================================
// Enemy Tactical Coordinator를 만들기 전 단계의 그룹 씨앗.
//
// 이 구조체는 "어떤 Formation Source를 사용해",
// "각 입력 Bucket에 어떤 적을 몇 마리 넣을지"만 표현한다.
//
// 실제 Actor, Target, Formation Anchor, Slot Assignment,
// Committed Home Slot 같은 런타임 상태는 포함하지 않는다.
//
// Enemy Tactical Coordinator生成前段階のグループSeed。
//
// この構造体は「どのFormation Sourceを使用し」
// 「各入力Bucketへどの敵を何体入れるか」だけを表す。
//
// 実際のActor・Target・Formation Anchor・Slot Assignment・
// Committed Home Slotなどのランタイム状態は含めない。
// =========================================================================
USTRUCT(BlueprintType)
struct TACTICALAI_API FEnemyTacticalGroupSeed
{
	GENERATED_BODY()

public:
	// =========================================================================
	// Bucket별 전체 인원수로 Formation Layout Context를 만든다.
	//
	// 이 함수는 Layout을 직접 생성하지 않는다.
	// EffectiveFormationSource가 유효한지와 Bucket 인원수만 검증한다.
	//
	// Bucketごとの総人数からFormation Layout Contextを作る。
	//
	// この関数はLayoutを直接生成しない。
	// EffectiveFormationSourceが有効か、Bucket人数が妥当かだけを検証する。
	// =========================================================================
	bool TryBuildLayoutContext(
		FEnemyFormationLayoutContext& OutContext,
		FString* OutError = nullptr) const;

	// =========================================================================
	// 이 그룹 전체에 들어갈 적 수를 계산한다.
	//
	// デバッグや検証用に、このグループ全体の敵数を計算する。
	// =========================================================================
	bool TryGetTotalMemberCount(
		int32& OutTotalMemberCount,
		FString* OutError = nullptr) const;

public:
	// 디버그에서 식별하기 위한 이름.
	// デバッグ上で識別するための名前。
	// 戦闘ロジックの条件分岐には使用しない。
	UPROPERTY(
		BlueprintReadOnly,
		Category = "敵Encounter|GroupSeed")
	FName DebugName = NAME_None;

	// 이 그룹에 실제 적용할 Formation Source.
	//
	// Override Encounter라면 Encounter의 FormationOverride가 들어간다.
	// Override가 없다면 EnemyClass의 DefaultFormationProfile을
	// Profile Source로 변환해 넣는다.
	//
	// このグループへ実際に適用するFormation Source。
	//
	// Override Encounterの場合はEncounterのFormationOverrideが入る。
	// Overrideが無い場合はEnemyClassのDefaultFormationProfileを
	// Profile Sourceへ変換して入れる。
	UPROPERTY(
		BlueprintReadOnly,
		Category = "敵Encounter|GroupSeed")
	FEnemyFormationSource EffectiveFormationSource;

	// Formation 입력 Bucket 배열.
	//
	// 배열 인덱스가 Formation 입력 번호에 대응한다.
	// 후열만 쓰는 [0, N] 같은 경우를 위해 중간 빈 Bucket은 유지할 수 있다.
	// 단, 뒤쪽의 미사용 빈 Bucket을 억지로 보존할 필요는 없다.
	//
	// Formation入力Bucket配列。
	//
	// 配列インデックスがFormation入力番号に対応する。
	// 後列だけを使う[0, N]のような場合に備え、
	// 中間の空Bucketは保持できる。
	// ただし、後続の未使用空Bucketを強制保持する必要はない。
	UPROPERTY(
		BlueprintReadOnly,
		Category = "敵Encounter|GroupSeed")
	TArray<FEnemyTacticalGroupBucketSeed> Buckets;
};