#include "Enemies/Formations/EnemyFormationStrategy_Composite.h"
#include "Enemies/Formations/EnemySubFormationStrategy.h"

bool UEnemyFormationStrategy_Composite::BuildLayoutInternal(
	const FEnemyFormationLayoutContext& Context,
	FEnemyFormationLayout& OutLayout,
	FString& OutError) const
{
	// [1] 입력 버킷과 하위 진형 정의는 1:1로 대응해야 한다.
	// [1] 入力バケットとサブフォーメーション定義は
	//     1対1で対応しなければならない。
	if (Context.BucketMemberCounts.Num()
		!= SubFormationEntries.Num())
	{
		OutError = FString::Printf(
			TEXT(
				"%s: 入力バケット数は%dですが、"
				"サブフォーメーション定義は%d個です。"),
			*GetClass()->GetName(),
			Context.BucketMemberCounts.Num(),
			SubFormationEntries.Num());

		return false;
	}

	OutLayout.BucketLayouts.SetNum(SubFormationEntries.Num());

	// [2] 각 입력 버킷의 하위 진형을 순서대로 생성한다.
	// [2] 各入力バケットのサブフォーメーションを
	//     配列順に生成する。
	for (int32 EntryIndex = 0;
		EntryIndex < SubFormationEntries.Num();
		++EntryIndex)
	{
		const FEnemyCompositeFormationEntry& Entry =
			SubFormationEntries[EntryIndex];

		// [3] 버킷이 비어 있더라도 진형 정의 자체는 유효해야 한다.
		//     지금 비어 있다는 이유로 누락된 전략을 허용하면,
		//     나중에 해당 버킷을 채웠을 때만 실패하게 된다.
		//
		// [3] バケットが空の場合でも定義自体は有効でなければならない。
		//     現在空であることを理由に未設定を許可すると、
		//     後から敵を割り当てた場合にのみ失敗してしまう。
		if (!IsValid(Entry.SubFormationStrategy.Get()))
		{
			OutError = FString::Printf(
				TEXT(
					"%s: 入力%dのサブフォーメーション戦略が"
					"設定されていません。"),
				*GetClass()->GetName(),
				EntryIndex + 1);

			return false;
		}

		// [4] 상대 위치와 회전에 잘못된 값이 없는지 확인한다.
		// [4] 相対位置と回転に不正な値がないか確認する。
		const FTransform SubFormationRootTransform(
			Entry.RelativeRotation,
			Entry.RelativeLocation,
			FVector::OneVector);

		if (SubFormationRootTransform.ContainsNaN())
		{
			OutError = FString::Printf(
				TEXT(
					"%s: 入力%dの相対位置または相対回転に"
					"非数値（NaN）が含まれています。"),
				*GetClass()->GetName(),
				EntryIndex + 1);

			return false;
		}

		// [5] 각 버킷마다 재현 가능하면서도 서로 다른 시드를 만든다.
		//     이후 여러 Scatter를 함께 사용해도
		//     동일한 로컬 패턴이 겹치는 것을 피하기 위한 경계다.
		//
		// [5] 各バケットに再現可能かつ異なるシードを割り当てる。
		//     複数の分散配置を組み合わせた際に、
		//     同一のローカルパターンが重なることを避ける。
		const uint32 DerivedSeed = HashCombineFast(
			GetTypeHash(Context.RandomSeed),
			GetTypeHash(EntryIndex));

		FEnemySubFormationBuildContext SubFormationContext;
		SubFormationContext.MemberCount = Context.BucketMemberCounts[EntryIndex];
		SubFormationContext.RandomSeed = static_cast<int32>(DerivedSeed);

		// [6] 하위 진형 자체 원점 기준의 로컬 슬롯을 생성한다.
		// [6] サブフォーメーション自身の原点を基準として
		//     ローカルスロットを生成する。
		TArray<FTransform> SubFormationLocalSlots;
		FString SubFormationError;

		if (!Entry.SubFormationStrategy->TryBuildLocalSlots(
			SubFormationContext,
			SubFormationLocalSlots,
			&SubFormationError))
		{
			OutError = FString::Printf(
				TEXT(
					"%s: 入力%dのサブフォーメーション生成に"
					"失敗しました。詳細: %s"),
				*GetClass()->GetName(),
				EntryIndex + 1,
				*SubFormationError);

			return false;
		}

		FEnemyFormationBucketLayout& BucketLayout = OutLayout.BucketLayouts[EntryIndex];

		BucketLayout.LocalSlotTransforms.Reserve(SubFormationLocalSlots.Num());

		const FQuat RootRotation = SubFormationRootTransform.GetRotation();

		const FVector RootLocation = SubFormationRootTransform.GetLocation();

		// [7] 하위 진형 로컬 슬롯을
		//     전체 진형 루트 기준 로컬 슬롯으로 변환한다.
		//
		// [7] サブフォーメーション基準のローカルスロットを、
		//     フォーメーションルート基準へ変換する。
		for (const FTransform& SubFormationLocalSlot : SubFormationLocalSlots)
		{
			const FVector FormationLocalLocation =
				RootRotation.RotateVector(SubFormationLocalSlot.GetLocation())
				+ RootLocation;

			FQuat FormationLocalRotation =
				RootRotation * SubFormationLocalSlot.GetRotation();

			FormationLocalRotation.Normalize();

			BucketLayout.LocalSlotTransforms.Emplace(
				FormationLocalRotation,
				FormationLocalLocation,
				SubFormationLocalSlot.GetScale3D());
		}
	}

	return true;
}