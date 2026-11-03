#include "AI/StateTree/Tasks/StateTreeMoveToHomeSlotTask.h"

#include "AIController.h"
#include "GameFramework/Pawn.h"
#include "Navigation/PathFollowingComponent.h"
#include "StateTreeExecutionContext.h"
#include "AI/Interfaces/HomeSlotProvider.h"

namespace
{
	// 목적지로 이동을 요청하고 요청 결과를 Task 실행 결과로 변환한다.
	// 이동 설정은 비전투 추종 이동(MoveToLocation 기본값)과 같게 맞춘다.
	// 目的地へ移動を要求し、要求結果をTaskの実行結果へ変換する。
	// 移動設定は非戦闘の追従移動（MoveToLocationの既定値）と揃える。
	EStateTreeRunStatus RequestMoveToGoal(
		AAIController& AIController,
		FMoveToHomeSlotTaskInstanceData& InstanceData,
		const FVector& Goal)
	{
		FAIMoveRequest MoveRequest(Goal);
		MoveRequest.SetAcceptanceRadius(InstanceData.AcceptanceRadius);
		MoveRequest.SetReachTestIncludesAgentRadius(true);
		MoveRequest.SetUsePathfinding(true);
		MoveRequest.SetAllowPartialPath(true);
		MoveRequest.SetProjectGoalLocation(false);
		MoveRequest.SetCanStrafe(true);

		const FPathFollowingRequestResult Result = AIController.MoveTo(MoveRequest);
		InstanceData.RequestedGoal = Goal;

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

FStateTreeMoveToHomeSlotTask::FStateTreeMoveToHomeSlotTask()
{
	// 실행 중 홈 슬롯 변화와 이동 진행 상태를 확인한다.
	// 実行中にホームスロットの変化と移動の進行状態を確認する。
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
// 상태 진입: 현재 홈 슬롯으로 이동 요청
// 요청이 받아들여진 경우에만 종료 시 중단할 책임을 가진다.
//
// 状態開始：現在のホームスロットへ移動を要求する。
// 要求が受理された場合のみ、終了時の中断を担当する。
// =======================================================
EStateTreeRunStatus FStateTreeMoveToHomeSlotTask::EnterState(
	FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	// [1] 이전 실행 데이터를 초기화하고 현재 Pawn을 확인한다.
	// [1] 以前の実行データを初期化し、現在のPawnを確認する。
	InstanceData.MovingPawn.Reset();
	InstanceData.RequestedGoal = FVector::ZeroVector;
	InstanceData.MoveRequestID = FAIRequestID::InvalidRequest;
	InstanceData.bStartedMove = false;

	AAIController* AIController = InstanceData.AIController.Get();
	if (!IsValid(AIController)) return EStateTreeRunStatus::Failed;

	APawn* ControlledPawn = AIController->GetPawn();
	if (!IsValid(ControlledPawn)) return EStateTreeRunStatus::Failed;

	// [2] 홈 슬롯을 조회한다. 없으면 이동할 기준이 없으므로 실패한다.
	// [2] ホームスロットを照会する。なければ移動の基準がないため失敗する。
	const IHomeSlotProvider* HomeProvider = Cast<IHomeSlotProvider>(ControlledPawn);
	if (!HomeProvider) return EStateTreeRunStatus::Failed;

	FVector HomeSlot = FVector::ZeroVector;
	if (!HomeProvider->TryGetHomeSlot(HomeSlot)) return EStateTreeRunStatus::Failed;

	// [3] 이동을 요청하고 요청 대상 Pawn을 보관한다.
	// [3] 移動を要求し、要求対象のPawnを保持する。
	InstanceData.MovingPawn = ControlledPawn;
	return RequestMoveToGoal(*AIController, InstanceData, HomeSlot);
}

// =======================================================
// 이동 진행 확인
// 이동 중 재커밋된 홈 슬롯을 따라가고, 경로 추종 결과를 실행 결과로 변환한다.
//
// 移動進行の確認。
// 移動中に再コミットされたホームスロットへ追従し、経路追従の結果を実行結果へ変換する。
// =======================================================
EStateTreeRunStatus FStateTreeMoveToHomeSlotTask::Tick(
	FStateTreeExecutionContext& Context,
	const float DeltaTime) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	// [1] 요청이 남아 있으며 같은 Controller가 같은 Pawn을 제어하는지 확인한다.
	// [1] 要求が残っており、同じControllerが同じPawnを制御しているか確認する。
	AAIController* AIController = InstanceData.AIController.Get();
	APawn* MovingPawn = InstanceData.MovingPawn.Get();

	if (!InstanceData.bStartedMove || !IsValid(AIController) || !MovingPawn)
	{
		InstanceData.bStartedMove = false;
		return EStateTreeRunStatus::Failed;
	}

	if (AIController->GetPawn() != MovingPawn)
	{
		InstanceData.bStartedMove = false;
		return EStateTreeRunStatus::Failed;
	}

	// [2] 홈 슬롯을 다시 조회한다. 전투 종료 등으로 사라지면 이동 기준을 잃은 것이다.
	// [2] ホームスロットを再照会する。戦闘終了などで消えた場合は移動の基準を失っている。
	const IHomeSlotProvider* HomeProvider = Cast<IHomeSlotProvider>(MovingPawn);
	FVector HomeSlot = FVector::ZeroVector;

	if (!HomeProvider || !HomeProvider->TryGetHomeSlot(HomeSlot))
	{
		return EStateTreeRunStatus::Failed;
	}

	// [3] 이동 중 재커밋으로 홈 슬롯이 기준 이상 바뀌었으면 새 위치로 다시 요청한다.
	// [3] 移動中の再コミットでホームスロットが基準以上変わった場合は、新しい位置へ再要求する。
	if (FVector::DistSquared2D(HomeSlot, InstanceData.RequestedGoal) >
		FMath::Square(static_cast<double>(InstanceData.RepathDistance)))
	{
		return RequestMoveToGoal(*AIController, InstanceData, HomeSlot);
	}

	// [4] 이동 중이면 이 Task의 요청인지 확인한다. 다른 요청으로 교체되었으면 관여하지 않는다.
	// [4] 移動中なら、このTaskの要求か確認する。他の要求に置き換わっていれば関与しない。
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

	// [5] 이동이 끝났으면 목적지 도달 여부를 결과로 반환한다.
	// [5] 移動が終了していれば、目的地への到達有無を結果として返す。
	InstanceData.bStartedMove = false;
	return PathFollowing->DidMoveReachGoal()
		? EStateTreeRunStatus::Succeeded
		: EStateTreeRunStatus::Failed;
}

// =======================================================
// 상태 이탈: 이 Task가 발행한 이동만 중단
// 요청 ID를 지정하므로 다른 시스템이 발행한 이동은 중단하지 않는다.
//
// 状態終了：このTaskが発行した移動のみ中断する。
// 要求IDを指定するため、他のシステムが発行した移動は中断しない。
// =======================================================
void FStateTreeMoveToHomeSlotTask::ExitState(
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
	InstanceData.RequestedGoal = FVector::ZeroVector;
	InstanceData.MoveRequestID = FAIRequestID::InvalidRequest;
	InstanceData.bStartedMove = false;
}