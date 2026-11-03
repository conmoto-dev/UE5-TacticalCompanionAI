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

	// =======================================================
	// 현재 타수의 공격 대상 결정.
	// 플레이어는 입력 방향 반영.
	// AI는 캐싱된 TargetSelector의 현재 대상을 사용한다.
	//
	// 現在段の攻撃対象を決定する。
	// プレイヤーは入力方向を反映する。
	// AIはキャッシュ済みTargetSelectorの現在ターゲットを使用する。
	// =======================================================
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

	// =======================================================
	// 타수별 입력 조회·대상 유지·방향 정렬
	// 입력 정보는 인터페이스로 조회하고, 평타 정책은 Ability에서 판단한다.
	//
	// 段ごとの入力取得・対象維持・方向整列。
	// 入力情報はインターフェースから取得し、通常攻撃の規則はAbilityで判断する。
	// =======================================================
	FVector GetDesiredInputDirection2D() const;

	bool CanKeepCurrentStepTarget(
		const AActor* AvatarActor,
		const AActor* TargetActor) const;

	void UpdateCurrentStepFacing();
	
	// 대상까지의 거리에 따른 평타 루트모션 이동 배율.
	// 対象までの距離に応じた通常攻撃のルートモーション移動倍率。
	float CalculateCurrentStepRootMotionScale(const FName& SectionName) const;

	// 이동 배율을 기본값으로 복원한다.
	// 移動倍率を既定値へ戻す。
	void ResetRootMotionScale();
	
	bool PrepareCurrentAttackStep();
	void PlayCurrentAttackStep();
	// 타격 시점의 명중 가능 여부
	// ヒット時点の命中可否。
	bool CanHitCurrentStepTarget() const;
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

	// =======================================================
	// 플레이어 평타 대상 검색·방향 설정
	// 루트모션 이동량과 실제 타격 범위는 별도로 결정한다.
	//
	// プレイヤー通常攻撃の対象検索・方向設定。
	// ルートモーション移動量と実際の命中範囲は別に決定する。
	// =======================================================

	// 플레이어 평타의 대상 검색 거리. Actor 위치 사이의 수평 중심 거리 기준.
	// プレイヤー通常攻撃の対象検索距離。Actor位置間の水平中心距離を使う。
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Basic Attack|Targeting",
		meta = (AllowPrivateAccess = "true", ClampMin = "0.0", Units = "cm"))
	float TargetSearchDistance = 300.f;

	// 검색 기준 방향 좌우의 허용 반각. 
	// 検索基準方向の左右許容半角。60なら全体の検索角度は120度。
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Basic Attack|Targeting",
		meta = (AllowPrivateAccess = "true", ClampMin = "0.0", ClampMax = "180.0"))
	float TargetSearchHalfAngleDeg = 90.f;

	// 평타의 타수 시작 시 대상 방향으로 회전한다.
	// 대상이 없으면 이동 입력 방향을 사용하며, 입력도 없으면 현재 방향을 유지한다.
	// プレイヤー通常攻撃の各段開始時に対象方向へ回転する。
	// 対象がなければ移動入力方向を使い、入力もなければ現在方向を維持する。
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Basic Attack|Targeting",
		meta = (AllowPrivateAccess = "true"))
	bool bFaceTargetOnStepStart = true;
	
	// =======================================================
	// 공격 이동 보정 설정
	// 対象への攻撃移動の補正設定
	// =======================================================

	// 공격자와 대상의 캡슐 표면 사이에 남길 거리.
	// 攻撃者と対象のカプセル表面間に残す距離。
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Basic Attack|Movement",
		meta = (AllowPrivateAccess = "true", ClampMin = "0.0", Units = "cm"))
	float AttackStopDistanceMargin = 10.f;
	
	// =======================================================
	// 타격 판정 설정
	// 플레이어와 AI가 공통으로 사용한다.
	//
	// 命中判定設定。
	// プレイヤーとAIで共通使用する。
	// =======================================================

	// 명중을 허용하는 수평 거리. 양쪽 캡슐 반경을 제외한 표면 간격 기준.
	// 命中を許可する水平距離。両者のカプセル半径を除いた表面間距離を使う。
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Basic Attack|Hit",
		meta = (AllowPrivateAccess = "true", ClampMin = "0.0", Units = "cm"))
	float HitRange = 100.f;

	// 타격 순간 캐릭터 정면을 기준으로 하는 명중 허용 반각.
	// ヒット時点のキャラクター前方を基準とする命中許容半角。
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Basic Attack|Hit",
		meta = (AllowPrivateAccess = "true", ClampMin = "0.0", ClampMax = "180.0"))
	float HitHalfAngleDeg = 90.f;

	// 명중을 허용하는 Actor 중심 간 높이 차이.
	// 수평 위치만 가까운 다른 높이의 대상을 타격하지 않도록 제한한다.
	// 命中を許可するActor中心間の高低差。
	// 水平位置だけが近い、異なる高さの対象への命中を制限する。
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Basic Attack|Hit",
		meta = (AllowPrivateAccess = "true", ClampMin = "0.0", Units = "cm"))
	float MaxHitHeightDifference = 120.f;
	
	// 현재 타수의 대상. 다음 타수 선정 시에는 이전 대상으로 참조한다.
	// 공격 종료 시 초기화.
	// 現在段の対象。次段の選定時には前段の対象として参照する。
	// 攻撃終了時に初期化。
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