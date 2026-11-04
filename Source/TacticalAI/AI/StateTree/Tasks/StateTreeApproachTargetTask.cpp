#include "AI/StateTree/Tasks/StateTreeApproachTargetTask.h"

#include "AIController.h"
#include "GameFramework/Pawn.h"
#include "Navigation/PathFollowingComponent.h"
#include "StateTreeExecutionContext.h"
#include "AI/Targeting/TargetSelectorComponent.h"

namespace
{
	// 캡슐 표면 사이 수평 거리. Target Range 조건과 같은 정의를 사용한다.
	// カプセル表面間の水平距離。Target Range条件と同じ定義を使用する。
	double CalculateSurfaceDistance2D(const AActor& Self, const AActor& Target)
	{
		return FVector::Dist2D(Self.GetActorLocation(), Target.GetActorLocation())
			- Self.GetSimpleCollisionRadius()
			- Target.GetSimpleCollisionRadius();
	}

	// 타겟 Actor로 이동을 요청하고 요청 결과를 Task 실행 결과로 변환한다.
	// 목표를 Actor로 지정해 경로 추종이 움직이는 타겟을 따라가게 한다.
	// ターゲットActorへ移動を要求し、要求結果をTaskの実行結果へ変換する。
	// 目標をActorで指定し、経路追従が動くターゲットへ追従するようにする。
	EStateTreeRunStatus RequestMoveToTarget(
		AAIController& AIController,
		FApproachTargetTaskInstanceData& InstanceData,
		AActor& Target)
	{
		const float AcceptanceRadius =
			FMath::Max(0.f, InstanceData.EngageDistance - InstanceData.ApproachMargin);

		FAIMoveRequest MoveRequest(&Target);
		MoveRequest.SetAcceptanceRadius(AcceptanceRadius);
		MoveRequest.SetReachTestIncludesAgentRadius(true);
		MoveRequest.SetReachTestIncludesGoalRadius(true);
		MoveRequest.SetUsePathfinding(true);
		MoveRequest.SetAllowPartialPath(true);
		MoveRequest.SetCanStrafe(true);

		const FPathFollowingRequestResult Result = AIController.MoveTo(MoveRequest);
		InstanceData.RequestedTarget = &Target;

		switch (Result.Code)
		{
		case EPathFollowingRequestResult::RequestSuccessful:
			InstanceData.MoveRequestID = Result.MoveId;
			InstanceData.bStartedMove = true;
			return EStateTreeRunStatus::Running;

		case EPathFollowingRequestResult::AlreadyAtGoal:
			InstanceData.bStartedMove = false;
			return EStateTreeRunStatus::Succeeded;

		case EPathFollowingRequestResult::Failed:
		default:
			InstanceData.bStartedMove = false;
			return EStateTreeRunStatus::Failed;
		}
	}
}

FStateTreeApproachTargetTask::FStateTreeApproachTargetTask()
{
	// 실행 중 타겟 변화와 교전 거리 도달을 확인한다.
	// 実行中にターゲットの変化と交戦距離への到達を確認する。
	bShouldCallTick = true;

	// 진입 시 전달받은 값을 이번 실행이 끝날 때까지 사용한다.
	// 開始時に受け取った値を今回の実行終了まで使用する。
	bShouldCopyBoundPropertiesOnTick = false;
	bShouldCopyBoundPropertiesOnExitState = false;

	// 같은 상태를 다시 선택하면 이전 요청을 정리하고 새로 시작한다.
	// 同じ状態を再選択した場合は、以前の要求を整理して再開始する。
	bShouldStateChangeOnReselect = true;
}

// =======================================================
// 상태 진입: 현재 타겟으로 이동 요청
// 이미 교전 거리 안이면 이동하지 않고 성공한다.
//
// 状態開始：現在のターゲットへ移動を要求する。
// すでに交戦距離内なら移動せずに成功する。
// =======================================================
EStateTreeRunStatus FStateTreeApproachTargetTask::EnterState(
	FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	// [1] 이전 실행 데이터를 초기화하고 현재 Pawn을 확인한다.
	// [1] 以前の実行データを初期化し、現在のPawnを確認する。
	InstanceData.MovingPawn.Reset();
	InstanceData.TargetSelector.Reset();
	InstanceData.RequestedTarget.Reset();
	InstanceData.MoveRequestID = FAIRequestID::InvalidRequest;
	InstanceData.bStartedMove = false;

	AAIController* AIController = InstanceData.AIController.Get();
	if (!IsValid(AIController)) return EStateTreeRunStatus::Failed;

	APawn* ControlledPawn = AIController->GetPawn();
	if (!IsValid(ControlledPawn)) return EStateTreeRunStatus::Failed;

	// [2] 진입 시 한 번만 셀렉터를 조회하고 현재 타겟을 확인한다.
	// [2] 開始時に一度だけセレクタを取得し、現在のターゲットを確認する。
	UTargetSelectorComponent* TargetSelector =
		ControlledPawn->FindComponentByClass<UTargetSelectorComponent>();
	if (!TargetSelector) return EStateTreeRunStatus::Failed;

	AActor* Target = TargetSelector->GetCurrentTarget();
	if (!IsValid(Target)) return EStateTreeRunStatus::Failed;

	InstanceData.MovingPawn = ControlledPawn;
	InstanceData.TargetSelector = TargetSelector;

	// [3] 이미 교전 거리 안이면 이동하지 않는다.
	// [3] すでに交戦距離内なら移動しない。
	if (CalculateSurfaceDistance2D(*ControlledPawn, *Target) <= InstanceData.EngageDistance)
	{
		return EStateTreeRunStatus::Succeeded;
	}

	return RequestMoveToTarget(*AIController, InstanceData, *Target);
}

