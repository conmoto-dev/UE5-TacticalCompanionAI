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
// 플레이어·AI의 태그 기반 Ability 요청을 처리한다.
//
// 요청 출처별 공개 함수를 구분하며,
// 단발 발동·유지 시작·유지 해제의 실제 처리는 내부에서 공유한다.
//
// 공격 요청 시점은 입력 처리·AI가 결정하고,
// 발동 후 실행 흐름과 종료 시점은 각 Gameplay Ability가 담당한다.
//
// キャラクターの初期Abilityを付与し、
// プレイヤー・AIからのタグによるAbility要求を処理する。
//
// 要求元ごとに公開関数を分け、
// 単発発動・維持開始・維持解除の処理は内部で共有する。
//
// 要求タイミングは入力処理・AIが判断し、
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
	// Player Requests
	//
	// 모든 요청은 Ability의 Asset Tag를 기준으로 처리한다.
	// すべての要求はAbilityのAsset Tagを基準に処理する。
	// =======================================================

	// 플레이어의 단발 발동 요청. 입력 유지 상태는 변경하지 않는다.
	// 하나 이상의 활성화 시도가 성공하면 true를 반환한다.
	// プレイヤーの単発発動要求。入力維持状態は変更しない。
	// 一つ以上の発動試行が成功した場合はtrueを返す。
	UFUNCTION(BlueprintCallable, Category = "Tactical Ability|Player")
	bool RequestActivateAbilityFromPlayer(FGameplayTag AbilityTag);

	// 플레이어의 유지 시작 요청. 하나 이상 활성 중이거나 새로 활성화되면 true.
	// プレイヤーの維持開始要求。一つ以上が発動中、または新しく発動した場合はtrue。
	UFUNCTION(BlueprintCallable, Category = "Tactical Ability|Player")
	bool RequestPressAbilityFromPlayer(FGameplayTag AbilityTag);

	// 플레이어의 유지 해제 요청. Ability에 입력 해제를 전달하며 강제 취소하지 않는다.
	// プレイヤーの維持解除要求。Abilityへ入力解放を伝達し、強制キャンセルは行わない。
	UFUNCTION(BlueprintCallable, Category = "Tactical Ability|Player")
	void RequestReleaseAbilityFromPlayer(FGameplayTag AbilityTag);

	// =======================================================
	// AI Requests
	//
	// 타깃·거리 등 AI의 행동 조건은 호출 전에 AI가 판단한다.
	// ターゲット・距離などの行動条件は、呼び出し前にAIが判断する。
	// =======================================================

	// AI의 단발 발동 요청. 입력 유지 상태는 변경하지 않는다.
	// 하나 이상의 활성화 시도가 성공하면 true를 반환한다.
	// AIの単発発動要求。入力維持状態は変更しない。
	// 一つ以上の発動試行が成功した場合はtrueを返す。
	UFUNCTION(BlueprintCallable, Category = "Tactical Ability|AI")
	bool RequestActivateAbilityFromAI(FGameplayTag AbilityTag);

	// AI의 유지 시작 요청. 하나 이상 활성 중이거나 새로 활성화되면 true.
	// AIの維持開始要求。一つ以上が発動中、または新しく発動した場合はtrue。
	UFUNCTION(BlueprintCallable, Category = "Tactical Ability|AI")
	bool RequestPressAbilityFromAI(FGameplayTag AbilityTag);

	// AI의 유지 해제 요청. 해제 이후의 실행과 종료 시점은 Ability가 결정한다.
	// AIの維持解除要求。解除後の処理と終了タイミングはAbilityが決定する。
	UFUNCTION(BlueprintCallable, Category = "Tactical Ability|AI")
	void RequestReleaseAbilityFromAI(FGameplayTag AbilityTag);

private:
	// =======================================================
	// Shared Request Processing
	//
	// 플레이어·AI 공개 함수가 공유하는 실제 처리.
	// 유지 해제는 Ability 취소와 구분한다.
	//
	// プレイヤー・AIの公開関数が共有する処理。
	// 維持解除とAbilityのキャンセルを区別する。
	// =======================================================

	bool ActivateAbilityInternal(const FGameplayTag& AbilityTag);
	bool PressAbilityInternal(const FGameplayTag& AbilityTag);
	void ReleaseAbilityInternal(const FGameplayTag& AbilityTag);

	// 지정한 Asset Tag와 정확히 일치하는 모든 Spec에 Callback을 실행한다.
	// 순회 중 Ability 목록 변경은 Scope Lock으로 보호한다.
	// 指定Asset Tagと完全一致する全SpecにCallbackを実行する。
	// 走査中のAbilityリスト変更はScope Lockで保護する。
	void ForEachSpecWithTag(const FGameplayTag& AbilityTag, TFunctionRef<void(FGameplayAbilitySpec&)> Callback);

	// 시작 Ability의 중복 부여 방지.
	// 初期Abilityの重複付与防止。
	bool bStartupAbilitiesGiven = false;
};