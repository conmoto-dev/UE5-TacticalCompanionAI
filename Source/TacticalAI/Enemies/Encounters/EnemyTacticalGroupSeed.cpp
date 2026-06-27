#include "Enemies/Encounters/EnemyTacticalGroupSeed.h"
#include "Characters/EnemyCharacter.h"

bool FEnemyTacticalGroupUnitSeed::IsValidSeed(
	FString* OutError) const
{
	if (OutError)
	{
		OutError->Reset();
	}

	// [1] 적 종류 Class가 없으면 스폰할 수 없다.
	// [1] 敵種Classが無い場合はスポーンできない。
	if (!EnemyClass.Get())
	{
		if (OutError)
		{
			*OutError =
				TEXT("敵Classが設定されていません。");
		}

		return false;
	}

	// [2] Seed 단계에서 0 이하 Count는 작성/해석 오류로 본다.
	// [2] Seed段階では0以下のCountを作成・解釈エラーとして扱う。
	if (Count <= 0)
	{
		if (OutError)
		{
			*OutError = FString::Printf(
				TEXT("敵数は1以上である必要があります（現在値: %d）。"),
				Count);
		}

		return false;
	}

	return true;
}

bool FEnemyTacticalGroupBucketSeed::TryAddUnit(
	TSubclassOf<AEnemyCharacter> InEnemyClass,
	int32 InCount,
	FString* OutError)
{
	if (OutError)
	{
		OutError->Reset();
	}

	// [1] Resolver 입력 단계에서 잘못된 Class를 빠르게 거부한다.
	// [1] Resolver入力段階で不正なClassを早期に拒否する。
	if (!InEnemyClass.Get())
	{
		if (OutError)
		{
			*OutError =
				TEXT("追加対象の敵Classが設定されていません。");
		}

		return false;
	}

	// [2] Count 0 항목을 Seed에 남기지 않는다.
	// [2] Count 0の項目をSeedに残さない。
	if (InCount <= 0)
	{
		if (OutError)
		{
			*OutError = FString::Printf(
				TEXT("追加する敵数は1以上である必要があります（現在値: %d）。"),
				InCount);
		}

		return false;
	}

	// [3] 같은 EnemyClass는 같은 Bucket 안에서 누적한다.
	// [3] 同じEnemyClassは同じBucket内で集約する。
	for (FEnemyTacticalGroupUnitSeed& Unit : Units)
	{
		if (Unit.EnemyClass == InEnemyClass)
		{
			Unit.Count += InCount;
			return true;
		}
	}

	FEnemyTacticalGroupUnitSeed NewUnit;
	NewUnit.EnemyClass = InEnemyClass;
	NewUnit.Count = InCount;

	Units.Add(MoveTemp(NewUnit));
	return true;
}

bool FEnemyTacticalGroupBucketSeed::TryGetMemberCount(
	int32& OutMemberCount,
	FString* OutError) const
{
	OutMemberCount = 0;

	if (OutError)
	{
		OutError->Reset();
	}

	// [1] Bucket 자체가 비어 있는 것은 허용한다.
	// [1] Bucket自体が空であることは許可する。
	if (Units.IsEmpty())
	{
		return true;
	}

	// [2] 각 Unit을 검증하면서 전체 인원수를 계산한다.
	// [2] 各Unitを検証しながら総人数を計算する。
	for (int32 UnitIndex = 0;
		UnitIndex < Units.Num();
		++UnitIndex)
	{
		FString UnitError;

		if (!Units[UnitIndex].IsValidSeed(&UnitError))
		{
			if (OutError)
			{
				*OutError = FString::Printf(
					TEXT("Bucket内Unit %d が無効です。詳細: %s"),
					UnitIndex + 1,
					*UnitError);
			}

			return false;
		}

		OutMemberCount += Units[UnitIndex].Count;
	}

	return true;
}

bool FEnemyTacticalGroupSeed::TryBuildLayoutContext(
	FEnemyFormationLayoutContext& OutContext,
	FString* OutError) const
{
	OutContext.BucketMemberCounts.Reset();

	if (OutError)
	{
		OutError->Reset();
	}

	// [1] GroupSeed는 이미 실제 사용할 Formation Source를 가져야 한다.
	// [1] GroupSeedは実際に使用するFormation Sourceを持っている必要がある。
	if (!EffectiveFormationSource.HasOverride())
	{
		if (OutError)
		{
			*OutError =
				TEXT("有効なFormation Sourceが設定されていません。");
		}

		return false;
	}

	// [2] Bucket이 하나도 없으면 레이아웃 입력을 만들 수 없다.
	// [2] Bucketが1つも無い場合、レイアウト入力を作れない。
	if (Buckets.IsEmpty())
	{
		if (OutError)
		{
			*OutError =
				TEXT("GroupSeedに入力Bucketがありません。");
		}

		return false;
	}

	OutContext.BucketMemberCounts.Reserve(Buckets.Num());

	int32 TotalMemberCount = 0;

	// [3] 각 Bucket의 전체 인원수를 Formation 입력으로 변환한다.
	// [3] 各Bucketの総人数をFormation入力へ変換する。
	for (int32 BucketIndex = 0;
		BucketIndex < Buckets.Num();
		++BucketIndex)
	{
		int32 BucketMemberCount = 0;
		FString BucketError;

		if (!Buckets[BucketIndex].TryGetMemberCount(
			BucketMemberCount,
			&BucketError))
		{
			if (OutError)
			{
				*OutError = FString::Printf(
					TEXT("入力Bucket %d が無効です。詳細: %s"),
					BucketIndex + 1,
					*BucketError);
			}

			return false;
		}

		OutContext.BucketMemberCounts.Add(BucketMemberCount);
		TotalMemberCount += BucketMemberCount;
	}

	// [4] 중간 빈 Bucket은 허용하지만 전체가 비어 있으면 의미가 없다.
	// [4] 中間の空Bucketは許可するが、全体が空の場合は意味がない。
	if (TotalMemberCount <= 0)
	{
		OutContext.BucketMemberCounts.Reset();

		if (OutError)
		{
			*OutError =
				TEXT("GroupSeed内のすべてのBucketが空です。");
		}

		return false;
	}

	return true;
}

bool FEnemyTacticalGroupSeed::TryGetTotalMemberCount(
	int32& OutTotalMemberCount,
	FString* OutError) const
{
	OutTotalMemberCount = 0;

	if (OutError)
	{
		OutError->Reset();
	}

	// [1] 디버그/검증용 전체 인원수 계산.
	// [1] デバッグ・検証用の総人数計算。
	for (int32 BucketIndex = 0;
		BucketIndex < Buckets.Num();
		++BucketIndex)
	{
		int32 BucketMemberCount = 0;
		FString BucketError;

		if (!Buckets[BucketIndex].TryGetMemberCount(
			BucketMemberCount,
			&BucketError))
		{
			if (OutError)
			{
				*OutError = FString::Printf(
					TEXT("入力Bucket %d が無効です。詳細: %s"),
					BucketIndex + 1,
					*BucketError);
			}

			return false;
		}

		OutTotalMemberCount += BucketMemberCount;
	}

	return true;
}