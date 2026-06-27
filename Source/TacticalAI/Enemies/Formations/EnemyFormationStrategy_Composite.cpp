#include "Enemies/Formations/EnemyFormationStrategy_Composite.h"
#include "Enemies/Formations/EnemySubFormationStrategy.h"

bool UEnemyFormationStrategy_Composite::BuildLayoutInternal(
	const FEnemyFormationLayoutContext& Context,
	FEnemyFormationLayout& OutLayout,
	FString& OutError) const
{
	// [1] 입력 버킷 수는 정의된 하위 진형 수를 초과할 수 없다.
	// [1] 入力バケット数は定義済みサブフォーメーション数を超えてはならない。
	if (Context.BucketMemberCounts.Num() > SubFormationEntries.Num())
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

	// [2] 출력 Layout은 실제 입력 버킷 수만큼만 만든다.
	//     뒤쪽 미입력 Entry를 [0]으로 강제 보존하지 않는다.
	//
	// [2] 出力Layoutは実際の入力バケット数だけ生成する。
	//     後続の未入力Entryを[0]として強制保持しない。
	OutLayout.BucketLayouts.SetNum(Context.BucketMemberCounts.Num());

	// [3] 입력된 버킷만 순서대로 처리한다.
	// [3] 入力されたバケットだけを配列順に処理する。
	for (int32 EntryIndex = 0;
		EntryIndex < Context.BucketMemberCounts.Num();
		++EntryIndex)
	{
		const int32 MemberCount =
			Context.BucketMemberCounts[EntryIndex];

		const FEnemyCompositeFormationEntry& Entry =
			SubFormationEntries[EntryIndex];

		FEnemyFormationBucketLayout& BucketLayout =
			OutLayout.BucketLayouts[EntryIndex];

		// [4] 빈 버킷은 정상 입력이다.
		//     이 경우 Strategy가 없어도 현재 레이아웃 생성에는 필요하지 않다.
		//
		// [4] 空バケットは正常入力として扱う。
		//     この場合、現在のレイアウト生成にはStrategy未設定でも問題ない。
		if (MemberCount == 0)
		{
			BucketLayout.LocalSlotTransforms.Reset();
			continue;
		}

		// [5] 실제 멤버가 있는 버킷은 하위 진형 Strategy가 반드시 필요하다.
		// [5] 実際にメンバーが存在するバケットには
		//     サブフォーメーションStrategyが必須。
		if (!IsValid(Entry.SubFormationStrategy.Get()))
		{
			OutError = FString::Printf(
				TEXT(
					"%s: 入力%dには%d体のメンバーがありますが、"
					"サブフォーメーション戦略が設定されていません。"),
				*GetClass()->GetName(),
				EntryIndex + 1,
				MemberCount);

			return false;
		}

		// [6] 사용 중인 Entry의 상대 위치와 회전에 잘못된 값이 없는지 확인한다.
		// [6] 使用中Entryの相対位置と回転に不正な値がないか確認する。
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

		// [7] 각 버킷마다 재현 가능하면서도 서로 다른 시드를 만든다.
		//     여러 Scatter 계열 하위 진형이 겹쳐도 같은 패턴이 반복되지 않게 한다.
		//
		// [7] 各バケットに再現可能かつ異なるシードを割り当てる。
		//     複数の分散系サブフォーメーションが重なっても、
		//     同じパターンの反復を避ける。
		const uint32 DerivedSeed = HashCombineFast(
			GetTypeHash(Context.RandomSeed),
			GetTypeHash(EntryIndex));

		FEnemySubFormationBuildContext SubFormationContext;
		SubFormationContext.MemberCount = MemberCount;
		SubFormationContext.RandomSeed =
			static_cast<int32>(DerivedSeed);

		// [8] 하위 진형 자체 원점 기준의 로컬 슬롯을 생성한다.
		// [8] サブフォーメーション自身の原点を基準として
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

		BucketLayout.LocalSlotTransforms.Reserve(
			SubFormationLocalSlots.Num());

		const FQuat RootRotation =
			SubFormationRootTransform.GetRotation();

		const FVector RootLocation =
			SubFormationRootTransform.GetLocation();

		// [9] 하위 진형 로컬 슬롯을
		//     전체 진형 루트 기준 로컬 슬롯으로 변환한다.
		//
		// [9] サブフォーメーション基準のローカルスロットを、
		//     フォーメーションルート基準へ変換する。
		for (const FTransform& SubFormationLocalSlot
			: SubFormationLocalSlots)
		{
			const FVector FormationLocalLocation =
				RootRotation.RotateVector(
					SubFormationLocalSlot.GetLocation())
				+ RootLocation;

			FQuat FormationLocalRotation =
				RootRotation
				* SubFormationLocalSlot.GetRotation();

			FormationLocalRotation.Normalize();

			BucketLayout.LocalSlotTransforms.Emplace(
				FormationLocalRotation,
				FormationLocalLocation,
				SubFormationLocalSlot.GetScale3D());
		}
	}

	return true;
}