#include "AbilitySystem/Abilities/TacticalBasicAttackAbility.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystem/Attributes/TacticalCombatAttributeSet.h"
#include "AbilitySystem/Tags/TacticalGameplayTags.h"
#include "AI/Targeting/TargetSelectorComponent.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "Abilities/Tasks/AbilityTask_WaitInputRelease.h"
#include "GameplayEffect.h"
#include "TacticalAI.h"

UTacticalBasicAttackAbility::UTacticalBasicAttackAbility()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

	AbilityTags.AddTag(TacticalGameplayTags::Ability_Attack_Basic);

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
// 선택적으로 TargetSelector를 캐싱하고,
// 입력 해제·타격·타수 종료 이벤트를 대기한 뒤 첫 타수를 재생한다.
//
// 通常攻撃の実行初期化。
// TargetSelectorが存在する場合はキャッシュし、
// 入力解放・ヒット・各段終了イベントの待機後に初段を再生する。
// =======================================================
void UTacticalBasicAttackAbility::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	// [1] コスト・クールダウンを確定する。失敗した場合は発動を取り消す。
	// [1] 코스트·쿨다운을 확정한다. 실패하면 발동을 취소한다.
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		FinishAbility(true);
		return;
	}

	// [2] TargetSelector가 있으면 캐싱한다. 없어도 공격은 진행한다.
	// [2] TargetSelectorが存在する場合はキャッシュする。存在しなくても攻撃は継続する。
	AActor* AvatarActor = ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr;
	TargetSelector = AvatarActor ? AvatarActor->FindComponentByClass<UTargetSelectorComponent>() : nullptr;

	// [3] 첫 타수의 실행 상태를 초기화한다.
	// [3] 初段の実行状態を初期化する。
	CurrentStepIndex = 0;
	bAttackInputHeld = true;
	bDamageAppliedForCurrentStep = false;
	CurrentStepTarget.Reset();

	// [4] 입력 해제와 애니메이션 Gameplay Event 수신을 준비한다.
	// [4] 入力解放とAnimation Gameplay Eventの受信を準備する。
	InputReleaseTask = UAbilityTask_WaitInputRelease::WaitInputRelease(this, true);
	if (InputReleaseTask)
	{
		InputReleaseTask->OnRelease.AddDynamic(this, &UTacticalBasicAttackAbility::HandleInputReleased);
		InputReleaseTask->ReadyForActivation();
	}

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

	// [5] 첫 타수의 몽타주 재생을 시작한다.
	// [5] 初段のMontage再生を開始する。
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
	bAttackInputHeld = false;
	bDamageAppliedForCurrentStep = false;

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

AActor* UTacticalBasicAttackAbility::ResolveAttackTarget_Implementation() const
{
	return TargetSelector.IsValid() ? TargetSelector->GetCurrentTarget() : nullptr;
}

// =======================================================
// 현재 타수 준비
//
// 타수 인덱스를 검증하고 대상과 피해 적용 상태를 갱신한다.
// 대상이 없어도 현재 타수의 애니메이션은 재생한다.
//
// 現在段の準備。
// 段インデックスを検証し、対象とダメージ適用状態を更新する。
// 対象が存在しなくても現在段のアニメーションを再生する。
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
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, bWasCancelled);
}

void UTacticalBasicAttackAbility::HandleHitEvent(FGameplayEventData Payload)
{
	ApplyCurrentStepDamage();
}

void UTacticalBasicAttackAbility::HandleStepEndEvent(FGameplayEventData Payload)
{
	if (!bAttackInputHeld)
	{
		FinishAbility(false);
		return;
	}

	if (AttackSteps.IsEmpty())
	{
		FinishAbility(true);
		return;
	}

	CurrentStepIndex = (CurrentStepIndex + 1) % AttackSteps.Num();
	PlayCurrentAttackStep();
}

void UTacticalBasicAttackAbility::HandleInputReleased(float TimeHeld)
{
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