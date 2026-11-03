#include "AI/StateTree/Tasks/StateTreeActivateAbilityTask.h"

#include "AIController.h"
#include "AbilitySystemGlobals.h"
#include "Abilities/GameplayAbility.h"
#include "GameFramework/Pawn.h"
#include "StateTreeExecutionContext.h"
#include "AbilitySystem/TacticalAbilitySystemComponent.h"

namespace
{
	// 종료 결과를 실행 결과로 변환한다. 취소는 사이클 미완료로 본다.
	// 終了結果を実行結果へ変換する。キャンセルはサイクル未完了として扱う。
	EStateTreeRunStatus ToRunStatus(const FActivateAbilityTaskRunResult& Result)
	{
		return Result.bWasCancelled
			? EStateTreeRunStatus::Failed
			: EStateTreeRunStatus::Succeeded;
	}

	// 종료 콜백을 해제한다. 취소 요청 전에 호출해 ExitState 중 재진입을 막는다.
	// 終了コールバックを解除する。キャンセル要求の前に呼び、ExitState中の再入を防ぐ。
	void UnbindAbilityEnded(FActivateAbilityTaskInstanceData& InstanceData)
	{
		if (UTacticalAbilitySystemComponent* AbilitySystem = InstanceData.AbilitySystem.Get())
		{
			AbilitySystem->OnAbilityEnded.Remove(InstanceData.AbilityEndedHandle);
		}
		InstanceData.AbilityEndedHandle.Reset();
	}
}

FStateTreeActivateAbilityTask::FStateTreeActivateAbilityTask()
{
	// 실행 중에는 기록된 종료 결과만 확인한다.
	// 実行中は記録された終了結果のみ確認する。
	bShouldCallTick = true;

	// 진입 시 전달받은 값을 이번 실행이 끝날 때까지 사용한다.
	// 開始時に受け取った値を今回の実行終了まで使用する。
	bShouldCopyBoundPropertiesOnTick = false;
	bShouldCopyBoundPropertiesOnExitState = false;

	// 같은 상태를 다시 선택하면 이전 실행을 정리하고 새로 시작한다.
	// 同じ状態を再選択した場合は、以前の実行を整理して再開始する。
	bShouldStateChangeOnReselect = true;
}

// =======================================================
// 상태 진입: Ability 발동 요청
// 종료 콜백을 먼저 등록한 뒤 발동한다.
// 발동은 동기이므로 요청 중에 종료될 수 있다.
//
// 状態開始：Abilityの発動要求。
// 終了コールバックを先に登録してから発動する。
// 発動は同期のため、要求中に終了する場合がある。
// =======================================================
EStateTreeRunStatus FStateTreeActivateAbilityTask::EnterState(
	FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	// [1] 이전 실행 데이터를 초기화하고 발동 대상을 확인한다.
	// [1] 以前の実行データを初期化し、発動対象を確認する。
	InstanceData.AbilitySystem.Reset();
	InstanceData.AbilityEndedHandle.Reset();
	InstanceData.RunResult.Reset();
	InstanceData.bStartedAbility = false;

	if (!InstanceData.AbilityTag.IsValid()) return EStateTreeRunStatus::Failed;

	AAIController* AIController = InstanceData.AIController.Get();
	if (!IsValid(AIController)) return EStateTreeRunStatus::Failed;

	APawn* ControlledPawn = AIController->GetPawn();
	if (!IsValid(ControlledPawn)) return EStateTreeRunStatus::Failed;

	UTacticalAbilitySystemComponent* AbilitySystem = Cast<UTacticalAbilitySystemComponent>(
		UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(ControlledPawn));
	if (!AbilitySystem) return EStateTreeRunStatus::Failed;

	// [2] 발동 전에 종료 콜백을 등록한다. 콜백은 결과를 기록만 한다.
	// [2] 発動前に終了コールバックを登録する。コールバックは結果の記録のみ行う。
	InstanceData.AbilitySystem = AbilitySystem;
	InstanceData.RunResult = MakeShared<FActivateAbilityTaskRunResult>();

	const TWeakPtr<FActivateAbilityTaskRunResult> WeakResult = InstanceData.RunResult;
	const FGameplayTag AbilityTag = InstanceData.AbilityTag;

	InstanceData.AbilityEndedHandle = AbilitySystem->OnAbilityEnded.AddLambda(
		[WeakResult, AbilityTag](const FAbilityEndedData& EndedData)
		{
			const TSharedPtr<FActivateAbilityTaskRunResult> Result = WeakResult.Pin();
			if (!Result || Result->bEnded) return;
			if (!EndedData.AbilityThatEnded) return;
			if (!EndedData.AbilityThatEnded->GetAssetTags().HasTagExact(AbilityTag)) return;

			Result->bEnded = true;
			Result->bWasCancelled = EndedData.bWasCancelled;
		});

	// [3] 발동을 요청한다. 쿨다운·차단 태그 등으로 발동하지 못하면 실패한다.
	// [3] 発動を要求する。クールダウン・遮断タグなどで発動できなければ失敗する。
	if (!AbilitySystem->RequestActivateAutoCompleteAbility(AbilityTag))
	{
		UnbindAbilityEnded(InstanceData);
		InstanceData.RunResult.Reset();
		return EStateTreeRunStatus::Failed;
	}

	// [4] 요청 중에 이미 종료되었으면 그 결과를 바로 반환한다.
	// [4] 要求中にすでに終了していれば、その結果を直ちに返す。
	if (InstanceData.RunResult->bEnded)
	{
		UnbindAbilityEnded(InstanceData);
		return ToRunStatus(*InstanceData.RunResult);
	}

	InstanceData.bStartedAbility = true;
	return EStateTreeRunStatus::Running;
}

