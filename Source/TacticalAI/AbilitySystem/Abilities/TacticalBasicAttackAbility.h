#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "TacticalBasicAttackAbility.generated.h"

class AActor;
class UAnimMontage;
class UGameplayEffect;
class UTargetSelectorComponent;
class UAbilityTask_PlayMontageAndWait;
class UAbilityTask_WaitGameplayEvent;
class UAbilityTask_WaitInputRelease;

// =======================================================
// 평타 한 타의 디자이너 설정.
//
// 하나의 공격 몽타주 안에서 재생할 섹션과
// 해당 타수의 피해 배율을 연결한다.
//
// 通常攻撃一段分のデザイナー設定。
// 一つの攻撃Montage内で再生するSectionと
// その段のダメージ倍率を関連付ける。
// =======================================================
USTRUCT(BlueprintType)
struct FTacticalBasicAttackStep
{
	GENERATED_BODY()

	// 이 타수에서 재생할 공격 몽타주 섹션.
	// この段で再生する攻撃Montage Section。
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Basic Attack")
	FName MontageSection = NAME_None;

	// BasicAttackPower에 적용할 타수별 피해 배율.
	// BasicAttackPowerへ適用する段別ダメージ倍率。
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Basic Attack", meta = (ClampMin = "0.0"))
	float DamageMultiplier = 1.f;
};

// =======================================================
// 연속 평타 Gameplay Ability
//
// 한 번 활성화된 뒤 입력이 유지되는 동안 AttackSteps를 순환한다.
// 타격과 타수 종료 시점은 애니메이션 Gameplay Event로 전달받는다.
//
// 連続通常攻撃Gameplay Ability。
// 一度発動した後、入力が維持されている間AttackStepsを循環する。
// ヒットと各段の終了タイミングはAnimation Gameplay Eventから受け取る。
// =======================================================
UCLASS(Abstract, Blueprintable)
class TACTICALAI_API UTacticalBasicAttackAbility : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UTacticalBasicAttackAbility();

	virtual bool CanActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayTagContainer* SourceTags = nullptr,
		const FGameplayTagContainer* TargetTags = nullptr,
		FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;

protected:
	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	virtual void EndAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility,
		bool bWasCancelled) override;

	// 현재 타수에서 공격할 대상 조회.
	// 기본 구현은 캐싱된 TargetSelector의 현재 타겟을 사용한다.
	// 現在段で攻撃する対象を取得する。
	// 基本実装はキャッシュ済みTargetSelectorの現在ターゲットを使う。
	UFUNCTION(BlueprintNativeEvent, Category = "Basic Attack")
	AActor* ResolveAttackTarget() const;
	virtual AActor* ResolveAttackTarget_Implementation() const;

private:
	bool PrepareCurrentAttackStep();
	void PlayCurrentAttackStep();
	void ApplyCurrentStepDamage();
	void FinishAbility(bool bWasCancelled);

	UFUNCTION()
	void HandleHitEvent(FGameplayEventData Payload);

	UFUNCTION()
	void HandleStepEndEvent(FGameplayEventData Payload);

	UFUNCTION()
	void HandleInputReleased(float TimeHeld);

	UFUNCTION()
	void HandleMontageCompleted();

	UFUNCTION()
	void HandleMontageInterrupted();

	UFUNCTION()
	void HandleMontageCancelled();

private:
	// 모든 타수가 공유하는 공격 몽타주.
	// 각 타수는 AttackSteps의 Section 이름으로 구분한다.
	// 全段で共有する攻撃Montage。
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Basic Attack|Animation", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UAnimMontage> AttackMontage;

	// 재생 순서대로 정의하는 평타 타수 목록.
	// 마지막 타수 다음에는 배열의 첫 타수로 돌아간다.
	// 再生順に定義する通常攻撃段の一覧。
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Basic Attack|Animation", meta = (AllowPrivateAccess = "true"))
	TArray<FTacticalBasicAttackStep> AttackSteps;

	// 계산된 최종 피해량을 대상에게 전달할 Gameplay Effect.
	// SetByCaller(Data.Damage)를 IncomingDamage로 전달하는 BP GE를 지정한다.
	// 計算済みダメージを対象へ渡すGameplay Effect。
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Basic Attack|Damage", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<UGameplayEffect> DamageEffectClass;

	// 현재 타수에서 사용할 타겟. 다음 타수 시작 시 다시 확정한다.
	// 現在段で使用するターゲット。次段開始時に再取得する。
	TWeakObjectPtr<AActor> CurrentStepTarget;

	// Avatar가 소유한 TargetSelector 캐시. Ability가 소유하지 않는다.
	// Avatar所有のTargetSelectorキャッシュ。Abilityは所有しない。
	TWeakObjectPtr<UTargetSelectorComponent> TargetSelector;

	int32 CurrentStepIndex = INDEX_NONE;

	bool bAttackInputHeld = false;
	bool bDamageAppliedForCurrentStep = false;

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_PlayMontageAndWait> MontageTask;

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_WaitGameplayEvent> HitEventTask;

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_WaitGameplayEvent> StepEndEventTask;

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_WaitInputRelease> InputReleaseTask;
};