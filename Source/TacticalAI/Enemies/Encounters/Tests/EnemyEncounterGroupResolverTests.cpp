#include "Misc/AutomationTest.h"

#include "Characters/EnemyCharacter.h"
#include "Enemies/Encounters/EnemyEncounterGroupResolver.h"
#include "Enemies/Encounters/Tests/EnemyEncounterGroupResolverTestTypes.h"
#include "Enemies/Formations/EnemyFormationProfile.h"
#include "Enemies/Formations/EnemyFormationStrategy_Composite.h"

#include "UObject/UnrealType.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace UE::TacticalAI::Tests
{
	// =========================================================================
	// AEnemyCharacter의 테스트용 DefaultFormationProfile 교체 헬퍼.
	//
	// 실제 프로덕션 경로에서는 EnemyCharacter의 private UPROPERTY를
	// 직접 수정하지 않는다.
	// 테스트에서는 Resolver가 CDO 기본 Profile을 읽는 경로를 검증하기 위해
	// Reflection으로 테스트 Profile을 주입한다.
	//
	// AEnemyCharacterのテスト用DefaultFormationProfile差し替えHelper。
	//
	// 実運用コードではprivate UPROPERTYを直接変更しない。
	// テストではResolverがCDOのデフォルトProfileを読む経路を
	// 検証するため、ReflectionでテストProfileを注入する。
	// =========================================================================
	static FObjectPropertyBase* FindDefaultFormationProfileProperty(
	FString& OutError)
	{
		OutError.Reset();

		FProperty* RawProperty =
			FindFProperty<FProperty>(
				AEnemyCharacter::StaticClass(),
				TEXT("DefaultFormationProfile"));

		FObjectPropertyBase* Property =
			CastField<FObjectPropertyBase>(RawProperty);

		if (!Property)
		{
			OutError =
				TEXT("DefaultFormationProfile Propertyを取得できませんでした。");

			return nullptr;
		}

		if (!Property->PropertyClass
			|| !Property->PropertyClass->IsChildOf(
				UEnemyFormationProfile::StaticClass()))
		{
			OutError =
				TEXT("DefaultFormationProfile Propertyの型が不正です。");

			return nullptr;
		}

		return Property;
	}

	static bool TrySetDefaultFormationProfileForClass(
		UClass* EnemyClass,
		UEnemyFormationProfile* Profile,
		FString& OutError)
	{
		OutError.Reset();

		if (!IsValid(EnemyClass))
		{
			OutError =
				TEXT("敵Classが無効です。");

			return false;
		}

		AEnemyCharacter* CDO =
			Cast<AEnemyCharacter>(EnemyClass->GetDefaultObject());

		if (!IsValid(CDO))
		{
			OutError = FString::Printf(
				TEXT("%s: 敵ClassのCDOを取得できませんでした。"),
				*EnemyClass->GetName());

			return false;
		}

		FObjectPropertyBase* Property =
		FindDefaultFormationProfileProperty(OutError);

		if (!Property)
		{
			return false;
		}

		void* PropertyValuePtr =
			Property->ContainerPtrToValuePtr<void>(CDO);

		Property->SetObjectPropertyValue(
			PropertyValuePtr,
			Profile);
		
		return true;
	}

	static UEnemyFormationProfile* GetDefaultFormationProfileForClass(
		UClass* EnemyClass)
	{
		if (!IsValid(EnemyClass))
		{
			return nullptr;
		}

		const AEnemyCharacter* CDO =
			Cast<AEnemyCharacter>(EnemyClass->GetDefaultObject());

		if (!IsValid(CDO))
		{
			return nullptr;
		}

		return CDO->GetDefaultFormationProfile();
	}

	// =========================================================================
	// 테스트 중 CDO의 DefaultFormationProfile을 임시 교체하고 복구한다.
	//
	// テスト中だけCDOのDefaultFormationProfileを差し替え、
	// Scope終了時に元へ戻す。
	// =========================================================================
	struct FScopedDefaultFormationProfileOverride
	{
		FScopedDefaultFormationProfileOverride(
			UClass* InEnemyClass,
			UEnemyFormationProfile* InProfile)
			: EnemyClass(InEnemyClass)
			, PreviousProfile(GetDefaultFormationProfileForClass(InEnemyClass))
		{
			FString Error;
			bApplied = TrySetDefaultFormationProfileForClass(
				EnemyClass,
				InProfile,
				Error);
		}

		~FScopedDefaultFormationProfileOverride()
		{
			if (!bApplied)
			{
				return;
			}

			FString Error;
			TrySetDefaultFormationProfileForClass(
				EnemyClass,
				PreviousProfile,
				Error);
		}

		bool IsApplied() const
		{
			return bApplied;
		}

	private:
		UClass* EnemyClass = nullptr;
		UEnemyFormationProfile* PreviousProfile = nullptr;
		bool bApplied = false;
	};

	static FEnemyEncounterSpawnEntry MakeSpawnEntry(
		TSubclassOf<AEnemyCharacter> EnemyClass,
		int32 Count,
		int32 OverrideInputNumber = 1)
	{
		FEnemyEncounterSpawnEntry Entry;
		Entry.EnemyClass = EnemyClass;
		Entry.Count = Count;
		Entry.OverrideInputNumber = OverrideInputNumber;
		return Entry;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FEnemyEncounterGroupResolverDefaultSplitTest,
	"TacticalAI.Enemies.Encounters.GroupResolver.DefaultSplitByEnemyClass",
	EAutomationTestFlags::EditorContext
	| EAutomationTestFlags::EngineFilter)

bool FEnemyEncounterGroupResolverDefaultSplitTest::RunTest(
	const FString& Parameters)
{
	using namespace UE::TacticalAI::Tests;

	UEnemyFormationProfile* ProfileA =
		NewObject<UEnemyFormationProfile>(
			GetTransientPackage(),
			TEXT("Test_DefaultFormation_Profile_A"));

	UEnemyFormationProfile* ProfileB =
		NewObject<UEnemyFormationProfile>(
			GetTransientPackage(),
			TEXT("Test_DefaultFormation_Profile_B"));

	TestNotNull(
		TEXT("テスト用Profile Aが生成される"),
		ProfileA);

	TestNotNull(
		TEXT("テスト用Profile Bが生成される"),
		ProfileB);

	FScopedDefaultFormationProfileOverride ScopedProfileA(
		AEnemyEncounterResolverTestEnemyA::StaticClass(),
		ProfileA);

	FScopedDefaultFormationProfileOverride ScopedProfileB(
		AEnemyEncounterResolverTestEnemyB::StaticClass(),
		ProfileB);

	TestTrue(
		TEXT("敵AのDefaultFormationProfileを差し替えられる"),
		ScopedProfileA.IsApplied());

	TestTrue(
		TEXT("敵BのDefaultFormationProfileを差し替えられる"),
		ScopedProfileB.IsApplied());

	FEnemyEncounterSpec Spec;
	Spec.FormationOverride.SourceMode =
		EEnemyFormationSourceMode::None;

	Spec.SpawnEntries.Add(
		MakeSpawnEntry(
			AEnemyEncounterResolverTestEnemyA::StaticClass(),
			3));

	Spec.SpawnEntries.Add(
		MakeSpawnEntry(
			AEnemyEncounterResolverTestEnemyB::StaticClass(),
			4));

	Spec.SpawnEntries.Add(
		MakeSpawnEntry(
			AEnemyEncounterResolverTestEnemyA::StaticClass(),
			2));

	TArray<FEnemyTacticalGroupSeed> GroupSeeds;
	FString Error;

	const bool bResolved =
		FEnemyEncounterGroupResolver::TryResolveGroupSeeds(
			Spec,
			GroupSeeds,
			&Error);

	TestTrue(
		FString::Printf(
			TEXT("Override無しEncounterを解決できる。Error: %s"),
			*Error),
		bResolved);

	TestEqual(
		TEXT("EnemyClass別に2つのGroupSeedが生成される"),
		GroupSeeds.Num(),
		2);

	if (GroupSeeds.Num() != 2)
	{
		return false;
	}

	const FEnemyTacticalGroupSeed& SeedA = GroupSeeds[0];
	const FEnemyTacticalGroupSeed& SeedB = GroupSeeds[1];

	TestEqual(
		TEXT("敵A GroupはProfile Aを使用する"),
		SeedA.EffectiveFormationSource.Profile.Get(),
		ProfileA);

	TestEqual(
		TEXT("敵B GroupはProfile Bを使用する"),
		SeedB.EffectiveFormationSource.Profile.Get(),
		ProfileB);

	TestEqual(
		TEXT("敵A GroupはBucketを1つ持つ"),
		SeedA.Buckets.Num(),
		1);

	TestEqual(
		TEXT("敵B GroupはBucketを1つ持つ"),
		SeedB.Buckets.Num(),
		1);

	if (SeedA.Buckets.Num() == 1
		&& SeedA.Buckets[0].Units.Num() == 1)
	{
		TestEqual(
			TEXT("敵Aは同じClass項目が集約されて5体になる"),
			SeedA.Buckets[0].Units[0].Count,
			5);
	}

	if (SeedB.Buckets.Num() == 1
		&& SeedB.Buckets[0].Units.Num() == 1)
	{
		TestEqual(
			TEXT("敵Bは4体になる"),
			SeedB.Buckets[0].Units[0].Count,
			4);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FEnemyEncounterGroupResolverOverrideBucketsTest,
	"TacticalAI.Enemies.Encounters.GroupResolver.OverrideBucketsByInputNumber",
	EAutomationTestFlags::EditorContext
	| EAutomationTestFlags::EngineFilter)

bool FEnemyEncounterGroupResolverOverrideBucketsTest::RunTest(
	const FString& Parameters)
{
	UEnemyFormationStrategy_Composite* InlineComposite =
		NewObject<UEnemyFormationStrategy_Composite>(
			GetTransientPackage(),
			TEXT("Test_InlineComposite"));

	TestNotNull(
		TEXT("テスト用Inline Compositeが生成される"),
		InlineComposite);

	FEnemyEncounterSpec Spec;
	Spec.FormationOverride.SourceMode =
		EEnemyFormationSourceMode::Inline;
	Spec.FormationOverride.InlineStrategy =
		InlineComposite;

	Spec.SpawnEntries.Add(
		UE::TacticalAI::Tests::MakeSpawnEntry(
			AEnemyEncounterResolverTestEnemyA::StaticClass(),
			3,
			1));

	Spec.SpawnEntries.Add(
		UE::TacticalAI::Tests::MakeSpawnEntry(
			AEnemyEncounterResolverTestEnemyB::StaticClass(),
			4,
			2));

	Spec.SpawnEntries.Add(
		UE::TacticalAI::Tests::MakeSpawnEntry(
			AEnemyEncounterResolverTestEnemyA::StaticClass(),
			2,
			2));

	TArray<FEnemyTacticalGroupSeed> GroupSeeds;
	FString Error;

	const bool bResolved =
		FEnemyEncounterGroupResolver::TryResolveGroupSeeds(
			Spec,
			GroupSeeds,
			&Error);

	TestTrue(
		FString::Printf(
			TEXT("Override Encounterを解決できる。Error: %s"),
			*Error),
		bResolved);

	TestEqual(
		TEXT("Override Encounterは1つのGroupSeedにまとまる"),
		GroupSeeds.Num(),
		1);

	if (GroupSeeds.Num() != 1)
	{
		return false;
	}

	const FEnemyTacticalGroupSeed& Seed = GroupSeeds[0];

	TestTrue(
	TEXT("Override GroupはInline Strategyを使用する"),
	Seed.EffectiveFormationSource.InlineStrategy.Get() == InlineComposite);

	TestEqual(
		TEXT("Override入力番号によりBucketが2つ生成される"),
		Seed.Buckets.Num(),
		2);

	if (Seed.Buckets.Num() != 2)
	{
		return false;
	}

	int32 Bucket0Count = 0;
	int32 Bucket1Count = 0;

	FString BucketError;

	TestTrue(
		TEXT("Bucket 0 の人数を計算できる"),
		Seed.Buckets[0].TryGetMemberCount(
			Bucket0Count,
			&BucketError));

	TestTrue(
		TEXT("Bucket 1 の人数を計算できる"),
		Seed.Buckets[1].TryGetMemberCount(
			Bucket1Count,
			&BucketError));

	TestEqual(
		TEXT("Bucket 0には敵A 3体が入る"),
		Bucket0Count,
		3);

	TestEqual(
		TEXT("Bucket 1には敵B 4体 + 敵A 2体が入る"),
		Bucket1Count,
		6);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FEnemyEncounterGroupResolverMiddleEmptyBucketTest,
	"TacticalAI.Enemies.Encounters.GroupResolver.PreserveMiddleEmptyBucket",
	EAutomationTestFlags::EditorContext
	| EAutomationTestFlags::EngineFilter)

bool FEnemyEncounterGroupResolverMiddleEmptyBucketTest::RunTest(
	const FString& Parameters)
{
	UEnemyFormationStrategy_Composite* InlineComposite =
		NewObject<UEnemyFormationStrategy_Composite>(
			GetTransientPackage(),
			TEXT("Test_InlineComposite_MiddleEmpty"));

	TestNotNull(
		TEXT("テスト用Inline Compositeが生成される"),
		InlineComposite);

	FEnemyEncounterSpec Spec;
	Spec.FormationOverride.SourceMode =
		EEnemyFormationSourceMode::Inline;
	Spec.FormationOverride.InlineStrategy =
		InlineComposite;

	Spec.SpawnEntries.Add(
		UE::TacticalAI::Tests::MakeSpawnEntry(
			AEnemyEncounterResolverTestEnemyB::StaticClass(),
			4,
			2));

	TArray<FEnemyTacticalGroupSeed> GroupSeeds;
	FString Error;

	const bool bResolved =
		FEnemyEncounterGroupResolver::TryResolveGroupSeeds(
			Spec,
			GroupSeeds,
			&Error);

	TestTrue(
		FString::Printf(
			TEXT("後列だけのOverride Encounterを解決できる。Error: %s"),
			*Error),
		bResolved);

	TestEqual(
		TEXT("GroupSeedは1つだけ生成される"),
		GroupSeeds.Num(),
		1);

	if (GroupSeeds.Num() != 1)
	{
		return false;
	}

	const FEnemyTacticalGroupSeed& Seed = GroupSeeds[0];

	TestEqual(
		TEXT("OverrideInputNumber 2 のためBucket 0と1が生成される"),
		Seed.Buckets.Num(),
		2);

	if (Seed.Buckets.Num() != 2)
	{
		return false;
	}

	FEnemyFormationLayoutContext LayoutContext;
	FString ContextError;

	TestTrue(
		FString::Printf(
			TEXT("LayoutContextを生成できる。Error: %s"),
			*ContextError),
		Seed.TryBuildLayoutContext(
			LayoutContext,
			&ContextError));

	TestEqual(
		TEXT("LayoutContextは2 Bucketを保持する"),
		LayoutContext.BucketMemberCounts.Num(),
		2);

	if (LayoutContext.BucketMemberCounts.Num() == 2)
	{
		TestEqual(
			TEXT("Bucket 0は空として保持される"),
			LayoutContext.BucketMemberCounts[0],
			0);

		TestEqual(
			TEXT("Bucket 1に4体入る"),
			LayoutContext.BucketMemberCounts[1],
			4);
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS