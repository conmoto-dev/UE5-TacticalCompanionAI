#include "AbilitySystem/Abilities/TacticalBasicAttackAbility.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystem/Attributes/TacticalCombatAttributeSet.h"
#include "AbilitySystem/Tags/TacticalGameplayTags.h"
#include "AI/Targeting/TargetSelectorComponent.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "Abilities/Tasks/AbilityTask_WaitInputRelease.h"
#include "Abilities/GameplayAbilityTypes.h"
#include "GameplayAbilitySpec.h"
#include "GameplayEffect.h"
#include "TacticalAI.h"

UTacticalBasicAttackAbility::UTacticalBasicAttackAbility()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

	FGameplayTagContainer AssetTags = GetAssetTags();
	AssetTags.AddTag(TacticalGameplayTags::Ability_Attack_Basic);
	SetAssetTags(AssetTags);

	ActivationOwnedTags.AddTag(TacticalGameplayTags::State_Attacking);

	ActivationBlockedTags.AddTag(TacticalGameplayTags::State_Attacking);
	ActivationBlockedTags.AddTag(TacticalGameplayTags::State_Dead);
}

// =======================================================
// 평타 발동 조건
//
// GAS 발동 조건과 필수 공격 설정을 검사한다.
// 타깃 유무는 평타 발동을 제한하지 않는다.
//
// 通常攻撃の発動条件。
// GASの発動条件と必須の攻撃設定を検証する。
// ターゲットの有無では通常攻撃の発動を制限しない。
// =======================================================
bool UTacticalBasicAttackAbility::CanActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayTagContainer* SourceTags,
	const FGameplayTagContainer* TargetTags,
	FGameplayTagContainer* OptionalRelevantTags) const
{
	// [1] 태그·코스트·쿨다운 등 GAS의 기본 발동 조건을 검사한다.
	// [1] タグ・コスト・クールダウンなど、GASの基本発動条件を検証する。
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
	{
		return false;
	}

	// [2] 공격 실행에 필요한 ASC와 Avatar가 유효한지 확인한다.
	// [2] 攻撃の実行に必要なASCとAvatarの有効性を確認する。
	if (!ActorInfo || !ActorInfo->AbilitySystemComponent.IsValid() || !ActorInfo->AvatarActor.IsValid())
	{
		return false;
	}

	// [3] 몽타주·타수 목록·피해 Effect가 설정되어 있는지 확인한다.
	// [3] Montage・攻撃段一覧・ダメージEffectの設定を確認する。
	if (!AttackMontage || AttackSteps.IsEmpty() || !DamageEffectClass)
	{
		return false;
	}

	for (const FTacticalBasicAttackStep& AttackStep : AttackSteps)
	{
		if (AttackStep.MontageSection.IsNone())
		{
			return false;
		}
	}

	return true;
}

