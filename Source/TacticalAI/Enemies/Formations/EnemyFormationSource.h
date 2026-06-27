#pragma once

#include "CoreMinimal.h"
#include "Enemies/Formations/EnemyFormationStrategy.h"
#include "EnemyFormationSource.generated.h"

class UEnemyFormationProfile;

// =========================================================================
// 적 진형 설정의 참조 방식.
//
// None은 Override가 없다는 뜻이다.
// Profile은 재사용 가능한 DataAsset 프리셋을 사용한다.
// Inline은 해당 Spawn/Encounter가 직접 소유한 전략 객체를 사용한다.
//
// 敵フォーメーション設定の参照方式。
//
// NoneはOverrideなしを意味する。
// Profileは再利用可能なDataAssetプリセットを使用する。
// Inlineは該当Spawn/Encounterが直接所有するStrategyを使用する。
// =========================================================================
UENUM(BlueprintType)
enum class EEnemyFormationSourceMode : uint8
{
	None UMETA(DisplayName = "なし"),
	Profile UMETA(DisplayName = "プロファイル"),
	Inline UMETA(DisplayName = "インライン")
};

// =========================================================================
// Spawn/Encounter에서 사용할 적 진형 소스.
//
// 이 구조체는 Profile과 Inline Strategy 중 하나를 선택해
// 로컬 레이아웃 생성을 위임한다.
//
// 사용하지 않는 필드에 값이 남아 있어도,
// 실제 선택은 SourceMode만 따른다.
// 예를 들어 SourceMode가 Inline이면 Profile은 무시된다.
//
// Spawn/Encounter用の敵フォーメーションソース。
//
// ProfileまたはInline Strategyのどちらかを選択し、
// ローカルレイアウト生成を委譲する。
//
// 使用しないフィールドに値が残っていても、
// 実際の選択はSourceModeのみで決定する。
// =========================================================================
USTRUCT(BlueprintType)
struct TACTICALAI_API FEnemyFormationSource
{
	GENERATED_BODY()

public:
	// =========================================================================
	// Override가 실제로 지정되어 있는지 확인한다.
	//
	// None이면 호출자가 몬스터 기본 Formation으로 fallback해야 한다.
	//
	// Overrideが実際に指定されているか確認する。
	//
	// Noneの場合、呼び出し側は敵のデフォルトFormationへ
	// fallbackする必要がある。
	// =========================================================================
	bool HasOverride() const;

	// =========================================================================
	// 선택된 소스로 로컬 레이아웃을 생성한다.
	//
	// SourceMode가 None이면 실패한다.
	// None은 오류라기보다 "이 소스로는 생성할 것이 없다"는 상태이며,
	// fallback 판단은 호출자가 수행한다.
	//
	// 選択されたソースでローカルレイアウトを生成する。
	//
	// SourceModeがNoneの場合は失敗する。
	// Noneはエラーというより「このソースでは生成しない」
	// という状態であり、fallback判断は呼び出し側が行う。
	// =========================================================================
	bool TryBuildLayout(
		const FEnemyFormationLayoutContext& Context,
		FEnemyFormationLayout& OutLayout,
		FString* OutError = nullptr) const;

public:
	// =========================================================================
	// 이 Formation Source가 사용할 참조 방식.
	//
	// None이면 Spawn/Encounter Override가 없는 상태로 해석한다.
	//
	// このFormation Sourceが使用する参照方式。
	//
	// Noneの場合、Spawn/Encounter Overrideなしとして扱う。
	// =========================================================================
	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		Category = "敵フォーメーション",
		meta = (DisplayName = "参照方式"))
	EEnemyFormationSourceMode SourceMode = EEnemyFormationSourceMode::None;

	// =========================================================================
	// 재사용 가능한 Formation Profile.
	//
	// SourceMode가 Profile일 때만 사용한다.
	// 여러 Spawn/Encounter에서 같은 설정을 공유하고 싶을 때 선택한다.
	//
	// 再利用可能なFormation Profile。
	//
	// SourceModeがProfileの場合のみ使用する。
	// 複数のSpawn/Encounterで同じ設定を共有したい場合に選択する。
	// =========================================================================
	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		Category = "敵フォーメーション",
		meta = (
			DisplayName = "プロファイル",
			EditCondition = "SourceMode == EEnemyFormationSourceMode::Profile",
			EditConditionHides))
	TObjectPtr<UEnemyFormationProfile> Profile = nullptr;

	// =========================================================================
	// 이 Source를 소유한 객체 안에서 직접 편집하는 Formation Strategy.
	//
	// SourceMode가 Inline일 때만 사용한다.
	// 특정 Spawn/Encounter 전용으로 거리, 오프셋, 하위 진형을
	// 빠르게 커스텀하고 싶을 때 선택한다.
	//
	// このSourceを所有するオブジェクト内で直接編集する
	// Formation Strategy。
	//
	// SourceModeがInlineの場合のみ使用する。
	// 特定のSpawn/Encounter専用に距離・Offset・SubFormationを
	// 素早く調整したい場合に選択する。
	// =========================================================================
	UPROPERTY(
		EditAnywhere,
		Instanced,
		BlueprintReadOnly,
		Category = "敵フォーメーション",
		meta = (
			DisplayName = "インライン戦略",
			EditCondition = "SourceMode == EEnemyFormationSourceMode::Inline",
			EditConditionHides))
	TObjectPtr<UEnemyFormationStrategy> InlineStrategy = nullptr;
};