#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "TacticalTraversalComponent.generated.h"

UENUM(BlueprintType)
enum class ETraversalState : uint8
{
	Idle,               // 대기 중 (매니저의 통제를 따름)
	MovingToTakeoff,    // 도약점을 향해 이동 중
	Airborne            // 공중 체공 중 (물리 엔진 적용)
};

UCLASS(ClassGroup=(TacticalAI), meta=(BlueprintSpawnableComponent))
class TACTICALAI_API UTacticalTraversalComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UTacticalTraversalComponent();
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	
	// 매니저(Hub)가 단차를 발견했을 때 호출하는 진입점
	bool RequestTacticalTraversal(const FVector& FinalTargetLoc);

	// [API] 매니저의 상태 쿼리 및 통제용
	FORCEINLINE bool IsTraversing() const { return CurrentState != ETraversalState::Idle; }
	FORCEINLINE ETraversalState GetCurrentState() const { return CurrentState; }
	FORCEINLINE const FVector& GetCachedFinalTarget() const { return CachedFinalTarget; }

	// 목표물이 너무 멀어졌거나 타임아웃 시 강제 취소
	void AbortTraversal();

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void OnLanded(const FHitResult& Hit); // 캐릭터의 OnLanded에 바인딩할 콜백

private:
	ETraversalState CurrentState = ETraversalState::Idle;
    
	FVector CachedTakeoffPoint;
	FVector CachedFinalTarget;
	
	// 발사 동안 공중 마찰 임시 제거 후 OnLanded/Abort에서 복원할 원본값.
	float SavedBrakingDecelFalling = 0.f;
	float SavedFallingLateralFriction = 0.f;
	
	// [엣지 케이스 방어용 타이머]
	float TimeSpentMoving = 0.f;
	float TimeSpentAirborne = 0.f;
	bool bIsOnCooldown = false;
	float CooldownTimer = 0.f;

	// 공간 쿼리 및 수학 연산
	bool TryCalculateTakeoffPoint(const FVector& AgentLoc, const FVector& TargetLoc, float CapsuleRadius, FVector& OutTakeoffPoint) const;
	void ExecuteParabolaJump();
};