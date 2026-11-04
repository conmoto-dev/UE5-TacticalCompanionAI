#pragma once

#include "CoreMinimal.h"
#include "AITypes.h"
#include "Tasks/StateTreeAITask.h"
#include "StateTreeApproachTargetTask.generated.h"

class AAIController;
class AActor;
class APawn;
class UTargetSelectorComponent;

// =======================================================
// 타겟 접근 Task의 실행별 데이터
// 교전 거리는 Evaluator Output에 바인딩한다.
//
// ターゲット接近Taskの実行ごとのデータ。
// 交戦距離はEvaluatorのOutputにバインドする。
// =======================================================
USTRUCT()
struct TACTICALAI_API FApproachTargetTaskInstanceData
{
	GENERATED_BODY()

	// 이동을 요청할 AIController.
	// 移動を要求するAIController。
	UPROPERTY(EditAnywhere, Category = Context)
	TObjectPtr<AAIController> AIController = nullptr;

	// 도착으로 판정할 캡슐 표면 사이 수평 거리(cm). 공격 개시 조건과 같은 값에 바인딩한다.
	// 到着と判定するカプセル表面間の水平距離(cm)。攻撃開始条件と同じ値にバインドする。
	UPROPERTY(EditAnywhere, Category = Parameter, meta = (ClampMin = "0.0", Units = "cm"))
	float EngageDistance = 150.f;

	// 경로 추종의 목표를 교전 거리보다 안쪽으로 두는 여유(cm).
	// 경로 추종이 교전 거리 경계에서 멈춰 공격 개시 조건이 실패하는 것을 막는다.
	// 経路追従の目標を交戦距離より内側に置く余白(cm)。
	// 経路追従が交戦距離の境界で停止し、攻撃開始条件が失敗することを防ぐ。
	UPROPERTY(EditAnywhere, Category = Parameter, meta = (ClampMin = "0.0", Units = "cm"))
	float ApproachMargin = 20.f;

	// 이동을 시작한 Pawn.
	// 移動を開始したPawn。
	UPROPERTY(Transient)
	TWeakObjectPtr<APawn> MovingPawn;

	// 진입 시 조회한 타겟 셀렉터. Pawn이 소유하며 Task는 소유하지 않는다.
	// 開始時に取得したターゲットセレクタ。Pawnが所有し、Taskは所有しない。
	UPROPERTY(Transient)
	TWeakObjectPtr<UTargetSelectorComponent> TargetSelector;

	// 현재 이동 요청의 목표 타겟.
	// 現在の移動要求の目標ターゲット。
	UPROPERTY(Transient)
	TWeakObjectPtr<AActor> RequestedTarget;

	// 이 Task가 마지막으로 발행한 이동 요청 ID.
	// このTaskが最後に発行した移動要求ID。
	FAIRequestID MoveRequestID;

	// 이번 Task가 진행 중인 이동 요청을 가지고 있는지 여부.
	// 今回のTaskが進行中の移動要求を持っているか。
	UPROPERTY(Transient)
	bool bStartedMove = false;
};

// =======================================================
// 타겟 접근
// 현재 전투 타겟을 향해 이동하고, 캡슐 표면 간격이 교전 거리 안에 들어오면 Succeeded.
// 타겟 상실·경로 실패·다른 이동 요청으로의 교체는 Failed.
// 접근 중 타겟이 바뀌면 새 타겟으로 이동을 다시 요청한다.
//
// ターゲット接近。
// 現在の戦闘ターゲットへ移動し、カプセル表面間隔が交戦距離内に入ればSucceeded。
// ターゲット喪失・経路失敗・他の移動要求への置き換えはFailed。
// 接近中にターゲットが変わった場合は、新しいターゲットへ移動を再要求する。
// =======================================================
USTRUCT(meta = (DisplayName = "Approach Target", Category = "Tactical AI|Combat"))
struct TACTICALAI_API FStateTreeApproachTargetTask : public FStateTreeAIActionTaskBase
{
	GENERATED_BODY()

	using FInstanceDataType = FApproachTargetTaskInstanceData;

	FStateTreeApproachTargetTask();

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