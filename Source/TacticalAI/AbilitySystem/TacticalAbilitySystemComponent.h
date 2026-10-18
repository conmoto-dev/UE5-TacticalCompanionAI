#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "GameplayTagContainer.h"
#include "TacticalAbilitySystemComponent.generated.h"

class UGameplayAbility;

// =======================================================
// Tactical Ability System Component
//
// 캐릭터의 시작 Ability를 부여하고,
// Asset Tag로 검색한 Ability에 실행 방식을 명시하여 발동을 요청한다.
//
// AutoComplete: 입력 해제를 기다리지 않고 Ability 자체의 완료 조건으로 종료한다.
// InputBound: 입력 누름·해제를 사용하며, 입력에 대한 반응은 Ability가 결정한다.
//
// 플레이어와 AI는 필요한 실행 방식의 요청 함수를 공통으로 사용한다.
// 발동 후 실행 흐름과 종료 시점은 각 Gameplay Ability가 담당한다.
//
// キャラクターの初期Abilityを付与し、
// Asset Tagで検索したAbilityに実行方式を明示して発動を要求する。
//
// AutoComplete: 入力解放を待たず、Ability自身の完了条件で終了する。
// InputBound: 入力の押下・解放を使用し、入力への反応はAbilityが決定する。
//
// プレイヤーとAIは、必要な実行方式の要求関数を共通で使用する。
// 発動後の実行フローと終了タイミングは各Gameplay Abilityが担当する。
// =======================================================
UCLASS()
class TACTICALAI_API UTacticalAbilitySystemComponent : public UAbilitySystemComponent
{
	GENERATED_BODY()

public:
	// =======================================================
	// Startup Abilities
	// =======================================================

	// 시작 Ability를 한 번만 부여한다. 서버 권한과 ActorInfo 초기화 후 호출한다.
	// 初期Abilityを一度だけ付与する。サーバー権限でActorInfo初期化後に呼び出す。
	void GiveStartupAbilities(const TArray<TSubclassOf<UGameplayAbility>>& AbilityClasses);

	// =======================================================
	// Ability Requests
	//
	// 모든 요청은 Ability의 Asset Tag를 기준으로 처리한다.
	// 타깃·거리 등 AI의 행동 조건은 호출 전에 AI가 판단한다.
	//
	// すべての要求はAbilityのAsset Tagを基準に処理する。
	// ターゲット・距離などAIの行動条件は、呼び出し前にAIが判断する。
	// =======================================================

	// Ability에 AutoComplete를 전달하여 발동을 요청한다.
	// 발동한 Ability는 입력 해제를 기다리지 않고 자체 완료 조건으로 종료한다.
	// Spec의 입력 유지 상태는 변경하지 않는다.
	// 하나 이상의 발동 시도가 성공하면 true를 반환한다. 실행 완료를 의미하지는 않는다.
	//
	// AbilityにAutoCompleteを伝達して発動を要求する。
	// 発動したAbilityは入力解放を待たず、自身の完了条件で終了する。
	// Specの入力維持状態は変更しない。
	// 一つ以上の発動試行が成功した場合はtrueを返す。実行完了を意味するものではない。
	UFUNCTION(BlueprintCallable, Category = "Tactical Ability|Requests")
	bool RequestActivateAutoCompleteAbility(FGameplayTag AbilityTag);

	// Spec에 입력 누름을 기록하고, 비활성 Ability에 InputBound를 전달하여 발동을 요청한다.
	// 이미 실행 중이면 실행 방식을 변경하지 않고 입력 누름만 전달한다.
	// 하나 이상의 실행 중인 Ability에 입력을 전달하거나 새 발동에 성공하면 true를 반환한다.
	//
	// Specに入力押下を記録し、非発動のAbilityにInputBoundを伝達して発動を要求する。
	// 発動中の場合は実行方式を変更せず、入力押下のみを伝達する。
	// 一つ以上の発動中のAbilityに入力を伝達するか、新規発動に成功した場合はtrueを返す。
	UFUNCTION(BlueprintCallable, Category = "Tactical Ability|Requests")
	bool RequestActivateInputBoundAbility(FGameplayTag AbilityTag);

	// Spec의 입력 유지 상태를 해제하고, 실행 중인 Ability에 입력 해제를 전달한다.
	// 해제 이후의 실행 흐름과 종료 시점은 Ability가 결정한다.
	//
	// Specの入力維持状態を解除し、発動中のAbilityへ入力解放を伝達する。
	// 解放後の実行フローと終了タイミングはAbilityが決定する。
	UFUNCTION(BlueprintCallable, Category = "Tactical Ability|Requests")
	void RequestReleaseAbilityInput(FGameplayTag AbilityTag);

private:
	// =======================================================
	// Shared Activation
	// =======================================================

	// 선택한 Spec의 Ability에 실행 방식을 전달하여 발동을 시도한다.
	// InputBound와 AutoComplete 발동 요청이 이 처리를 공유한다.
	//
	// 選択したSpecのAbilityに実行方式を伝達して発動を試みる。
	// InputBoundとAutoCompleteの発動要求がこの処理を共有する。
	bool ActivateAbilityWithMode(FGameplayAbilitySpecHandle AbilityHandle, const FGameplayTag& ExecutionModeTag);

	// 지정한 Asset Tag와 정확히 일치하는 모든 Spec에 Callback을 실행한다.
	// 순회 중 Ability 목록 변경은 Scope Lock으로 보호한다.
	// 指定Asset Tagと完全一致する全SpecにCallbackを実行する。
	// 走査中のAbilityリスト変更はScope Lockで保護する。
	void ForEachSpecWithTag(const FGameplayTag& AbilityTag, TFunctionRef<void(FGameplayAbilitySpec&)> Callback);

	// 시작 Ability의 중복 부여 방지.
	// 初期Abilityの重複付与防止。
	bool bStartupAbilitiesGiven = false;
};