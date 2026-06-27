#pragma once

#include "CoreMinimal.h"
#include "Enemies/Encounters/EnemyEncounterTypes.h"
#include "Enemies/Encounters/EnemyTacticalGroupSeed.h"

// =========================================================================
// Enemy Encounter 작성 데이터를 전술 그룹 Seed로 해석하는 Resolver.
//
// 이 타입은 실제 Actor를 스폰하지 않는다.
// Encounter에 적힌 EnemyClass, Count, FormationOverride를 읽어
// 이후 Spawner/Coordinator가 사용할 그룹 씨앗만 만든다.
//
// Override가 없으면 EnemyClass별로 그룹을 나누고,
// Override가 있으면 몬스터 종류를 무시하고 지정된 입력 Bucket으로 묶는다.
//
// Enemy Encounter作成データを戦術グループSeedへ変換するResolver。
//
// この型は実際のActorをスポーンしない。
// Encounterに記述されたEnemyClass・Count・FormationOverrideを読み取り、
// 後段のSpawner/Coordinatorが使用するグループSeedだけを生成する。
//
// Overrideが無い場合はEnemyClass別にグループを分割し、
// Overrideがある場合は敵種を無視して指定入力Bucketへまとめる。
// =========================================================================
class TACTICALAI_API FEnemyEncounterGroupResolver
{
public:
	// =========================================================================
	// Encounter Spec을 전술 그룹 Seed 배열로 변환한다.
	// 成功時、OutGroupSeedsには1つ以上の有効なGroupSeedが入る。
	// =========================================================================
	static bool TryResolveGroupSeeds(
		const FEnemyEncounterSpec& Spec,
		TArray<FEnemyTacticalGroupSeed>& OutGroupSeeds,
		FString* OutError = nullptr);

private:
	static bool TryResolveDefaultGroups(
		const FEnemyEncounterSpec& Spec,
		TArray<FEnemyTacticalGroupSeed>& OutGroupSeeds,
		FString& OutError);

	static bool TryResolveOverrideGroup(
		const FEnemyEncounterSpec& Spec,
		TArray<FEnemyTacticalGroupSeed>& OutGroupSeeds,
		FString& OutError);

	static bool TryValidateSpawnEntry(
		const FEnemyEncounterSpawnEntry& Entry,
		int32 EntryIndex,
		bool bRequireOverrideInput,
		FString& OutError);

	static bool TryValidateFormationSource(
		const FEnemyFormationSource& Source,
		FString& OutError);

	static bool TryMakeDefaultFormationSource(
		TSubclassOf<AEnemyCharacter> EnemyClass,
		FEnemyFormationSource& OutSource,
		FString& OutError);
};