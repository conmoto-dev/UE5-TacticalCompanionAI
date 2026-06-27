#include "Enemies/Encounters/EnemyEncounterGroupResolver.h"
#include "Characters/EnemyCharacter.h"
#include "Enemies/Formations/EnemyFormationProfile.h"
#include "Enemies/Formations/EnemyFormationStrategy.h"

bool FEnemyEncounterGroupResolver::TryResolveGroupSeeds(
	const FEnemyEncounterSpec& Spec,
	TArray<FEnemyTacticalGroupSeed>& OutGroupSeeds,
	FString* OutError)
{
	OutGroupSeeds.Reset();

	if (OutError)
	{
		OutError->Reset();
	}

	// [1] 스폰 항목이 없는 Encounter는 전술 그룹을 만들 수 없다.
	// [1] スポーン項目が無いEncounterからは戦術グループを作れない。
	if (Spec.SpawnEntries.IsEmpty())
	{
		if (OutError)
		{
			*OutError =
				TEXT("Encounterにスポーン項目がありません。");
		}

		return false;
	}

	FString Error;

	const bool bSucceeded = Spec.HasFormationOverride()
		? TryResolveOverrideGroup(Spec, OutGroupSeeds, Error)
		: TryResolveDefaultGroups(Spec, OutGroupSeeds, Error);

	if (!bSucceeded)
	{
		OutGroupSeeds.Reset();

		if (OutError)
		{
			*OutError = MoveTemp(Error);
		}

		return false;
	}

	// [2] Resolver 결과는 최소 하나 이상의 그룹을 가져야 한다.
	// [2] Resolver結果には少なくとも1つのグループが必要。
	if (OutGroupSeeds.IsEmpty())
	{
		if (OutError)
		{
			*OutError =
				TEXT("Encounter Resolverが有効なGroupSeedを生成しませんでした。");
		}

		return false;
	}

	return true;
}

bool FEnemyEncounterGroupResolver::TryResolveDefaultGroups(
	const FEnemyEncounterSpec& Spec,
	TArray<FEnemyTacticalGroupSeed>& OutGroupSeeds,
	FString& OutError)
{
	OutGroupSeeds.Reset();
	OutError.Reset();

	// [1] Override가 없으면 EnemyClass별로 그룹을 나눈다.
	// [1] Overrideが無い場合はEnemyClass別にグループを分割する。
	TMap<UClass*, int32> EnemyClassToSeedIndex;

	for (int32 EntryIndex = 0;
		EntryIndex < Spec.SpawnEntries.Num();
		++EntryIndex)
	{
		const FEnemyEncounterSpawnEntry& Entry =
			Spec.SpawnEntries[EntryIndex];

		if (!TryValidateSpawnEntry(
			Entry,
			EntryIndex,
			false,
			OutError))
		{
			return false;
		}

		UClass* EnemyClass = Entry.EnemyClass.Get();

		if (const int32* ExistingSeedIndex =
			EnemyClassToSeedIndex.Find(EnemyClass))
		{
			// [2] 같은 EnemyClass 항목은 같은 기본 그룹의 Bucket 0에 누적한다.
			// [2] 同じEnemyClass項目は同じデフォルトグループのBucket 0へ集約する。
			FEnemyTacticalGroupSeed& ExistingSeed =
				OutGroupSeeds[*ExistingSeedIndex];

			if (!ExistingSeed.Buckets[0].TryAddUnit(
				Entry.EnemyClass,
				Entry.Count,
				&OutError))
			{
				OutError = FString::Printf(
					TEXT("デフォルトGroupへの敵追加に失敗しました。詳細: %s"),
					*OutError);

				return false;
			}

			continue;
		}

		// [3] 새 EnemyClass 그룹은 해당 적 종류의 기본 Formation Profile을 사용한다.
		// [3] 新しいEnemyClassグループは、その敵種のデフォルトFormation Profileを使う。
		FEnemyFormationSource DefaultFormationSource;

		if (!TryMakeDefaultFormationSource(
			Entry.EnemyClass,
			DefaultFormationSource,
			OutError))
		{
			OutError = FString::Printf(
				TEXT(
					"Entry %d の敵種デフォルトFormation取得に失敗しました。詳細: %s"),
				EntryIndex + 1,
				*OutError);

			return false;
		}

		FEnemyTacticalGroupSeed NewSeed;
		NewSeed.DebugName = FName(*FString::Printf(
			TEXT("%s_DefaultFormation"),
			*EnemyClass->GetName()));
		NewSeed.EffectiveFormationSource = DefaultFormationSource;
		NewSeed.Buckets.SetNum(1);

		if (!NewSeed.Buckets[0].TryAddUnit(
			Entry.EnemyClass,
			Entry.Count,
			&OutError))
		{
			OutError = FString::Printf(
				TEXT("新規デフォルトGroupへの敵追加に失敗しました。詳細: %s"),
				*OutError);

			return false;
		}

		const int32 NewSeedIndex = OutGroupSeeds.Add(MoveTemp(NewSeed));
		EnemyClassToSeedIndex.Add(EnemyClass, NewSeedIndex);
	}

	// [4] 생성된 Seed들이 Layout Context로 변환 가능한지 가볍게 검증한다.
	// [4] 生成されたSeedがLayout Contextへ変換可能か軽く検証する。
	for (int32 SeedIndex = 0;
		SeedIndex < OutGroupSeeds.Num();
		++SeedIndex)
	{
		FEnemyFormationLayoutContext LayoutContext;
		FString SeedError;

		if (!OutGroupSeeds[SeedIndex].TryBuildLayoutContext(
			LayoutContext,
			&SeedError))
		{
			OutError = FString::Printf(
				TEXT("デフォルトGroupSeed %d が無効です。詳細: %s"),
				SeedIndex + 1,
				*SeedError);

			return false;
		}
	}

	return true;
}

