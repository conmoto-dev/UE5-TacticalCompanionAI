#include "Controllers/TacticalPlayerController.h"
#include "AI/Components/PlayerCrowdAgentComponent.h"
#include "GameFramework/Pawn.h"

void ATacticalPlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	// 플레이어가 빙의한 Pawn = Leader = crowd 장애물.
	SetAvoidanceRole(ECrowdAvoidanceRole::Leader);
}

void ATacticalPlayerController::OnUnPossess()
{
	// 떠나기 전, 빙의 중이던 Pawn의 장애물 등록을 끈다. (Super가 Pawn 참조를 끊으므로 그 전에 처리)
	if (APawn* Old = GetPawn())
	{
		if (auto* Obs = Old->FindComponentByClass<UPlayerCrowdAgentComponent>())
		{
			Obs->SetObstacleActive(false);
		}
	}
	Super::OnUnPossess();
}

void ATacticalPlayerController::SetAvoidanceRole(ECrowdAvoidanceRole CrowdRole)
{
	APawn* P = GetPawn();
	if (!P) return;
	if (auto* Obs = P->FindComponentByClass<UPlayerCrowdAgentComponent>())
	{
		// 플레이어 입장에서 의미 있는 건 Leader뿐. 그 외 role이 오면 장애물 끔(방어).
		Obs->SetObstacleActive(CrowdRole == ECrowdAvoidanceRole::Leader);
	}
}