// =======================================================
// 평타 실행 초기화
//
// ASC가 전달한 실행 방식을 확인하고 첫 타수를 준비한다.
// InputBound 실행에서만 입력 해제를 기다린다.
// 타격·타수 종료 이벤트는 두 실행 방식에서 공통으로 사용한다.
//
// 通常攻撃の実行初期化。
// ASCから受け取った実行方式を確認し、初段を準備する。
// InputBoundでの実行時のみ入力解放を待機する。
// ヒット・各段終了イベントは両方の実行方式で共通して使用する。
// =======================================================
void UTacticalBasicAttackAbility::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	// [1] 전달받은 실행 방식을 확인한다.
	// 누락되거나 지원하지 않는 방식이면 코스트·쿨다운을 확정하기 전에 중단한다.
	// [1] 受け取った実行方式を確認する。
	// 未指定または未対応の方式なら、コスト・クールダウンの確定前に中断する。
	if (!InitializeExecutionMode(TriggerEventData))
	{
		FinishAbility(true);
		return;
	}

	// [2] 코스트·쿨다운을 확정한다. 실패하면 발동을 취소한다.
	// [2] コスト・クールダウンを確定する。失敗した場合は発動を取り消す。
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		FinishAbility(true);
		return;
	}

	// [3] TargetSelector가 있으면 캐싱한다. 없어도 공격은 진행한다.
	// [3] TargetSelectorが存在する場合はキャッシュする。存在しなくても攻撃は継続する。
	AActor* AvatarActor = ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr;
	TargetSelector = AvatarActor ? AvatarActor->FindComponentByClass<UTargetSelectorComponent>() : nullptr;

	// [4] 첫 타수의 실행 상태를 초기화한다.
	// 실행 방식은 이미 결정되어 있으며, Spec에서는 현재 입력 상태만 읽는다.
	// [4] 初段の実行状態を初期化する。
	// 実行方式はすでに決定済みであり、Specからは現在の入力状態のみを読み取る。
	CurrentStepIndex = 0;
	bAttackInputHeld = false;
	bDamageAppliedForCurrentStep = false;
	CurrentStepTarget.Reset();

	if (ExecutionMode == EExecutionMode::InputBound)
	{
		const FGameplayAbilitySpec* Spec = GetCurrentAbilitySpec();
		bAttackInputHeld = Spec && Spec->InputPressed;
	}

	// [5] InputBound 실행에서 입력 해제를 기다린다.
	// [5] InputBoundでの実行時に入力解放を待機する。
	if (ExecutionMode == EExecutionMode::InputBound)
	{
		StartInputReleaseTask();
	}

	// [6] 애니메이션 Gameplay Event 수신을 준비한다.
	// [6] Animation Gameplay Eventの受信を準備する。
	HitEventTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
		this,
		TacticalGameplayTags::Event_Attack_Basic_Hit,
		nullptr,
		false,
		true);

	if (HitEventTask)
	{
		HitEventTask->EventReceived.AddDynamic(this, &UTacticalBasicAttackAbility::HandleHitEvent);
		HitEventTask->ReadyForActivation();
	}

	StepEndEventTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
		this,
		TacticalGameplayTags::Event_Attack_Basic_SectionEnd,
		nullptr,
		false,
		true);

	if (StepEndEventTask)
	{
		StepEndEventTask->EventReceived.AddDynamic(this, &UTacticalBasicAttackAbility::HandleStepEndEvent);
		StepEndEventTask->ReadyForActivation();
	}

	// [7] 첫 타수의 몽타주 재생을 시작한다.
	// [7] 初段のMontage再生を開始する。
	PlayCurrentAttackStep();
}