bool FEnemyEncounterGroupResolver::TryResolveOverrideGroup(
	const FEnemyEncounterSpec& Spec,
	TArray<FEnemyTacticalGroupSeed>& OutGroupSeeds,
	FString& OutError)
{
	OutGroupSeeds.Reset();
	OutError.Reset();

	// [1] Override Source는 실제 사용할 Profile 또는 Inline Strategy를 가져야 한다.
	// [1] Override Sourceは実際に使うProfileまたはInline Strategyを持つ必要がある。
	if (!TryValidateFormationSource(
		Spec.FormationOverride,
		OutError))
	{
		return false;
	}

	FEnemyTacticalGroupSeed OverrideSeed;
	OverrideSeed.DebugName = TEXT("EncounterFormationOverride");
	OverrideSeed.EffectiveFormationSource =
		Spec.FormationOverride;

	// [2] Override가 있으면 EnemyClass가 아니라 OverrideInputNumber별로 Bucket을 만든다.
	// [2] Overrideがある場合はEnemyClassではなくOverrideInputNumber別にBucketを作る。
	for (int32 EntryIndex = 0;
		EntryIndex < Spec.SpawnEntries.Num();
		++EntryIndex)
	{
		const FEnemyEncounterSpawnEntry& Entry =
			Spec.SpawnEntries[EntryIndex];

		if (!TryValidateSpawnEntry(
			Entry,
			EntryIndex,
			true,
			OutError))
		{
			return false;
		}

		const int32 BucketIndex =
			Entry.GetOverrideBucketIndex();

		OverrideSeed.Buckets.SetNum(
			FMath::Max(
				OverrideSeed.Buckets.Num(),
				BucketIndex + 1));

		if (!OverrideSeed.Buckets[BucketIndex].TryAddUnit(
			Entry.EnemyClass,
			Entry.Count,
			&OutError))
		{
			OutError = FString::Printf(
				TEXT(
					"Override入力Bucket %d への敵追加に失敗しました。詳細: %s"),
				BucketIndex + 1,
				*OutError);

			return false;
		}
	}

	// [3] 중간 빈 Bucket은 유지한다. 뒤쪽 미사용 Bucket은 만들지 않는다.
	// [3] 中間の空Bucketは保持する。後続の未使用Bucketは作らない。
	FEnemyFormationLayoutContext LayoutContext;
	FString SeedError;

	if (!OverrideSeed.TryBuildLayoutContext(
		LayoutContext,
		&SeedError))
	{
		OutError = FString::Printf(
			TEXT("Override GroupSeedが無効です。詳細: %s"),
			*SeedError);

		return false;
	}

	OutGroupSeeds.Add(MoveTemp(OverrideSeed));
	return true;
}

