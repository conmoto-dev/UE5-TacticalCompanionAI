#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Navigation/CrowdAgentInterface.h"
#include "PlayerCrowdAgentComponent.generated.h"


class ACharacter;

UCLASS(ClassGroup=(TacticalAI), meta=(BlueprintSpawnableComponent))
class TACTICALAI_API UPlayerCrowdAgentComponent : public UActorComponent, public ICrowdAgentInterface
{
	GENERATED_BODY()
public:
	UPlayerCrowdAgentComponent();

	// 외부(컨트롤러)에서 빙의 상태에 따라 등록/해제를 지시하는 스위치.
	void SetObstacleActive(bool bActive);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:
	// ICrowdAgentInterface
	virtual FVector GetCrowdAgentLocation() const override;
	virtual FVector GetCrowdAgentVelocity() const override;
	virtual void GetCrowdAgentCollisions(float& CylinderRadius, float& CylinderHalfHeight) const override;
	virtual float GetCrowdAgentMaxSpeed() const override;
	virtual int32 GetCrowdAgentAvoidanceGroup() const override;
	virtual int32 GetCrowdAgentGroupsToAvoid() const override;
	virtual int32 GetCrowdAgentGroupsToIgnore() const override;

private:
	TWeakObjectPtr<ACharacter> CachedOwnerChar;
	bool bIsRegistered = false;
};