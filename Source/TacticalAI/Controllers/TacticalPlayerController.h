#pragma once
#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "Interfaces/TacticalAvoidanceController.h"
#include "TacticalPlayerController.generated.h"

UCLASS()
class TACTICALAI_API ATacticalPlayerController : public APlayerController,
												 public ITacticalAvoidanceController
{
	GENERATED_BODY()
public:
	virtual void SetAvoidanceRole(ECrowdAvoidanceRole CrowdRole) override;

protected:
	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnUnPossess() override;
};