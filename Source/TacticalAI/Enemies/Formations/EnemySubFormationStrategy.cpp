#include "Enemies/Formations/EnemySubFormationStrategy.h"

bool UEnemySubFormationStrategy::TryBuildLocalSlots(
	const FEnemySubFormationBuildContext& Context,
	TArray<FTransform>& OutLocalSlotTransforms,
	FString* OutError) const
{
	OutLocalSlotTransforms.Reset();

	if (OutError)
	{
		OutError->Reset();
	}

	// [1] 음수 인원 입력 방어.
	// [1] 負数の人数は呼び出し側の契約違反として扱う。
	if (Context.MemberCount < 0)
	{
		if (OutError)
		{
			*OutError = FString::Printf(
				TEXT("%s: メンバー数に負数が指定されています（%d）。"),
				*GetClass()->GetName(),
				Context.MemberCount);
		}

		return false;
	}

	// [2] 비어 있는 하위 진형은 정상 입력.
	// [2] 空のサブフォーメーションは正常入力として扱う。
	if (Context.MemberCount == 0)
	{
		return true;
	}

	OutLocalSlotTransforms.Reserve(Context.MemberCount);

	// [3] 구체 전략이 이상적인 로컬 슬롯을 생성한다.
	// [3] 具体戦略が理想的なローカルスロットを生成する。
	FString StrategyError;

	if (!BuildLocalSlotsInternal(
		Context,
		OutLocalSlotTransforms,
		StrategyError))
	{
		OutLocalSlotTransforms.Reset();

		if (StrategyError.IsEmpty())
		{
			StrategyError = FString::Printf(
				TEXT("%s: 敵サブフォーメーションのスロット生成に失敗しました。"),
				*GetClass()->GetName());
		}

		if (OutError)
		{
			*OutError = MoveTemp(StrategyError);
		}

		return false;
	}

	// [4] 한 몬스터당 하나의 슬롯이 생성됐는지 확인한다.
	// [4] 敵1体につき1個のスロットが生成されたか確認する。
	if (OutLocalSlotTransforms.Num() != Context.MemberCount)
	{
		const int32 GeneratedSlotCount = OutLocalSlotTransforms.Num();

		OutLocalSlotTransforms.Reset();

		if (OutError)
		{
			*OutError = FString::Printf(
				TEXT(
					"%s: %d体の入力に対して%d個のローカルスロットが生成されました。"
					"入力数とスロット数は一致している必要があります。"),
				*GetClass()->GetName(),
				Context.MemberCount,
				GeneratedSlotCount);
		}

		return false;
	}

	// [5] 잘못된 수학 결과가 이동 계층으로 전파되지 않게 막는다.
	// [5] 不正な計算結果が移動処理へ伝播しないように検証する。
	for (const FTransform& LocalSlotTransform : OutLocalSlotTransforms)
	{
		if (LocalSlotTransform.ContainsNaN())
		{
			OutLocalSlotTransforms.Reset();

			if (OutError)
			{
				*OutError = FString::Printf(
					TEXT(
						"%s: 生成されたローカルスロットの変換値に"
						"非数値（NaN）が含まれています。"),
					*GetClass()->GetName());
			}

			return false;
		}
	}

	return true;
}