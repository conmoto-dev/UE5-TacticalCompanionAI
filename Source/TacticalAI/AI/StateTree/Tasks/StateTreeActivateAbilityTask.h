#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Tasks/StateTreeAITask.h"
#include "StateTreeActivateAbilityTask.generated.h"

class AAIController;
class UTacticalAbilitySystemComponent;

// =======================================================
// Ability 종료 결과
// ASC의 종료 콜백이 기록하고, Task가 자신의 진입점에서 읽는다.
// 콜백은 Task 실행 중 동기로 호출될 수 있으므로 판단은 하지 않는다.
//
// Ability終了結果。
// ASCの終了コールバックが記録し、Taskが自身のエントリポイントで読む。
// コールバックはTask実行中に同期で呼ばれ得るため、判断は行わない。
// =======================================================
struct FActivateAbilityTaskRunResult
{
	bool bEnded = false;
	bool bWasCancelled = false;
};

// =======================================================
// Ability 발동 Task의 실행별 데이터
//
// Ability発動Taskの実行ごとのデータ。
// =======================================================
USTRUCT()
struct TACTICALAI_API FActivateAbilityTaskInstanceData
{
	GENERATED_BODY()

	// Ability를 발동할 Pawn을 제어하는 AIController.
	// Abilityを発動するPawnを制御するAIController。
	UPROPERTY(EditAnywhere, Category = Context)
	TObjectPtr<AAIController> AIController = nullptr;

	// 발동할 Ability의 Asset Tag. AutoComplete로 발동하며 Ability 자신의 완료 조건으로 종료한다.
	// 発動するAbilityのAsset Tag。AutoCompleteで発動し、Ability自身の完了条件で終了する。
	UPROPERTY(EditAnywhere, Category = Parameter, meta = (Categories = "Ability"))
	FGameplayTag AbilityTag;

	// 이번 실행에서 발동을 요청한 ASC.
	// 今回の実行で発動を要求したASC。
	UPROPERTY(Transient)
	TWeakObjectPtr<UTacticalAbilitySystemComponent> AbilitySystem;

	// 종료 콜백 해제용 핸들.
	// 終了コールバック解除用のハンドル。
	FDelegateHandle AbilityEndedHandle;

	// 종료 콜백과 공유하는 결과. 콜백은 약참조로만 접근한다.
	// 終了コールバックと共有する結果。コールバックは弱参照のみでアクセスする。
	TSharedPtr<FActivateAbilityTaskRunResult> RunResult;

	// 이번 Task가 발동을 시작했고 아직 종료를 확인하지 않았는지 여부.
	// 今回のTaskが発動を開始し、まだ終了を確認していないか。
	UPROPERTY(Transient)
	bool bStartedAbility = false;
};

// =======================================================
// Ability 발동 (AutoComplete)
// 지정한 Ability를 발동하고 종료될 때까지 실행 상태를 유지한다.
// 완료는 Succeeded, 취소는 Failed. 상태 이탈 시 남은 Ability를 취소한다.
// 발동 여부 판단은 진입 조건이, 실행 내용은 Ability가 담당한다.
//
// Ability発動（AutoComplete）。
// 指定したAbilityを発動し、終了するまで実行状態を維持する。
// 完了はSucceeded、キャンセルはFailed。状態終了時に残っているAbilityをキャンセルする。
// 発動可否の判断は開始条件が、実行内容はAbilityが担当する。
// =======================================================
USTRUCT(meta = (DisplayName = "Activate Ability (Auto Complete)", Category = "Tactical AI|Combat"))
struct TACTICALAI_API FStateTreeActivateAbilityTask : public FStateTreeAIActionTaskBase
{
	GENERATED_BODY()

	using FInstanceDataType = FActivateAbilityTaskInstanceData;

	FStateTreeActivateAbilityTask();

	virtual const UStruct* GetInstanceDataType() const override
	{
		return FInstanceDataType::StaticStruct();
	}

	virtual EStateTreeRunStatus EnterState(
		FStateTreeExecutionContext& Context,
		const FStateTreeTransitionResult& Transition) const override;

	virtual EStateTreeRunStatus Tick(
		FStateTreeExecutionContext& Context,
		const float DeltaTime) const override;

	virtual void ExitState(
		FStateTreeExecutionContext& Context,
		const FStateTreeTransitionResult& Transition) const override;
};