void UTacticalBasicAttackAbility::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled)
{
	MontageTask = nullptr;
	HitEventTask = nullptr;
	StepEndEventTask = nullptr;
	InputReleaseTask = nullptr;

	TargetSelector.Reset();
	CurrentStepTarget.Reset();

	CurrentStepIndex = INDEX_NONE;
	ExecutionMode = EExecutionMode::AutoComplete;
	bAttackInputHeld = false;
	bDamageAppliedForCurrentStep = false;

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

// =======================================================
// 실행 중 추가 입력 누름
//
// InputBound 실행에서 입력 유지 상태를 다시 기록한다.
// 입력 해제 후 현재 타격이 끝나기 전에 다시 누르면 공격을 이어갈 수 있다.
// AutoComplete 실행에는 입력 누름을 반영하지 않는다.
//
// 発動中の追加入力押下。
// InputBoundでの実行時に入力維持状態を再度記録する。
// 入力解放後、現在段が終了する前に再度押すと攻撃を継続できる。
// AutoCompleteでの実行には入力押下を反映しない。
// =======================================================
void UTacticalBasicAttackAbility::InputPressed(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo)
{
	Super::InputPressed(Handle, ActorInfo, ActivationInfo);

	if (!IsActive() || ExecutionMode != EExecutionMode::InputBound)
	{
		return;
	}

	bAttackInputHeld = true;
	StartInputReleaseTask();
}

AActor* UTacticalBasicAttackAbility::ResolveAttackTarget_Implementation() const
{
	return TargetSelector.IsValid() ? TargetSelector->GetCurrentTarget() : nullptr;
}

// =======================================================
// 실행 방식 설정
//
// 발동 시 전달받은 EventTag를 평타 내부 실행 방식으로 변환한다.
// 입력 상태나 Controller 종류로 실행 방식을 추측하지 않는다.
//
// 実行方式の設定。
// 発動時に受け取ったEventTagを通常攻撃内部の実行方式へ変換する。
// 入力状態やControllerの種類から実行方式を推測しない。
// =======================================================
bool UTacticalBasicAttackAbility::InitializeExecutionMode(const FGameplayEventData* TriggerEventData)
{
	if (!TriggerEventData)
	{
		UE_LOG(LogTacticalAI, Warning, TEXT("通常攻撃: 実行方式を含む発動データがありません。"));
		return false;
	}

	const FGameplayTag& ExecutionModeTag = TriggerEventData->EventTag;

	if (ExecutionModeTag == TacticalGameplayTags::Event_Ability_Activate_InputBound)
	{
		ExecutionMode = EExecutionMode::InputBound;
		return true;
	}

	if (ExecutionModeTag == TacticalGameplayTags::Event_Ability_Activate_AutoComplete)
	{
		ExecutionMode = EExecutionMode::AutoComplete;
		return true;
	}

	UE_LOG(LogTacticalAI, Warning, TEXT("通常攻撃: 未対応の実行方式です。Mode=%s"),
		*ExecutionModeTag.ToString());
	return false;
}

// =======================================================
// 입력 해제 대기
//
// InputBound 실행에서 입력 해제를 기다리는 Task를 생성한다.
// 이미 대기 중이면 기존 Task를 유지한다.
// 해제 이벤트를 받은 Task는 종료되므로, 재입력 시 새로 생성한다.
//
// 入力解放の待機。
// InputBoundでの実行時に入力解放を待機するTaskを生成する。
// すでに待機中の場合は既存のTaskを維持する。
// 解放イベントを受けたTaskは終了するため、再入力時に新しく生成する。
// =======================================================
void UTacticalBasicAttackAbility::StartInputReleaseTask()
{
	if (!IsActive() || ExecutionMode != EExecutionMode::InputBound || InputReleaseTask)
	{
		return;
	}

	InputReleaseTask = UAbilityTask_WaitInputRelease::WaitInputRelease(this, true);
	if (InputReleaseTask)
	{
		InputReleaseTask->OnRelease.AddDynamic(this, &UTacticalBasicAttackAbility::HandleInputReleased);
		InputReleaseTask->ReadyForActivation();
	}
}

// =======================================================
// 현재 타수 준비
//
// 타수 인덱스를 검증하고 대상과 피해 적용 상태를 갱신한다.
// 대상이 없어도 현재 타수의 애니메이션은 재생한다.
//
// 現在段の準備。
// 段インデックスを検証し、対象とダメージ適用状態を更新する。
// 対象が存在しない場合も現在段のアニメーションは再生する。
// =======================================================
bool UTacticalBasicAttackAbility::PrepareCurrentAttackStep()
{
	// [1] 재생할 타수 데이터가 유효한지 확인한다.
	// [1] 再生する攻撃段のデータが有効か確認する。
	if (!AttackSteps.IsValidIndex(CurrentStepIndex))
	{
		UE_LOG(LogTacticalAI, Warning, TEXT("通常攻撃: 無効なAttackStepです。Index=%d"), CurrentStepIndex);
		return false;
	}

	// [2] 이번 타수의 대상을 조회하고 중복 피해 방지 상태를 초기화한다.
	// 대상이 없는 경우도 정상적인 공격 실행으로 처리한다.
	// [2] 現在段の対象を取得し、重複ダメージ防止状態を初期化する。
	// 対象が存在しない場合も通常の攻撃実行として扱う。
	CurrentStepTarget = ResolveAttackTarget();
	bDamageAppliedForCurrentStep = false;

	return true;
}

void UTacticalBasicAttackAbility::PlayCurrentAttackStep()
{
	if (!PrepareCurrentAttackStep())
	{
		FinishAbility(false);
		return;
	}

	const FTacticalBasicAttackStep& CurrentStep = AttackSteps[CurrentStepIndex];

	if (!MontageTask)
	{
		MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
			this,
			NAME_None,
			AttackMontage,
			1.0f,
			CurrentStep.MontageSection,
			true);

		if (!MontageTask)
		{
			UE_LOG(LogTacticalAI, Warning, TEXT("通常攻撃: MontageTaskの作成に失敗しました。"));
			FinishAbility(true);
			return;
		}

		MontageTask->OnCompleted.AddDynamic(this, &UTacticalBasicAttackAbility::HandleMontageCompleted);
		MontageTask->OnInterrupted.AddDynamic(this, &UTacticalBasicAttackAbility::HandleMontageInterrupted);
		MontageTask->OnCancelled.AddDynamic(this, &UTacticalBasicAttackAbility::HandleMontageCancelled);
		MontageTask->ReadyForActivation();

		return;
	}

	UAbilitySystemComponent* SourceASC = GetAbilitySystemComponentFromActorInfo();
	if (!SourceASC)
	{
		UE_LOG(LogTacticalAI, Warning, TEXT("通常攻撃: Source ASCがありません。"));
		FinishAbility(true);
		return;
	}

	SourceASC->CurrentMontageJumpToSection(CurrentStep.MontageSection);
}

