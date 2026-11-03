#pragma once

#include "CoreMinimal.h"
#include "AITypes.h"
#include "Tasks/StateTreeAITask.h"
#include "StateTreeMoveToHomeSlotTask.generated.h"

class AAIController;
class APawn;

// =======================================================
// 홈 슬롯 이동 Task의 실행별 데이터
// Controller는 Context에서, 이동 기준값은 Parameter에서 전달받는다.
// 요청 ID는 이 Task가 발행한 이동만 확인·중단하기 위해 보관한다.
//
// ホームスロット移動Taskの実行ごとのデータ。
// ControllerはContextから、移動の基準値はParameterから受け取る。
// 要求IDは、このTaskが発行した移動のみを確認・中断するために保持する。
// =======================================================
USTRUCT()
struct TACTICALAI_API FMoveToHomeSlotTaskInstanceData
{
	GENERATED_BODY()

	// 이동을 요청할 AIController.
	// 移動を要求するAIController。
	UPROPERTY(EditAnywhere, Category = Context)
	TObjectPtr<AAIController> AIController = nullptr;

	// 도착 판정 반경(cm). 캐릭터 캡슐 반경을 포함한 거리로 판정한다.
	// 到着判定半径(cm)。キャラクターのカプセル半径を含めた距離で判定する。
	UPROPERTY(EditAnywhere, Category = Parameter, meta = (ClampMin = "0.0", Units = "cm"))
	float AcceptanceRadius = 30.f;

	// 이동 중 홈 슬롯이 이 거리를 초과해 바뀌면 새 위치로 이동을 다시 요청한다(cm).
	// 작을수록 재커밋을 빠르게 따라가지만 경로 재계산이 잦아진다.
	// 移動中にホームスロットがこの距離を超えて変わった場合、新しい位置へ移動を再要求する(cm)。
	// 小さいほど再コミットへの追従は速いが、経路の再計算が増える。
	UPROPERTY(EditAnywhere, Category = Parameter, meta = (ClampMin = "1.0", Units = "cm"))
	float RepathDistance = 50.f;

	// 이동을 시작한 Pawn. 실행 중 빙의 대상이 바뀌었는지 확인한다.
	// 移動を開始したPawn。実行中にPossess対象が変わったか確認する。
	UPROPERTY(Transient)
	TWeakObjectPtr<APawn> MovingPawn;

	// 마지막으로 요청한 목적지.
	// 最後に要求した目的地。
	UPROPERTY(Transient)
	FVector RequestedGoal = FVector::ZeroVector;

	// 이 Task가 마지막으로 발행한 이동 요청 ID.
	// 리플렉션 대상이 아닌 값 타입이므로 UPROPERTY 없이 보관한다.
	// このTaskが最後に発行した移動要求ID。
	// リフレクション対象ではない値型のため、UPROPERTYなしで保持する。
	FAIRequestID MoveRequestID;

	// 이번 Task가 진행 중인 이동 요청을 가지고 있는지 여부.
	// 今回のTaskが進行中の移動要求を持っているか。
	UPROPERTY(Transient)
	bool bStartedMove = false;
};

// =======================================================
// 홈 슬롯 이동
// 현재 홈 슬롯으로 이동을 요청하고, 이동 중 홈 슬롯이 바뀌면 목적지를 갱신한다.
// 도착하면 Succeeded, 홈 슬롯 상실·경로 실패·다른 이동 요청으로의 교체는 Failed.
// 슬롯의 결정은 배치 시스템이 담당하고, 이 Task는 이동 실행만 담당한다.
//
// ホームスロットへの移動。
// 現在のホームスロットへ移動を要求し、移動中にホームスロットが変わったら目的地を更新する。
// 到着でSucceeded、ホームスロット喪失・経路失敗・他の移動要求への置き換えはFailed。
// スロットの決定は配置システムが担当し、このTaskは移動の実行のみを担当する。
// =======================================================
USTRUCT(meta = (DisplayName = "Move To Home Slot", Category = "Tactical AI|Combat"))
struct TACTICALAI_API FStateTreeMoveToHomeSlotTask : public FStateTreeAIActionTaskBase
{
	GENERATED_BODY()

	using FInstanceDataType = FMoveToHomeSlotTaskInstanceData;

	FStateTreeMoveToHomeSlotTask();

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