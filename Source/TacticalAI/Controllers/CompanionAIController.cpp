#include "Controllers/CompanionAIController.h"
#include "AI/Components/TacticalCrowdFollowingComponent.h"
#include "AI/Components/PlayerCrowdAgentComponent.h"
#include "Components/StateTreeAIComponent.h"
#include "GameFramework/Pawn.h"

// 기존의 빈 생성자 ACompanionAIController::ACompanionAIController() 대신 아래 코드를 사용합니다.
// 💡 부모(AAIController)가 원래 만들려던 "PathFollowingComponent"의 타입을 
// 우리가 만든 UTacticalCrowdFollowingComponent로 강제 교체해서 생성하라고 지시합니다.
ACompanionAIController::ACompanionAIController(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UTacticalCrowdFollowingComponent>(
		TEXT("PathFollowingComponent")))
{
	// [1] StateTree 실행 컴포넌트를 생성하고 AI 행동 실행 컴포넌트로 연결한다.
	// [1] StateTree実行コンポーネントを生成し、AI行動の実行コンポーネントとして設定する。
	StateTreeComponent =
		CreateDefaultSubobject<UStateTreeAIComponent>(TEXT("StateTreeComponent"));
	BrainComponent = StateTreeComponent;

	// [2] 시작 시점은 Controller의 준비 상태와 빙의 상태를 확인한 뒤 결정한다.
	// [2] 開始時点はControllerの準備状態とPossess状態を確認して決定する。
	StateTreeComponent->SetStartLogicAutomatically(false);
	bStartAILogicOnPossess = false;
}

void ACompanionAIController::BeginPlay()
{
	Super::BeginPlay();
	
	// BeginPlay 이전에 빙의가 완료된 경우 여기서 행동을 시작한다.
	// BeginPlay前にPossessが完了している場合は、ここで行動を開始する。
	TryStartStateTree();
}

void ACompanionAIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	// 스왑으로 직전까지 플레이어 agent였던 Pawn이면 그 등록을 끈다.
	if (auto* Agent = InPawn->FindComponentByClass<UPlayerCrowdAgentComponent>())
	{
		Agent->SetObstacleActive(false);
	}

	// 이 Pawn을 일반 동료 role로.
	SetAvoidanceRole(ECrowdAvoidanceRole::Normal);
	
	// 기존 빙의 설정을 마친 뒤 StateTree 실행을 요청한다.
	// 既存のPossess設定を終えた後、StateTreeの実行を要求する。
	TryStartStateTree();
}

// =======================================================
// 빙의 해제
// Pawn 연결을 해제하기 전에 Task를 종료하여 이전 행동을 정리한다.
//
// Possess解除。
// Pawnとの接続を解除する前にTaskを終了し、以前の行動を整理する。
// =======================================================
void ACompanionAIController::OnUnPossess()
{
	// [1] 기존 Pawn을 참조할 수 있는 동안 실행 중인 행동을 종료한다.
	// [1] 元のPawnを参照できる間に、実行中の行動を終了する。
	if (StateTreeComponent)
	{
		StateTreeComponent->StopLogic(TEXT("PawnのPossess解除"));
	}

	// [2] 행동 정리 후 부모의 빙의 해제 처리를 수행한다.
	// [2] 行動の整理後、親のPossess解除処理を実行する。
	Super::OnUnPossess();
}

void ACompanionAIController::SetAvoidanceRole(ECrowdAvoidanceRole CrowdRole)
{
	if (auto* Crowd = Cast<UTacticalCrowdFollowingComponent>(GetPathFollowingComponent()))
	{
		Crowd->ApplyRole(CrowdRole);
	}
}

// =======================================================
// StateTree 시작
// Controller의 BeginPlay와 Pawn 빙의가 모두 완료된 경우 실행한다.
// 실행할 에셋과 파라미터는 StateTreeComponent에서 설정한다.
//
// StateTreeの開始。
// ControllerのBeginPlayとPawnのPossessが完了している場合に実行する。
// 実行するアセットとパラメータはStateTreeComponentで設定する。
// =======================================================
void ACompanionAIController::TryStartStateTree()
{
	// [1] Controller와Pawn의 준비 상태 및 중복 실행 여부를 확인한다.
	// [1] ControllerとPawnの準備状態および重複実行の有無を確認する。
	if (!HasActorBegunPlay() || !IsValid(GetPawn())) return;
	if (!StateTreeComponent) return;

	if (StateTreeComponent->IsRunning() || StateTreeComponent->IsPaused())
	{
		return;
	}

	// [2] 컴포넌트에 설정된 에셋과 파라미터로 실행을 시작한다.
	// [2] コンポーネントに設定されたアセットとパラメータで実行を開始する。
	StateTreeComponent->StartLogic();
}

void ACompanionAIController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Controller 종료 시에도 실행 중인 행동을 정리한다.
	// Controller終了時にも実行中の行動を整理する。
	if (StateTreeComponent)
	{
		StateTreeComponent->StopLogic(TEXT("Controllerの終了"));
	}

	Super::EndPlay(EndPlayReason);
}