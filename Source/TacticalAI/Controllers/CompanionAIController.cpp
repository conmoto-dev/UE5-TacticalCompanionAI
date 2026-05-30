#include "Controllers/CompanionAIController.h"
#include "AI/Components/TacticalCrowdFollowingComponent.h"
#include "AI/Components/PlayerCrowdAgentComponent.h"
#include "GameFramework/Pawn.h"

// 기존의 빈 생성자 ACompanionAIController::ACompanionAIController() 대신 아래 코드를 사용합니다.
// 💡 부모(AAIController)가 원래 만들려던 "PathFollowingComponent"의 타입을 
// 우리가 만든 UTacticalCrowdFollowingComponent로 강제 교체해서 생성하라고 지시합니다.
ACompanionAIController::ACompanionAIController(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UTacticalCrowdFollowingComponent>(
		TEXT("PathFollowingComponent")))
{
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
}

void ACompanionAIController::SetAvoidanceRole(ECrowdAvoidanceRole CrowdRole)
{
	if (auto* Crowd = Cast<UTacticalCrowdFollowingComponent>(GetPathFollowingComponent()))
	{
		Crowd->ApplyRole(CrowdRole);
	}
}