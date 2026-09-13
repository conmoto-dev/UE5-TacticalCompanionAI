#include "AbilitySystem/TacticalAbilitySystemComponent.h"
#include "Abilities/GameplayAbility.h"
#include "TacticalAI.h"

// =======================================================
// Startup Abilities
//
// 전달받은 Ability 클래스를 Spec으로 생성하여 ASC에 한 번만 부여한다.
// 부여할 Ability 목록의 구성은 호출자가 담당한다.
//
// 受け取ったAbilityクラスからSpecを生成し、ASCへ一度だけ付与する。
// 付与するAbility一覧の構成は呼び出し元が担当する。
// =======================================================
void UTacticalAbilitySystemComponent::GiveStartupAbilities(const TArray<TSubclassOf<UGameplayAbility>>& AbilityClasses)
{
	// [1] 시작 Ability의 반복 부여를 방지한다.
	// [1] 初期Abilityの重複付与を防止する。
	if (bStartupAbilitiesGiven) return;
	bStartupAbilitiesGiven = true;

	for (const TSubclassOf<UGameplayAbility>& AbilityClass : AbilityClasses)
	{
		// [2] 미설정 항목은 건너뛴다.
		// [2] 未設定の項目はスキップする。
		if (!AbilityClass)
		{
			UE_LOG(LogTacticalAI, Warning, TEXT("初期アビリティにnullクラスが含まれています。Owner=%s"),
				*GetNameSafe(GetOwner()));
			continue;
		}

		// [3] 레벨 1로 부여하고, SourceObject에 ASC Owner를 기록한다.
		// 숫자 InputID는 사용하지 않으며, 요청 시 Asset Tag로 Spec을 검색한다.
		// [3] レベル1で付与し、SourceObjectにASC Ownerを記録する。
		// 数値InputIDは使用せず、要求時にAsset TagでSpecを検索する。
		FGameplayAbilitySpec Spec(AbilityClass, 1, INDEX_NONE, GetOwner());
		GiveAbility(Spec);
	}

	UE_LOG(LogTacticalAI, Log, TEXT("初期アビリティを付与しました。Owner=%s, 数=%d"),
		*GetNameSafe(GetOwner()), AbilityClasses.Num());
}

// =======================================================
// Player Requests
//
// 플레이어 입력에서 들어온 요청을 실행 방식별 공통 처리에 전달한다.
// プレイヤー入力からの要求を、実行方式ごとの共通処理へ渡す。
// =======================================================
bool UTacticalAbilitySystemComponent::RequestActivateAbilityFromPlayer(FGameplayTag AbilityTag)
{
	return ActivateAbilityInternal(AbilityTag);
}

bool UTacticalAbilitySystemComponent::RequestPressAbilityFromPlayer(FGameplayTag AbilityTag)
{
	return PressAbilityInternal(AbilityTag);
}

void UTacticalAbilitySystemComponent::RequestReleaseAbilityFromPlayer(FGameplayTag AbilityTag)
{
	ReleaseAbilityInternal(AbilityTag);
}

// =======================================================
// AI Requests
//
// AI가 행동 조건을 판단한 뒤 전달한 요청을 처리한다.
// ASC는 타깃 선정이나 공격 거리 판단을 수행하지 않는다.
//
// AIが行動条件を判断した後に送る要求を処理する。
// ASCではターゲット選択や攻撃距離の判断を行わない。
// =======================================================
bool UTacticalAbilitySystemComponent::RequestActivateAbilityFromAI(FGameplayTag AbilityTag)
{
	return ActivateAbilityInternal(AbilityTag);
}

bool UTacticalAbilitySystemComponent::RequestPressAbilityFromAI(FGameplayTag AbilityTag)
{
	return PressAbilityInternal(AbilityTag);
}

void UTacticalAbilitySystemComponent::RequestReleaseAbilityFromAI(FGameplayTag AbilityTag)
{
	ReleaseAbilityInternal(AbilityTag);
}

// =======================================================
// Single Activation
//
// 입력 유지 상태를 변경하지 않고 Ability 활성화를 시도한다.
// 入力維持状態を変更せず、Abilityの発動を試みる。
// =======================================================
bool UTacticalAbilitySystemComponent::ActivateAbilityInternal(const FGameplayTag& AbilityTag)
{
	bool bAnyActivated = false;

	ForEachSpecWithTag(AbilityTag, [this, &bAnyActivated](FGameplayAbilitySpec& Spec)
	{
		// [1] 발동 가능 여부는 GAS와 해당 Ability의 활성화 조건에 따라 결정한다.
		// [1] 発動可否はGASと該当Abilityの発動条件に従って判断する。
		if (TryActivateAbility(Spec.Handle))
		{
			bAnyActivated = true;
		}
		else
		{
			UE_LOG(LogTacticalAI, Verbose, TEXT("アビリティの発動に失敗しました。Owner=%s, Ability=%s"),
				*GetNameSafe(GetOwner()), *GetNameSafe(Spec.Ability));
		}
	});

	return bAnyActivated;
}

