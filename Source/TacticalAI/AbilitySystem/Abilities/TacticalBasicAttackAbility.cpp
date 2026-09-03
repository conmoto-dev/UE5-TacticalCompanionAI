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

bool UTacticalBasicAttackAbility::CanActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayTagContainer* SourceTags,
	const FGameplayTagContainer* TargetTags,
	FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
	{
		return false;
	}

	if (!ActorInfo || !ActorInfo->AbilitySystemComponent.IsValid() || !ActorInfo->AvatarActor.IsValid())
	{
		return false;
	}

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

	const AActor* AvatarActor = ActorInfo->AvatarActor.Get();
	const UTargetSelectorComponent* FoundTargetSelector = AvatarActor->FindComponentByClass<UTargetSelectorComponent>();

	return FoundTargetSelector && IsValid(FoundTargetSelector->GetCurrentTarget());
}

void UTacticalBasicAttackAbility::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		FinishAbility(true);
		return;
	}

	AActor* AvatarActor = ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr;
	TargetSelector = AvatarActor ? AvatarActor->FindComponentByClass<UTargetSelectorComponent>() : nullptr;

	if (!TargetSelector.IsValid())
	{
		UE_LOG(LogTacticalAI, Warning, TEXT("通常攻撃: TargetSelectorが見つかりません。"));
		FinishAbility(true);
		return;
	}

	CurrentStepIndex = 0;
	bAttackInputHeld = true;
	bDamageAppliedForCurrentStep = false;
	CurrentStepTarget.Reset();

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

bool UTacticalBasicAttackAbility::PrepareCurrentAttackStep()
{
	if (!AttackSteps.IsValidIndex(CurrentStepIndex))
	{
		UE_LOG(LogTacticalAI, Warning, TEXT("通常攻撃: 無効なAttackStepです。Index=%d"), CurrentStepIndex);
		return false;
	}

	CurrentStepTarget = ResolveAttackTarget();
	bDamageAppliedForCurrentStep = false;

	if (!CurrentStepTarget.IsValid())
	{
		UE_LOG(LogTacticalAI, Verbose, TEXT("通常攻撃: 有効なターゲットがありません。"));
		return false;
	}

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