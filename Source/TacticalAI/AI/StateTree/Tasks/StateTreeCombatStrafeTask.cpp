#include "AI/StateTree/Tasks/StateTreeCombatStrafeTask.h"

#include "AIController.h"
#include "GameFramework/Pawn.h"
#include "StateTreeExecutionContext.h"
#include "AI/Components/CombatMicroMovementComponent.h"

FStateTreeCombatStrafeTask::FStateTreeCombatStrafeTask()
{
	// 실행 중에는 저장된 컴포넌트의 상태만 확인한다.
	// 実行中は保持したコンポーネントの状態のみ確認する。
	bShouldCallTick = true;

	// 진입 시 전달받은 Controller를 이번 실행이 끝날 때까지 사용한다.
	// 開始時に受け取ったControllerを今回の実行終了まで使用する。
	bShouldCopyBoundPropertiesOnTick = false;
	bShouldCopyBoundPropertiesOnExitState = false;

	// 같은 이동 상태를 다시 선택하면 이전 요청을 정리하고 새로 시작한다.
	// 同じ移動状態を再選択した場合は、以前の要求を整理して再開始する。
	bShouldStateChangeOnReselect = true;
}

// =======================================================
// 상태 진입: 횡이동 1회 요청
// 시작에 성공한 경우에만 종료 시 취소할 책임을 가진다.
//
// 状態開始：横移動を一回要求する。
// 開始に成功した場合のみ、終了時のキャンセルを担当する。
// =======================================================
EStateTreeRunStatus FStateTreeCombatStrafeTask::EnterState(
	FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	// [1] 이전 실행 데이터를 초기화하고 현재 Pawn을 확인한다.
	// [1] 以前の実行データを初期化し、現在のPawnを確認する。
	InstanceData.MicroMovement.Reset();
	InstanceData.bStartedStrafe = false;

	AAIController* AIController = InstanceData.AIController.Get();
	if (!IsValid(AIController)) return EStateTreeRunStatus::Failed;

	APawn* ControlledPawn = AIController->GetPawn();
	if (!IsValid(ControlledPawn)) return EStateTreeRunStatus::Failed;

	// [2] 진입 시 한 번만 컴포넌트를 조회하고 이동을 요청한다.
	// [2] 開始時に一度だけコンポーネントを取得し、移動を要求する。
	UCombatMicroMovementComponent* MicroMovement =
		ControlledPawn->FindComponentByClass<UCombatMicroMovementComponent>();

	if (!IsValid(MicroMovement)) return EStateTreeRunStatus::Failed;
	if (!MicroMovement->StartStrafe()) return EStateTreeRunStatus::Failed;

	// [3] 시작에 성공한 요청의 참조와 실행 여부를 보관한다.
	// [3] 開始に成功した要求の参照と実行状態を保持する。
	InstanceData.MicroMovement = MicroMovement;
	InstanceData.bStartedStrafe = true;

	return EStateTreeRunStatus::Running;
}

// =======================================================
// 실행 결과 확인
// 이동 계산은 컴포넌트가 수행하고, Task는 실행 결과만 해석한다.
//
// 実行結果の確認。
// 移動計算はコンポーネントが行い、Taskは実行結果のみを判定する。
// =======================================================
EStateTreeRunStatus FStateTreeCombatStrafeTask::Tick(
	FStateTreeExecutionContext& Context,
	const float DeltaTime) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	// [1] 요청 대상이 유효하며 같은 Controller가 계속 제어하는지 확인한다.
	// [1] 要求対象が有効で、同じControllerが引き続き制御しているか確認する。
	UCombatMicroMovementComponent* MicroMovement =
		InstanceData.MicroMovement.Get();
	AAIController* AIController = InstanceData.AIController.Get();

	if (!InstanceData.bStartedStrafe ||
		!MicroMovement || !IsValid(AIController))
	{
		return EStateTreeRunStatus::Failed;
	}

	if (AIController->GetPawn() != MicroMovement->GetOwner())
	{
		return EStateTreeRunStatus::Failed;
	}

	// [2] 개별 이동 결과를 StateTree의 실행 결과로 변환한다.
	// [2] 個別移動の結果をStateTreeの実行結果へ変換する。
	switch (MicroMovement->GetStrafeState())
	{
	case ECombatStrafeState::Running:
		return EStateTreeRunStatus::Running;

	case ECombatStrafeState::Completed:
		InstanceData.bStartedStrafe = false;
		return EStateTreeRunStatus::Succeeded;

	case ECombatStrafeState::Interrupted:
	case ECombatStrafeState::Idle:
	default:
		InstanceData.bStartedStrafe = false;
		return EStateTreeRunStatus::Failed;
	}
}

// =======================================================
// 상태 이탈: 남은 이동 취소
// 시작하지 못한 이동은 취소하지 않는다.
// 현재 Controller의 Pawn을 다시 검색하지 않고 원래 요청 대상을 정리한다.
//
// 状態終了：残っている移動をキャンセルする。
// 開始できなかった移動はキャンセルしない。
// 現在のPawnを再検索せず、元の要求対象を整理する。
// =======================================================
void FStateTreeCombatStrafeTask::ExitState(
	FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	// [1] 이번 Task가 시작한 이동이 남아 있으면 취소한다.
	// [1] 今回のTaskが開始した移動が残っていればキャンセルする。
	if (InstanceData.bStartedStrafe)
	{
		if (UCombatMicroMovementComponent* MicroMovement =
			InstanceData.MicroMovement.Get())
		{
			MicroMovement->CancelStrafe();
		}
	}

	// [2] 다음 실행에 참조와 시작 여부가 남지 않도록 초기화한다.
	// [2] 次の実行に参照と開始状態を残さないよう初期化する。
	InstanceData.MicroMovement.Reset();
	InstanceData.bStartedStrafe = false;
}