// =======================================================
// 실행 결과 확인
// 종료 판정은 Ability가 하고, Task는 기록된 결과만 해석한다.
//
// 実行結果の確認。
// 終了判定はAbilityが行い、Taskは記録された結果のみ判定する。
// =======================================================
EStateTreeRunStatus FStateTreeActivateAbilityTask::Tick(
	FStateTreeExecutionContext& Context,
	const float DeltaTime) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	// [1] 발동한 ASC와 결과 기록이 유효한지 확인한다.
	// [1] 発動したASCと結果記録が有効か確認する。
	if (!InstanceData.bStartedAbility ||
		!InstanceData.RunResult.IsValid() ||
		!InstanceData.AbilitySystem.IsValid())
	{
		InstanceData.bStartedAbility = false;
		return EStateTreeRunStatus::Failed;
	}

	// [2] 종료가 기록되기 전까지 실행 상태를 유지한다.
	// [2] 終了が記録されるまで実行状態を維持する。
	if (!InstanceData.RunResult->bEnded)
	{
		return EStateTreeRunStatus::Running;
	}

	// [3] 종료를 확인했으므로 이탈 시 취소할 대상이 없다.
	// [3] 終了を確認したため、終了時にキャンセルする対象はない。
	InstanceData.bStartedAbility = false;
	return ToRunStatus(*InstanceData.RunResult);
}

// =======================================================
// 상태 이탈: 남은 Ability 취소
// 상위 전이(전투 종료·빙의 해제 등)로 이탈한 경우에만 취소 대상이 남는다.
// 취소는 종료 콜백을 동기로 호출하므로 콜백을 먼저 해제한다.
//
// 状態終了：残っているAbilityをキャンセルする。
// 上位遷移（戦闘終了・Possess解除など）で終了した場合のみキャンセル対象が残る。
// キャンセルは終了コールバックを同期で呼ぶため、先にコールバックを解除する。
// =======================================================
void FStateTreeActivateAbilityTask::ExitState(
	FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	// [1] 취소로 인한 콜백 재진입을 막기 위해 먼저 해제한다.
	// [1] キャンセルによるコールバックの再入を防ぐため、先に解除する。
	UnbindAbilityEnded(InstanceData);

	// [2] 이 Task가 시작한 Ability가 아직 종료되지 않았으면 취소한다.
	// [2] このTaskが開始したAbilityがまだ終了していなければキャンセルする。
	const bool bAbilityStillRunning = InstanceData.bStartedAbility &&
		InstanceData.RunResult.IsValid() && !InstanceData.RunResult->bEnded;

	if (bAbilityStillRunning)
	{
		if (UTacticalAbilitySystemComponent* AbilitySystem = InstanceData.AbilitySystem.Get())
		{
			const FGameplayTagContainer CancelTags(InstanceData.AbilityTag);
			AbilitySystem->CancelAbilities(&CancelTags);
		}
	}

	// [3] 다음 실행에 참조와 결과가 남지 않도록 초기화한다.
	// [3] 次の実行に参照と結果を残さないよう初期化する。
	InstanceData.AbilitySystem.Reset();
	InstanceData.RunResult.Reset();
	InstanceData.bStartedAbility = false;
}