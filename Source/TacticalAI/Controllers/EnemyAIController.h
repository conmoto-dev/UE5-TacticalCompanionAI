#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "EnemyAIController.generated.h"

class UStateTreeAIComponent;

// ==================================================
// 적 AI Controller.
// 일반 적과 보스의 StateTree 실행 수명을 관리한다.
//
// 敵AI Controller。
// 通常敵とボスのStateTree実行ライフサイクルを管理する。
// ==================================================
UCLASS(Blueprintable)
class TACTICALAI_API AEnemyAIController : public AAIController
{
	GENERATED_BODY()

public:
	AEnemyAIController();

protected:
	virtual void BeginPlay() override;
	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnUnPossess() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	// ==================================================
	// StateTree 실행 관리.
	// Controller 시작과 Pawn 빙의가 완료된 경우 실행한다.
	//
	// StateTree実行管理。
	// Controllerの開始とPawnのPossessが完了した場合に実行する。
	// ==================================================
	void TryStartStateTree();

	// 적 행동을 실행하는 컴포넌트. 실행할 StateTree는 Controller BP에서 설정한다.
	// 敵の行動を実行するコンポーネント。実行するStateTreeはController BPで設定する。
	UPROPERTY(VisibleAnywhere, Category = "AI|StateTree")
	TObjectPtr<UStateTreeAIComponent> StateTreeComponent;
};