#include "AbilitySystem/TacticalAbilitySystemComponent.h"
#include "AbilitySystem/Tags/TacticalGameplayTags.h"
#include "Abilities/GameplayAbility.h"
#include "Abilities/GameplayAbilityTypes.h"
#include "GameplayAbilitySpec.h"
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
		// 요청 시 Asset Tag로 Spec을 검색한다.
		// [3] レベル1で付与し、SourceObjectにASC Ownerを記録する。
		// 要求時にAsset TagでSpecを検索する。
		FGameplayAbilitySpec Spec(AbilityClass, 1, INDEX_NONE, GetOwner());
		GiveAbility(Spec);
	}

	UE_LOG(LogTacticalAI, Log, TEXT("初期アビリティを付与しました。Owner=%s, 数=%d"),
		*GetNameSafe(GetOwner()), AbilityClasses.Num());
}

// =======================================================
// AutoComplete Activation Request
//
// Asset Tag로 검색한 Ability에 AutoComplete를 전달하여 발동을 요청한다.
// 입력 유지 상태는 변경하지 않는다.
// 발동한 Ability는 입력 해제를 기다리지 않고 자체 완료 조건으로 종료한다.
//
// Asset Tagで検索したAbilityにAutoCompleteを伝達して発動を要求する。
// 入力維持状態は変更しない。
// 発動したAbilityは入力解放を待たず、自身の完了条件で終了する。
// =======================================================
bool UTacticalAbilitySystemComponent::RequestActivateAutoCompleteAbility(FGameplayTag AbilityTag)
{
	bool bAnyActivated = false;

	ForEachSpecWithTag(AbilityTag, [this, &bAnyActivated](FGameplayAbilitySpec& Spec)
	{
		// [1] 실행 중인 Ability는 다시 발동하거나 실행 방식을 변경하지 않는다.
		// [1] 発動中のAbilityは再発動せず、実行方式も変更しない。
		if (Spec.IsActive()) return;

		// [2] Ability 자체의 완료 조건으로 종료하도록 AutoComplete를 전달한다.
		// 발동 가능 여부는 GAS와 해당 Ability의 활성화 조건에 따라 결정한다.
		// [2] Ability自身の完了条件で終了するよう、AutoCompleteを伝達する。
		// 発動可否はGASと該当Abilityの発動条件に従って判断する。
		if (ActivateAbilityWithMode(Spec.Handle, TacticalGameplayTags::Event_Ability_Activate_AutoComplete))
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
// InputBound Activation Request
//
// Spec에 입력 유지 상태를 기록한다.
// 비활성 Ability는 InputBound를 전달하여 발동을 요청하고,
// 활성 중인 Ability에는 Press 입력을 전달한다.
//
// Specに入力維持状態を記録する。
// 非発動のAbilityにはInputBoundを伝達して発動を要求し、
// 発動中のAbilityにはPress入力を伝達する。
// =======================================================
bool UTacticalAbilitySystemComponent::RequestActivateInputBoundAbility(FGameplayTag AbilityTag)
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
			// 실행 방식은 변경하지 않는다.
			// [2] 発動中のAbilityに標準のPress処理を適用する。
			// 実行方式は変更しない。
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

		// [4] 비활성이면 InputBound를 전달하여 발동을 요청한다.
		// 실패해도 입력 해제 요청 전까지 입력 상태는 유지한다.
		// [4] 非発動ならInputBoundを伝達して発動を要求する。
		// 失敗しても入力解放要求までは入力状態を維持する。
		if (ActivateAbilityWithMode(Spec.Handle, TacticalGameplayTags::Event_Ability_Activate_InputBound))
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
// Release Ability Input
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
void UTacticalAbilitySystemComponent::RequestReleaseAbilityInput(FGameplayTag AbilityTag)
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
// Activate Ability With Mode
//
// 선택한 Spec의 Ability에 실행 방식을 전달하여 발동을 시도한다.
// FGameplayEventData의 EventTag에 실행 방식 태그를 담는다.
// Ability는 ActivateAbility의 TriggerEventData 인자로 이 데이터를 받는다.
//
// 選択したSpecのAbilityに実行方式を伝達して発動を試みる。
// FGameplayEventDataのEventTagに実行方式タグを格納する。
// AbilityはActivateAbilityのTriggerEventData引数でこのデータを受け取る。
// =======================================================
bool UTacticalAbilitySystemComponent::ActivateAbilityWithMode(FGameplayAbilitySpecHandle AbilityHandle, const FGameplayTag& ExecutionModeTag)
{
	// [1] 지원하는 실행 방식인지 확인한다.
	// [1] 対応する実行方式か確認する。
	if (ExecutionModeTag != TacticalGameplayTags::Event_Ability_Activate_InputBound &&
		ExecutionModeTag != TacticalGameplayTags::Event_Ability_Activate_AutoComplete)
	{
		UE_LOG(LogTacticalAI, Warning, TEXT("未対応の実行方式です。Owner=%s, Mode=%s"),
			*GetNameSafe(GetOwner()), *ExecutionModeTag.ToString());
		return false;
	}

	// [2] 발동에 필요한 ActorInfo와 Owner·Avatar가 유효한지 확인한다.
	// [2] 発動に必要なActorInfoとOwner・Avatarの有効性を確認する。
	if (!AbilityActorInfo.IsValid() ||
		!AbilityActorInfo->OwnerActor.IsValid() ||
		!AbilityActorInfo->AvatarActor.IsValid())
	{
		return false;
	}

	// [3] 발동할 Spec이 유효한지 확인한다.
	// 이미 실행 중인 Ability의 재발동과 실행 방식 변경은 허용하지 않는다.
	// [3] 発動するSpecの有効性を確認する。
	// 発動中のAbilityの再発動と実行方式の変更は許可しない。
	const FGameplayAbilitySpec* Spec = FindAbilitySpecFromHandle(AbilityHandle);
	if (!Spec || !Spec->Ability || Spec->IsActive())
	{
		return false;
	}

	// [4] Ability에 전달할 데이터에 실행 방식과 발동 주체를 기록한다.
	// Instigator는 발동하는 캐릭터이며, 공격 대상은 여기서 결정하지 않는다.
	// [4] Abilityへ渡すデータに実行方式と発動主体を記録する。
	// Instigatorは発動するキャラクターであり、攻撃対象はここでは決定しない。
	FGameplayEventData Payload;
	Payload.EventTag = ExecutionModeTag;
	Payload.Instigator = AbilityActorInfo->AvatarActor.Get();

	// [5] 선택한 Spec을 직접 지정하여 데이터와 함께 발동한다.
	// 실행 방식 태그로 다른 Ability를 검색하거나 이벤트를 일괄 전달하지 않는다.
	// [5] 選択したSpecを直接指定し、データとともに発動する。
	// 実行方式タグによる別Abilityの検索や、イベントの一斉送信は行わない。
	return TriggerAbilityFromGameplayEvent(
		AbilityHandle,
		AbilityActorInfo.Get(),
		ExecutionModeTag,
		&Payload,
		*this);
}

// =======================================================
// Ability Spec Search
//
// 지정한 Asset Tag와 정확히 일치하는 모든 Spec에 Callback을 실행한다.
// 순회 중 Ability 추가·제거 요청이 발생하면,
// 실제 목록 변경은 잠금이 해제될 때까지 지연한다.
//
// 指定Asset Tagと完全一致する全SpecにCallbackを実行する。
// 走査中にAbilityの追加・削除が要求された場合、
// 実際のリスト変更はロック解除まで遅延する。
// =======================================================
void UTacticalAbilitySystemComponent::ForEachSpecWithTag(const FGameplayTag& AbilityTag, TFunctionRef<void(FGameplayAbilitySpec&)> Callback)
{
	if (!AbilityTag.IsValid()) return;

	// [1] Callback 실행 중 발생하는 목록 변경을 잠금 해제까지 지연한다.
	// [1] Callback実行中に発生するリスト変更をロック解除まで遅延する。
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