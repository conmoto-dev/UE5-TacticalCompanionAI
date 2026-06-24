#include "Enemies/Formations/EnemyFormationStrategy.h"

bool UEnemyFormationStrategy::TryBuildLayout(
	const FEnemyFormationLayoutContext& Context,
	FEnemyFormationLayout& OutLayout,
	FString* OutError) const
{
	OutLayout.BucketLayouts.Reset();

	if (OutError)
	{
		OutError->Reset();
	}

	// [1] 버킷 자체가 없는 입력은 유효한 진형 레이아웃이 아니다.
	// [1] 入力バケット自体が存在しない設定は無効とする。
	if (Context.BucketMemberCounts.IsEmpty())
	{
		if (OutError)
		{
			*OutError = FString::Printf(
				TEXT("%s: 入力バケットが指定されていません。"),
				*GetClass()->GetName());
		}

		return false;
	}

	// [2] 각 버킷의 인원수를 검사하고 전체 인원수를 계산한다.
	// [2] 各バケットの人数を検証し、全体人数を集計する。
	int64 TotalMemberCount = 0;

	for (int32 BucketIndex = 0;
		BucketIndex < Context.BucketMemberCounts.Num();
		++BucketIndex)
	{
		const int32 MemberCount = Context.BucketMemberCounts[BucketIndex];

		if (MemberCount < 0)
		{
			if (OutError)
			{
				*OutError = FString::Printf(
					TEXT(
						"%s: 入力バケット%dのメンバー数に"
						"負数が指定されています（%d）。"),
					*GetClass()->GetName(),
					BucketIndex + 1,
					MemberCount);
			}

			return false;
		}

		TotalMemberCount += static_cast<int64>(MemberCount);
	}

	// [3] 개별 빈 버킷은 허용하지만 전체가 비어 있으면 실패한다.
	// [3] 個別の空バケットは許可するが、
	//     すべてのバケットが空の場合は失敗とする。
	if (TotalMemberCount == 0)
	{
		if (OutError)
		{
			*OutError = FString::Printf(
				TEXT(
					"%s: すべての入力バケットが空です。"
					"1つ以上のバケットにメンバーを割り当ててください。"),
				*GetClass()->GetName());
		}

		return false;
	}

	// [4] 구체 전략이 진형 루트 기준의 로컬 레이아웃을 생성한다.
	// [4] 具体戦略がフォーメーションルート基準の
	//     ローカルレイアウトを生成する。
	FString StrategyError;

	if (!BuildLayoutInternal(Context, OutLayout, StrategyError))
	{
		OutLayout.BucketLayouts.Reset();

		if (StrategyError.IsEmpty())
		{
			StrategyError = FString::Printf(
				TEXT(
					"%s: 敵フォーメーションの"
					"ローカルレイアウト生成に失敗しました。"),
				*GetClass()->GetName());
		}

		if (OutError)
		{
			*OutError = MoveTemp(StrategyError);
		}

		return false;
	}

	// [5] 입력과 출력의 버킷 구조가 동일한지 검사한다.
	// [5] 入力と出力のバケット構造が一致しているか確認する。
	if (OutLayout.BucketLayouts.Num()
		!= Context.BucketMemberCounts.Num())
	{
		const int32 GeneratedBucketCount = OutLayout.BucketLayouts.Num();

		OutLayout.BucketLayouts.Reset();

		if (OutError)
		{
			*OutError = FString::Printf(
				TEXT(
					"%s: 入力バケット数は%dですが、"
					"出力バケット数は%dです。"),
				*GetClass()->GetName(),
				Context.BucketMemberCounts.Num(),
				GeneratedBucketCount);
		}

		return false;
	}

	// [6] 각 버킷이 몬스터 수와 같은 수의 슬롯을 만들었는지 검사한다.
	// [6] 各バケットが敵数と同数のスロットを
	//     生成したか確認する。
	for (int32 BucketIndex = 0;
		BucketIndex < Context.BucketMemberCounts.Num();
		++BucketIndex)
	{
		const int32 ExpectedSlotCount = Context.BucketMemberCounts[BucketIndex];

		const TArray<FTransform>& LocalSlotTransforms =
			OutLayout.BucketLayouts[BucketIndex].LocalSlotTransforms;

		if (LocalSlotTransforms.Num() != ExpectedSlotCount)
		{
			const int32 GeneratedSlotCount = LocalSlotTransforms.Num();

			OutLayout.BucketLayouts.Reset();

			if (OutError)
			{
				*OutError = FString::Printf(
					TEXT(
						"%s: 入力バケット%dは%d体ですが、"
						"%d個のローカルスロットが生成されました。"),
					*GetClass()->GetName(),
					BucketIndex + 1,
					ExpectedSlotCount,
					GeneratedSlotCount);
			}

			return false;
		}

		// [7] 잘못된 수학 결과가 후속 계층으로 전파되지 않게 막는다.
		// [7] 不正な計算結果が後段の処理へ
		//     伝播しないように検証する。
		for (const FTransform& LocalSlotTransform : LocalSlotTransforms)
		{
			if (LocalSlotTransform.ContainsNaN())
			{
				OutLayout.BucketLayouts.Reset();

				if (OutError)
				{
					*OutError = FString::Printf(
						TEXT(
							"%s: 入力バケット%dのローカルスロットに"
							"非数値（NaN）が含まれています。"),
						*GetClass()->GetName(),
						BucketIndex + 1);
				}

				return false;
			}
		}
	}

	return true;
}