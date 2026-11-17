#include "Controllers/EnemyAIController.h"
#include "Components/StateTreeAIComponent.h"
#include "GameFramework/Pawn.h"

AEnemyAIController::AEnemyAIController()
{
	// [1] StateTree 실행 컴포넌트를 생성하고 BrainComponent로 지정한다.
	// [1] StateTree実行コンポーネントを生成し、BrainComponentに設定する。
	StateTreeComponent =
		CreateDefaultSubobject<UStateTreeAIComponent>(TEXT("StateTreeComponent"));
	BrainComponent = StateTreeComponent;

	// [2] 시작 조건을 Controller에서 확인하도록 자동 실행을 끈다.
	// [2] Controller側で開始条件を確認するため、自動実行を無効にする。
	StateTreeComponent->SetStartLogicAutomatically(false);
	bStartAILogicOnPossess = false;
}

void AEnemyAIController::BeginPlay()
{
	Super::BeginPlay();
	
	TryStartStateTree();
}

void AEnemyAIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	
	TryStartStateTree();
}

// 빙의 해제.
// Possess解除。
void AEnemyAIController::OnUnPossess()
{
	// [1] Pawn 연결을 해제하기 전에 실행 중인 행동을 정리한다.
	// [1] Pawnとの接続を解除する前に、実行中の行動を終了する。
	if (StateTreeComponent)
	{
		StateTreeComponent->StopLogic(TEXT("敵PawnのPossess解除"));
	}

	// [2] 행동 종료 후 부모의 빙의 해제 처리를 수행한다.
	// [2] 行動終了後、親のPossess解除処理を実行する。
	Super::OnUnPossess();
}

void AEnemyAIController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Controller 종료 시 실행 중인 행동을 정리한다.
	// Controller終了時に実行中の行動を終了する。
	if (StateTreeComponent)
	{
		StateTreeComponent->StopLogic(TEXT("敵Controllerの終了"));
	}

	Super::EndPlay(EndPlayReason);
}

// ==================================================
// StateTree 시작.
// Controller 시작과 Pawn 연결을 확인하고 중복 실행을 방지한다.
//
// StateTreeの開始。
// Controllerの開始とPawnの接続を確認し、重複実行を防ぐ。
// ==================================================
void AEnemyAIController::TryStartStateTree()
{
	// [1] 실행에 필요한 Controller와 Pawn의 상태를 확인한다.
	// [1] 実行に必要なControllerとPawnの状態を確認する。
	if (!HasActorBegunPlay() || !IsValid(GetPawn())) return;
	if (!StateTreeComponent) return;

	if (StateTreeComponent->IsRunning() || StateTreeComponent->IsPaused())
	{
		return;
	}

	// [2] 컴포넌트에 설정된 StateTree와 파라미터로 실행한다.
	// [2] コンポーネントに設定されたStateTreeとパラメータで実行する。
	StateTreeComponent->StartLogic();
}