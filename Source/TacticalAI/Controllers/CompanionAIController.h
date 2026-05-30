#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "Interfaces/TacticalAvoidanceController.h"
#include "CompanionAIController.generated.h"

/**
 * 
 */
UCLASS()
class TACTICALAI_API ACompanionAIController : public AAIController,
											  public ITacticalAvoidanceController
{
	GENERATED_BODY()
	
public:
	// 기본 생성자 대신 FObjectInitializer를 파라미터로 받는 생성자로 변경
	ACompanionAIController(const FObjectInitializer& ObjectInitializer);
	
	virtual void SetAvoidanceRole(ECrowdAvoidanceRole CrowdRole) override;

protected:
	virtual void OnPossess(APawn* InPawn) override;
};
