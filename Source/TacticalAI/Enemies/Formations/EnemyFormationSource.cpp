#include "Enemies/Formations/EnemyFormationSource.h"
#include "Enemies/Formations/EnemyFormationProfile.h"

bool FEnemyFormationSource::HasOverride() const
{
	return SourceMode != EEnemyFormationSourceMode::None;
}

bool FEnemyFormationSource::TryBuildLayout(
	const FEnemyFormationLayoutContext& Context,
	FEnemyFormationLayout& OutLayout,
	FString* OutError) const
{
	OutLayout.BucketLayouts.Reset();

	if (OutError)
	{
		OutError->Reset();
	}

	switch (SourceMode)
	{
	case EEnemyFormationSourceMode::None:
		{
			if (OutError)
			{
				*OutError = TEXT("フォーメーションソースが指定されていません。");
			}

			return false;
		}

	case EEnemyFormationSourceMode::Profile:
		{
			// [1] 재사용 Profile이 유효한지 확인한다.
			// [1] 再利用Profileが有効か確認する。
			if (!IsValid(Profile.Get()))
			{
				if (OutError)
				{
					*OutError = TEXT("フォーメーションプロファイルが設定されていません。");
				}

				return false;
			}

			// [2] Profile 내부 Strategy에 레이아웃 생성을 위임한다.
			// [2] Profile内部のStrategyへレイアウト生成を委譲する。
			FString ProfileError;

			if (!Profile->TryBuildLayout(
				Context,
				OutLayout,
				&ProfileError))
			{
				OutLayout.BucketLayouts.Reset();

				if (ProfileError.IsEmpty())
				{
					ProfileError = TEXT("詳細なエラー情報がありません。");
				}

				if (OutError)
				{
					*OutError = FString::Printf(
						TEXT(
							"フォーメーションプロファイルによる"
							"レイアウト生成に失敗しました。詳細: %s"),
							*ProfileError);
				}

				return false;
			}

			return true;
		}

	case EEnemyFormationSourceMode::Inline:
		{
			// [3] 이 Spawn/Encounter가 직접 소유한 Strategy가 유효한지 확인한다.
			// [3] このSpawn/Encounterが直接所有するStrategyが有効か確認する。
			if (!IsValid(InlineStrategy.Get()))
			{
				if (OutError)
				{
					*OutError = TEXT("インライン戦略が設定されていません。");
				}

				return false;
			}

			// [4] Inline Strategy에 직접 레이아웃 생성을 위임한다.
			// [4] Inline Strategyへ直接レイアウト生成を委譲する。
			FString StrategyError;

			if (!InlineStrategy->TryBuildLayout(
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
							"インライン戦略による"
							"レイアウト生成に失敗しました。詳細: %s"),
						*StrategyError);
				}

				return false;
			}

			return true;
		}

	default:
		break;
	}

	if (OutError)
	{
		*OutError = FString::Printf(
			TEXT( "未対応のフォーメーションソース方式です（%d）。"),
			static_cast<int32>(SourceMode));
	}

	return false;
}