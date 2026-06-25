#include "Enemies/Formations/EnemyFormationProfile.h"

bool UEnemyFormationProfile::TryBuildLayout(
	const FEnemyFormationLayoutContext& Context,
	FEnemyFormationLayout& OutLayout,
	FString* OutError) const
{
	OutLayout.BucketLayouts.Reset();

	if (OutError)
	{
		OutError->Reset();
	}

	// [1] Profile이 완성된 레이아웃 전략을 소유하는지 확인한다.
	// [1] Profileに有効なレイアウト戦略が設定されているか確認する。
	if (!IsValid(LayoutStrategy.Get()))
	{
		if (OutError)
		{
			*OutError = FString::Printf(
				TEXT(
					"%s: レイアウト戦略が設定されていません。"),
				*GetName());
		}

		return false;
	}

	// [2] 입력 검증과 실제 레이아웃 생성을 Strategy에 위임한다.
	// [2] 入力検証と実際のレイアウト生成をStrategyへ委譲する。
	FString StrategyError;

	if (!LayoutStrategy->TryBuildLayout(
		Context,
		OutLayout,
		&StrategyError))
	{
		OutLayout.BucketLayouts.Reset();

		if (StrategyError.IsEmpty())
		{
			StrategyError = TEXT("詳細なエラー情報がありません。");
		}

		if (OutError)
		{
			*OutError = FString::Printf(
				TEXT(
					"%s: フォーメーションレイアウトの生成に"
					"失敗しました。詳細: %s"),
					*GetName(),
					*StrategyError);
		}

		return false;
	}

	return true;
}