void UTacticalBasicAttackAbility::ApplyCurrentStepDamage()
{
	if (bDamageAppliedForCurrentStep)
	{
		return;
	}

	UAbilitySystemComponent* SourceASC = GetAbilitySystemComponentFromActorInfo();
	UAbilitySystemComponent* TargetASC = CurrentStepTarget.IsValid()
		? UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(CurrentStepTarget.Get())
		: nullptr;

	if (!SourceASC || !TargetASC)
	{
		UE_LOG(LogTacticalAI, Verbose, TEXT("通常攻撃: SourceまたはTargetのASCがありません。"));
		return;
	}

	if (!AttackSteps.IsValidIndex(CurrentStepIndex))
	{
		return;
	}

	const float BasicAttackPower = SourceASC->GetNumericAttribute(
		UTacticalCombatAttributeSet::GetBasicAttackPowerAttribute());

	const float DamageAmount = BasicAttackPower * AttackSteps[CurrentStepIndex].DamageMultiplier;
	if (DamageAmount <= 0.f)
	{
		return;
	}

	FGameplayEffectSpecHandle DamageSpecHandle =
		MakeOutgoingGameplayEffectSpec(DamageEffectClass, GetAbilityLevel());

	if (!DamageSpecHandle.IsValid() || !DamageSpecHandle.Data.IsValid())
	{
		UE_LOG(LogTacticalAI, Warning, TEXT("通常攻撃: DamageSpecの作成に失敗しました。"));
		return;
	}

	DamageSpecHandle.Data->SetSetByCallerMagnitude(
		TacticalGameplayTags::Data_Damage,
		DamageAmount);

	SourceASC->ApplyGameplayEffectSpecToTarget(*DamageSpecHandle.Data.Get(), TargetASC);
	bDamageAppliedForCurrentStep = true;
}

void UTacticalBasicAttackAbility::FinishAbility(bool bWasCancelled)
{
	if (!IsActive()) return;

	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, bWasCancelled);
}

void UTacticalBasicAttackAbility::HandleHitEvent(FGameplayEventData Payload)
{
	ApplyCurrentStepDamage();
}

// =======================================================
// 현재 타수 종료
//
// InputBound는 입력 유지 여부로 공격 지속을 결정한다.
// AutoComplete는 마지막 타수를 마쳤는지로 종료를 결정한다.
//
// 現在段の終了。
// InputBoundは入力維持状態によって攻撃の継続を判断する。
// AutoCompleteは最終段を完了したかによって終了を判断する。
// =======================================================
void UTacticalBasicAttackAbility::HandleStepEndEvent(FGameplayEventData Payload)
{
	if (!IsActive()) return;

	if (!AttackSteps.IsValidIndex(CurrentStepIndex))
	{
		FinishAbility(true);
		return;
	}

	switch (ExecutionMode)
	{
	case EExecutionMode::InputBound:
		// 입력을 해제했다면 현재 타격을 마친 이 시점에 종료한다.
		// 入力が解放されていれば、現在段を完了したこの時点で終了する。
		if (!bAttackInputHeld)
		{
			FinishAbility(false);
			return;
		}
		break;

	case EExecutionMode::AutoComplete:
		// 마지막 타격을 마쳤다면 한 사이클을 완료한 것이므로 종료한다.
		// 最終段を完了していれば、1サイクルが完了したため終了する。
		if (CurrentStepIndex == AttackSteps.Num() - 1)
		{
			FinishAbility(false);
			return;
		}
		break;
	}

	// 다음 타수를 재생한다.
	// 마지막 타수에서 첫 타수로 돌아가는 것은 입력을 유지 중인 InputBound 실행뿐이다.
	// 次段を再生する。
	// 最終段から初段へ戻るのは、入力維持中のInputBound実行のみ。
	CurrentStepIndex = (CurrentStepIndex + 1) % AttackSteps.Num();
	PlayCurrentAttackStep();
}

void UTacticalBasicAttackAbility::HandleInputReleased(float TimeHeld)
{
	// 해제를 받은 Task는 스스로 종료한다.
	// 참조를 비워 이후 재입력 시 새 Task를 생성할 수 있도록 한다.
	// 解放を受けたTaskは自身で終了する。
	// 参照をクリアし、再入力時に新しいTaskを生成できるようにする。
	InputReleaseTask = nullptr;
	bAttackInputHeld = false;
}

void UTacticalBasicAttackAbility::HandleMontageCompleted()
{
	FinishAbility(false);
}

void UTacticalBasicAttackAbility::HandleMontageInterrupted()
{
	FinishAbility(true);
}

void UTacticalBasicAttackAbility::HandleMontageCancelled()
{
	FinishAbility(true);
}