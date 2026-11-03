#pragma once

#include "CoreMinimal.h"
#include "Tasks/StateTreeAITask.h"
#include "StateTreeCombatStrafeTask.generated.h"

class AAIController;
class UCombatMicroMovementComponent;

// =======================================================
// 횡이동 Task의 실행별 데이터
// Controller는 Context에서 전달받고, 컴포넌트는 진입 시 조회한다.
//
// 横移動Taskの実行ごとのデータ。
// ControllerはContextから受け取り、コンポーネントは開始時に取得する。
// =======================================================
USTRUCT()
struct TACTICALAI_API FCombatStrafeTaskInstanceData
{
	GENERATED_BODY()

	// 횡이동할 Pawn을 제어하는 AIController.
	// 横移動するPawnを制御するAIController。
	UPROPERTY(EditAnywhere, Category = Context)
	TObjectPtr<AAIController> AIController = nullptr;

	// 이번 실행에서 요청한 컴포넌트. 
	// 今回の実行で要求したコンポーネント。
	UPROPERTY(Transient)
	TWeakObjectPtr<UCombatMicroMovementComponent> MicroMovement;

	// 이번 Task가 이동 시작에 성공했는지 여부.
	// 今回のTaskが移動開始に成功したか。
	UPROPERTY(Transient)
	bool bStartedStrafe = false;
};

// =======================================================
// 전투 횡이동 1회 실행
// 이동을 요청하고 결과를 확인하며, 상태 이탈 시 남은 이동을 취소한다.
// 대기·반복·전체 행동 시간은 StateTree에서 별도로 관리한다.
//
// 戦闘中の横移動を一回実行する。
// 移動を要求して結果を確認し、状態終了時に残っている移動をキャンセルする。
// 待機・繰り返し・行動全体の時間はStateTreeで別途管理する。
// =======================================================
USTRUCT(meta = (DisplayName = "Combat Strafe", Category = "Tactical AI|Combat"))
struct TACTICALAI_API FStateTreeCombatStrafeTask : public FStateTreeAIActionTaskBase
{
	GENERATED_BODY()

	using FInstanceDataType = FCombatStrafeTaskInstanceData;

	FStateTreeCombatStrafeTask();

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