// =======================================================
// 접근 진행 확인
// 교전 거리 도달을 경로 추종 완료보다 먼저 판정한다.
//
// 接近進行の確認。
// 交戦距離への到達を、経路追従の完了より先に判定する。
// =======================================================
EStateTreeRunStatus FStateTreeApproachTargetTask::Tick(
	FStateTreeExecutionContext& Context,
	const float DeltaTime) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	// [1] 요청이 남아 있으며 같은 Controller가 같은 Pawn을 제어하는지 확인한다.
	// [1] 要求が残っており、同じControllerが同じPawnを制御しているか確認する。
	AAIController* AIController = InstanceData.AIController.Get();
	APawn* MovingPawn = InstanceData.MovingPawn.Get();
	UTargetSelectorComponent* TargetSelector = InstanceData.TargetSelector.Get();

	if (!InstanceData.bStartedMove || !IsValid(AIController) || !MovingPawn || !TargetSelector)
	{
		InstanceData.bStartedMove = false;
		return EStateTreeRunStatus::Failed;
	}

	if (AIController->GetPawn() != MovingPawn)
	{
		InstanceData.bStartedMove = false;
		return EStateTreeRunStatus::Failed;
	}

	// [2] 현재 타겟을 확인한다. 상실하면 접근할 대상이 없으므로 실패한다.
	// [2] 現在のターゲットを確認する。喪失した場合は接近する対象がないため失敗する。
	AActor* Target = TargetSelector->GetCurrentTarget();
	if (!IsValid(Target)) return EStateTreeRunStatus::Failed;

	// [3] 교전 거리 안에 들어왔으면 경로 추종의 완료를 기다리지 않고 성공한다.
	//     이동 중단은 ExitState가 담당한다.
	// [3] 交戦距離内に入っていれば、経路追従の完了を待たずに成功する。
	//     移動の中断はExitStateが担当する。
	if (CalculateSurfaceDistance2D(*MovingPawn, *Target) <= InstanceData.EngageDistance)
	{
		return EStateTreeRunStatus::Succeeded;
	}

	// [4] 접근 중 타겟이 바뀌었으면 새 타겟으로 다시 요청한다.
	// [4] 接近中にターゲットが変わった場合は、新しいターゲットへ再要求する。
	if (InstanceData.RequestedTarget.Get() != Target)
	{
		return RequestMoveToTarget(*AIController, InstanceData, *Target);
	}

	// [5] 이동 중이면 이 Task의 요청인지 확인한다. 다른 요청으로 교체되었으면 관여하지 않는다.
	// [5] 移動中なら、このTaskの要求か確認する。他の要求に置き換わっていれば関与しない。
	const UPathFollowingComponent* PathFollowing = AIController->GetPathFollowingComponent();
	if (!PathFollowing)
	{
		InstanceData.bStartedMove = false;
		return EStateTreeRunStatus::Failed;
	}

	if (PathFollowing->GetStatus() != EPathFollowingStatus::Idle)
	{
		if (PathFollowing->GetCurrentRequestId() == InstanceData.MoveRequestID)
		{
			return EStateTreeRunStatus::Running;
		}

		InstanceData.bStartedMove = false;
		return EStateTreeRunStatus::Failed;
	}

	// [6] 교전 거리에 들어오지 못한 채 이동이 끝났다(경로 일부만 도달 등).
	// [6] 交戦距離に入れないまま移動が終了した（部分経路のみ到達など）。
	InstanceData.bStartedMove = false;
	return EStateTreeRunStatus::Failed;
}

// =======================================================
// 상태 이탈: 이 Task가 발행한 이동만 중단
//
// 状態終了：このTaskが発行した移動のみ中断する。
// =======================================================
void FStateTreeApproachTargetTask::ExitState(
	FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	// [1] 진행 중인 요청이 남아 있으면 같은 ID일 때만 중단한다.
	// [1] 進行中の要求が残っていれば、同じIDの場合のみ中断する。
	if (InstanceData.bStartedMove)
	{
		if (AAIController* AIController = InstanceData.AIController.Get())
		{
			if (UPathFollowingComponent* PathFollowing = AIController->GetPathFollowingComponent())
			{
				PathFollowing->AbortMove(
					*AIController,
					FPathFollowingResultFlags::OwnerFinished,
					InstanceData.MoveRequestID);
			}
		}
	}

	// [2] 다음 실행에 참조와 요청 정보가 남지 않도록 초기화한다.
	// [2] 次の実行に参照と要求情報を残さないよう初期化する。
	InstanceData.MovingPawn.Reset();
	InstanceData.TargetSelector.Reset();
	InstanceData.RequestedTarget.Reset();
	InstanceData.MoveRequestID = FAIRequestID::InvalidRequest;
	InstanceData.bStartedMove = false;
}