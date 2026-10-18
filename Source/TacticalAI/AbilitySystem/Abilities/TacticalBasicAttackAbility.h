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
// ASC가 전달한 실행 방식에 따라 AttackSteps를 재생한다.
// InputBound: 입력 유지 중 반복하며, 해제하면 현재 타격을 마무리하고 종료한다.
// AutoComplete: 입력 상태와 관계없이 전체 타수를 한 사이클 실행하고 종료한다.
// 타격과 타수 종료 시점은 애니메이션 Gameplay Event로 전달받는다.
//
// 連続通常攻撃Gameplay Ability。
// ASCから受け取った実行方式に従ってAttackStepsを再生する。
// InputBound: 入力中は繰り返し、解放後は現在段を完了して終了する。
// AutoComplete: 入力状態に関係なく全段を1サイクル実行して終了する。
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

	// InputBound로 실행 중인 평타에 추가 입력 누름을 반영한다.
	// 입력 해제 후 공격이 끝나기 전에 다시 누르면, 입력 해제 대기를 다시 시작한다.
	// InputBoundで発動中の通常攻撃に、追加入力の押下を反映する。
	// 入力解放後、攻撃終了前に再度押された場合は入力解放の待機を再開する。
	virtual void InputPressed(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo) override;

	// 현재 타수에서 공격할 대상 조회.
	// 기본 구현은 캐싱된 TargetSelector의 현재 타겟을 사용한다.
	// 現在段で攻撃する対象を取得する。
	// 基本実装はキャッシュ済みTargetSelectorの現在ターゲットを使う。
	UFUNCTION(BlueprintNativeEvent, Category = "Basic Attack")
	AActor* ResolveAttackTarget() const;
	virtual AActor* ResolveAttackTarget_Implementation() const;

private:
	// 발동 시 전달받은 태그를 해석하여 저장하는 평타의 실행 방식.
	// 현재 입력 유지 상태와는 별도로 관리한다.
	// 発動時に受け取ったタグを解釈して保存する通常攻撃の実行方式。
	// 現在の入力維持状態とは別に管理する。
	enum class EExecutionMode : uint8
	{
		InputBound,
		AutoComplete
	};

	// TriggerEventData의 EventTag를 읽어 이번 발동의 실행 방식을 설정한다.
	// 데이터가 없거나 지원하지 않는 태그이면 false를 반환한다.
	// TriggerEventDataのEventTagを読み、今回の発動の実行方式を設定する。
	// データがない場合、または未対応のタグの場合はfalseを返す。
	bool InitializeExecutionMode(const FGameplayEventData* TriggerEventData);

	// InputBound 실행에서 입력 해제 대기를 시작한다.
	// 이미 대기 중이면 중복 Task를 만들지 않는다.
	// InputBoundでの実行時に入力解放の待機を開始する。
	// すでに待機中の場合はTaskを重複生成しない。
	void StartInputReleaseTask();

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
	// InputBound는 입력 유지 중 마지막 타수 다음에 첫 타수로 돌아간다.
	// AutoComplete는 마지막 타수를 마치면 종료한다.
	// 再生順に定義する通常攻撃段の一覧。
	// InputBoundは入力中、最終段の次に初段へ戻る。
	// AutoCompleteは最終段を完了すると終了する。
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

	// 발동할 때마다 ASC가 전달한 태그로 설정한다.
	// 発動ごとにASCから受け取ったタグで設定する。
	EExecutionMode ExecutionMode = EExecutionMode::AutoComplete;

	// InputBound 실행에서 사용하는 현재 입력 유지 상태.
	// InputBoundでの実行に使用する現在の入力維持状態。
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