bool FEnemyEncounterGroupResolver::TryValidateSpawnEntry(
	const FEnemyEncounterSpawnEntry& Entry,
	int32 EntryIndex,
	bool bRequireOverrideInput,
	FString& OutError)
{
	OutError.Reset();

	// [1] EnemyClass가 없으면 기본 그룹화도 Override 그룹화도 불가능하다.
	// [1] EnemyClassが無い場合、デフォルト分割もOverride分割もできない。
	if (!Entry.EnemyClass.Get())
	{
		OutError = FString::Printf(
			TEXT("Entry %d: 敵Classが設定されていません。"),
			EntryIndex + 1);

		return false;
	}

	// [2] Count는 작성 데이터 단계에서 1 이상이어야 한다.
	// [2] Countは作成データ段階で1以上でなければならない。
	if (Entry.Count <= 0)
	{
		OutError = FString::Printf(
			TEXT("Entry %d: 敵数は1以上である必要があります（現在値: %d）。"),
			EntryIndex + 1,
			Entry.Count);

		return false;
	}

	// [3] Override 입력 번호는 Override 경로에서만 의미를 갖는다.
	// [3] Override入力番号はOverride経路でのみ意味を持つ。
	if (bRequireOverrideInput
		&& Entry.OverrideInputNumber <= 0)
	{
		OutError = FString::Printf(
			TEXT(
				"Entry %d: Override入力番号は1以上である必要があります"
				"（現在値: %d）。"),
			EntryIndex + 1,
			Entry.OverrideInputNumber);

		return false;
	}

	return true;
}

bool FEnemyEncounterGroupResolver::TryValidateFormationSource(
	const FEnemyFormationSource& Source,
	FString& OutError)
{
	OutError.Reset();

	switch (Source.SourceMode)
	{
	case EEnemyFormationSourceMode::None:
		OutError =
			TEXT("Formation OverrideがNoneに設定されています。");
		return false;

	case EEnemyFormationSourceMode::Profile:
		if (!IsValid(Source.Profile.Get()))
		{
			OutError =
				TEXT("Formation OverrideのProfileが設定されていません。");
			return false;
		}

		return true;

	case EEnemyFormationSourceMode::Inline:
		if (!IsValid(Source.InlineStrategy.Get()))
		{
			OutError =
				TEXT("Formation OverrideのInline Strategyが設定されていません。");
			return false;
		}

		return true;

	default:
		break;
	}

	OutError = FString::Printf(
		TEXT("未対応のFormation Source方式です（%d）。"),
		static_cast<int32>(Source.SourceMode));

	return false;
}

bool FEnemyEncounterGroupResolver::TryMakeDefaultFormationSource(
	TSubclassOf<AEnemyCharacter> EnemyClass,
	FEnemyFormationSource& OutSource,
	FString& OutError)
{
	OutSource = FEnemyFormationSource();
	OutError.Reset();

	// [1] EnemyClass CDO에서 적 종류 기본 Formation Profile을 읽는다.
	// [1] EnemyClass CDOから敵種デフォルトFormation Profileを取得する。
	if (!EnemyClass.Get())
	{
		OutError =
			TEXT("敵Classが設定されていません。");
		return false;
	}

	const AEnemyCharacter* EnemyCDO =
		Cast<AEnemyCharacter>(EnemyClass->GetDefaultObject());

	if (!IsValid(EnemyCDO))
	{
		OutError = FString::Printf(
			TEXT("%s: 敵ClassのCDOを取得できませんでした。"),
			*EnemyClass->GetName());

		return false;
	}

	UEnemyFormationProfile* DefaultFormationProfile =
		EnemyCDO->GetDefaultFormationProfile();

	if (!IsValid(DefaultFormationProfile))
	{
		OutError = FString::Printf(
			TEXT(
				"%s: デフォルトFormation Profileが設定されていません。"),
			*EnemyClass->GetName());

		return false;
	}

	// [2] 기본 Formation은 Profile Source로 고정한다.
	// [2] デフォルトFormationはProfile Sourceとして扱う。
	OutSource.SourceMode = EEnemyFormationSourceMode::Profile;
	OutSource.Profile = DefaultFormationProfile;
	OutSource.InlineStrategy = nullptr;

	return true;
}