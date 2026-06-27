#include "Enemies/Encounters/EnemyEncounterMarker.h"
#include "Components/SceneComponent.h"
#include "Characters/EnemyCharacter.h"
#include "Enemies/Encounters/EnemyEncounterGroupResolver.h"
#include "Enemies/Formations/EnemyFormationProfile.h"
#include "Enemies/Formations/EnemyFormationSource.h"
#include "Enemies/Formations/EnemyFormationStrategy.h"

AEnemyEncounterMarker::AEnemyEncounterMarker()
{
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(
		TEXT("SceneRoot"));

	SetRootComponent(SceneRoot);
}

bool AEnemyEncounterMarker::TryResolveGroupSeeds(
	TArray<FEnemyTacticalGroupSeed>& OutGroupSeeds,
	FString* OutError) const
{
	return FEnemyEncounterGroupResolver::TryResolveGroupSeeds(
		EncounterSpec,
		OutGroupSeeds,
		OutError);
}

void AEnemyEncounterMarker::ValidateEncounter()
{
	TArray<FEnemyTacticalGroupSeed> GroupSeeds;
	FString Error;

	if (!TryResolveGroupSeeds(
		GroupSeeds,
		&Error))
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("[EnemyEncounterMarker] %s: Encounter検証に失敗しました。詳細: %s"),
			*GetName(),
			*Error);

		return;
	}

	int32 TotalMemberCount = 0;

	// [1] Resolver 결과의 전체 적 수를 계산한다.
	// [1] Resolver結果の総敵数を計算する。
	for (int32 SeedIndex = 0;
		SeedIndex < GroupSeeds.Num();
		++SeedIndex)
	{
		int32 SeedMemberCount = 0;
		FString SeedError;

		if (!GroupSeeds[SeedIndex].TryGetTotalMemberCount(
			SeedMemberCount,
			&SeedError))
		{
			UE_LOG(
				LogTemp,
				Warning,
				TEXT(
					"[EnemyEncounterMarker] %s: GroupSeed %d の検証に失敗しました。"
					"詳細: %s"),
				*GetName(),
				SeedIndex + 1,
				*SeedError);

			return;
		}

		TotalMemberCount += SeedMemberCount;
	}

	UE_LOG(
		LogTemp,
		Display,
		TEXT(
			"[EnemyEncounterMarker] %s: Encounter検証に成功しました。"
			"Group=%d, Enemy=%d"),
		*GetName(),	GroupSeeds.Num(), TotalMemberCount);

	if (bLogResolvedGroupsOnValidation)
	{
		LogResolvedGroupSeeds(GroupSeeds);
	}
}

void AEnemyEncounterMarker::OnConstruction(
	const FTransform& Transform)
{
	Super::OnConstruction(Transform);

#if WITH_EDITOR
	// [1] 에디터 편집 중 자동 검증 옵션.
	//     기본값은 false라 수동 Validate 버튼 사용을 우선한다.
	//
	// [1] Editor編集中の自動検証Option。
	//     デフォルトはfalseで、手動Validateボタンを優先する。
	if (bValidateOnConstruction
		&& !HasAnyFlags(RF_ClassDefaultObject))
	{
		ValidateEncounter();
	}
#endif
}

void AEnemyEncounterMarker::LogResolvedGroupSeeds(
	const TArray<FEnemyTacticalGroupSeed>& GroupSeeds) const
{
	// [1] GroupSeed별 Formation Source와 Bucket 구성을 로그로 보여준다.
	// [1] GroupSeedごとのFormation SourceとBucket構成をログ表示する。
	for (int32 SeedIndex = 0;
		SeedIndex < GroupSeeds.Num();
		++SeedIndex)
	{
		const FEnemyTacticalGroupSeed& Seed =
			GroupSeeds[SeedIndex];

		int32 SeedMemberCount = 0;
		FString SeedError;

		if (!Seed.TryGetTotalMemberCount(
			SeedMemberCount,
			&SeedError))
		{
			UE_LOG(
				LogTemp,
				Warning,
				TEXT(
					"[EnemyEncounterMarker] %s: GroupSeed %d の人数計算に"
					"失敗しました。詳細: %s"),
				*GetName(),
				SeedIndex + 1,
				*SeedError);

			continue;
		}

		UE_LOG(
			LogTemp,
			Display,
			TEXT(
				"[EnemyEncounterMarker] %s: GroupSeed %d '%s' Source=%s "
				"Enemy=%d Bucket=%d"),
			*GetName(),
			SeedIndex + 1,
			*Seed.DebugName.ToString(),
			*DescribeFormationSource(Seed.EffectiveFormationSource),
			SeedMemberCount,
			Seed.Buckets.Num());

		for (int32 BucketIndex = 0;
			BucketIndex < Seed.Buckets.Num();
			++BucketIndex)
		{
			const FEnemyTacticalGroupBucketSeed& Bucket =
				Seed.Buckets[BucketIndex];

			int32 BucketMemberCount = 0;
			FString BucketError;

			if (!Bucket.TryGetMemberCount(
				BucketMemberCount,
				&BucketError))
			{
				UE_LOG(
					LogTemp,
					Warning,
					TEXT(
						"[EnemyEncounterMarker] %s: GroupSeed %d Bucket %d の"
						"人数計算に失敗しました。詳細: %s"),
					*GetName(),
					SeedIndex + 1,
					BucketIndex + 1,
					*BucketError);

				continue;
			}

			FString UnitSummary;

			for (int32 UnitIndex = 0;
				UnitIndex < Bucket.Units.Num();
				++UnitIndex)
			{
				const FEnemyTacticalGroupUnitSeed& Unit =
					Bucket.Units[UnitIndex];

				if (!UnitSummary.IsEmpty())
				{
					UnitSummary += TEXT(", ");
				}

				UnitSummary += FString::Printf(
					TEXT("%s x%d"),
					*GetNameSafe(Unit.EnemyClass.Get()),
					Unit.Count);
			}

			if (UnitSummary.IsEmpty())
			{
				UnitSummary = TEXT("空");
			}

			UE_LOG(
				LogTemp,
				Display,
				TEXT(
					"[EnemyEncounterMarker] %s:   Bucket %d Enemy=%d [%s]"),
				*GetName(),
				BucketIndex + 1,
				BucketMemberCount,
				*UnitSummary);
		}
	}
}

FString AEnemyEncounterMarker::DescribeFormationSource(
	const FEnemyFormationSource& Source) const
{
	switch (Source.SourceMode)
	{
	case EEnemyFormationSourceMode::None:
		return TEXT("None");

	case EEnemyFormationSourceMode::Profile:
		return FString::Printf(
			TEXT("Profile:%s"),
			*GetNameSafe(Source.Profile.Get()));

	case EEnemyFormationSourceMode::Inline:
		return FString::Printf(
			TEXT("Inline:%s"),
			*GetNameSafe(Source.InlineStrategy.Get()));

	default:
		break;
	}

	return FString::Printf(
		TEXT("Unknown:%d"),
		static_cast<int32>(Source.SourceMode));
}