// =======================================================
// Begin Ability Hold
//
// Spec에 입력 유지 상태를 기록한다.
// 비활성 Ability는 활성화를 시도하고,
// 활성 중인 Ability에는 Press 입력을 전달한다.
//
// Specに入力維持状態を記録する。
// 非発動Abilityは発動を試み、
// 発動中のAbilityにはPress入力を伝達する。
// =======================================================
bool UTacticalAbilitySystemComponent::PressAbilityInternal(const FGameplayTag& AbilityTag)
{
	bool bAnyHeld = false;

	ForEachSpecWithTag(AbilityTag, [this, &bAnyHeld](FGameplayAbilitySpec& Spec)
	{
		// [1] 활성화 중 생성되는 Input Task가 유지 상태를 확인할 수 있도록 먼저 기록한다.
		// [1] 発動時に生成されるInput Taskが維持状態を確認できるよう、先に記録する。
		Spec.InputPressed = true;

		if (Spec.IsActive())
		{
			// [2] 활성 중인 Ability에 표준 Press 처리를 적용한다.
			// [2] 発動中のAbilityに標準のPress処理を適用する。
			AbilitySpecInputPressed(Spec);

			// [3] 현재 인스턴스의 활성화에 InputPressed Generic Event를 전달한다.
			// [3] 現在のインスタンスの発動にInputPressed Generic Eventを伝達する。
			if (const UGameplayAbility* PrimaryInstance = Spec.GetPrimaryInstance())
			{
				InvokeReplicatedEvent(
					EAbilityGenericReplicatedEvent::InputPressed,
					Spec.Handle,
					PrimaryInstance->GetCurrentActivationInfo().GetActivationPredictionKey());
			}

			bAnyHeld = true;
			return;
		}

		// [4] 비활성이면 활성화를 시도한다. 실패해도 유지 해제 요청 전까지 입력 상태는 유지한다.
		// [4] 非発動なら発動を試みる。失敗しても維持解除要求までは入力状態を維持する。
		if (TryActivateAbility(Spec.Handle))
		{
			bAnyHeld = true;
		}
		else
		{
			UE_LOG(LogTacticalAI, Verbose, TEXT("アビリティの発動に失敗しました。Owner=%s, Ability=%s"),
				*GetNameSafe(GetOwner()), *GetNameSafe(Spec.Ability));
		}
	});

	return bAnyHeld;
}

// =======================================================
// End Ability Hold
//
// Spec의 입력 유지 상태를 해제하고,
// 활성 중인 Ability에 Release 입력을 전달한다.
//
// Ability를 직접 종료하거나 취소하지 않는다.
// 해제 이후의 실행 흐름은 해당 Ability가 결정한다.
//
// Specの入力維持状態を解除し、
// 発動中のAbilityへRelease入力を伝達する。
//
// Abilityを直接終了・キャンセルしない。
// 解除後の実行フローは該当Abilityが決定する。
// =======================================================
void UTacticalAbilitySystemComponent::ReleaseAbilityInternal(const FGameplayTag& AbilityTag)
{
	ForEachSpecWithTag(AbilityTag, [this](FGameplayAbilitySpec& Spec)
	{
		// [1] 발동에 실패했거나 이미 종료된 Ability도 입력 유지 상태를 해제한다.
		// [1] 発動に失敗したAbilityや終了済みのAbilityも入力維持状態を解除する。
		Spec.InputPressed = false;

		if (!Spec.IsActive()) return;

		// [2] 활성 중인 Ability에 표준 Release 처리를 적용한다.
		// [2] 発動中のAbilityに標準のRelease処理を適用する。
		AbilitySpecInputReleased(Spec);

		// [3] 현재 인스턴스의 활성화에 InputReleased Generic Event를 전달한다.
		// [3] 現在のインスタンスの発動にInputReleased Generic Eventを伝達する。
		if (const UGameplayAbility* PrimaryInstance = Spec.GetPrimaryInstance())
		{
			InvokeReplicatedEvent(
				EAbilityGenericReplicatedEvent::InputReleased,
				Spec.Handle,
				PrimaryInstance->GetCurrentActivationInfo().GetActivationPredictionKey());
		}
	});
}

// =======================================================
// Ability Spec Search
//
// 지정한 Asset Tag와 정확히 일치하는 모든 Spec에 Callback을 실행한다.
// Callback에서 Ability 부여·제거가 발생할 수 있으므로,
// Scope Lock으로 순회 중 목록의 구조 변경을 지연시킨다.
//
// 指定Asset Tagと完全一致する全SpecにCallbackを実行する。
// Callback内でAbilityの付与・削除が発生する可能性があるため、
// Scope Lockで走査中のリスト構造の変更を遅延させる。
// =======================================================
void UTacticalAbilitySystemComponent::ForEachSpecWithTag(const FGameplayTag& AbilityTag, TFunctionRef<void(FGameplayAbilitySpec&)> Callback)
{
	if (!AbilityTag.IsValid()) return;

	// [1] Callback 실행 중 Ability 목록의 순회를 보호한다.
	// [1] Callback実行中のAbilityリスト走査を保護する。
	ABILITYLIST_SCOPE_LOCK();

	for (FGameplayAbilitySpec& Spec : GetActivatableAbilities())
	{
		// [2] Ability가 없거나 Asset Tag가 정확히 일치하지 않는 Spec은 제외한다.
		// [2] Abilityが存在しない、またはAsset Tagが完全一致しないSpecは除外する。
		if (!Spec.Ability) continue;
		if (!Spec.Ability->GetAssetTags().HasTagExact(AbilityTag)) continue;

		// [3] 일치한 Spec에 요청 처리를 적용한다.
		// [3] 一致したSpecに要求処理を適用する。
		Callback(Spec);
	}
}