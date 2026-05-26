#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "TacticalTraversalComponent.generated.h"

UENUM(BlueprintType)
enum class ETraversalState : uint8
{
    Idle,               // 대기 중
    MovingToTakeoff,    // 도약점을 향해 이동 중 (매크로 길찾기)
    SteeringToTakeoff,  // 도약점 진입 조향 중 (마이크로 베지어 곡선 + Focus)
    Airborne            // 공중 체공 중
};

UCLASS(ClassGroup=(TacticalAI), meta=(BlueprintSpawnableComponent))
class TACTICALAI_API UTacticalTraversalComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UTacticalTraversalComponent();
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
    
    bool RequestTacticalTraversal(const FVector& FinalTargetLoc);
    void AbortTraversal();

    FORCEINLINE bool IsTraversing() const { return CurrentState != ETraversalState::Idle; }
    FORCEINLINE ETraversalState GetCurrentState() const { return CurrentState; }
    FORCEINLINE const FVector& GetCachedFinalTarget() const { return CachedFinalTarget; }

protected:
    virtual void BeginPlay() override;

    UFUNCTION()
    void OnLanded(const FHitResult& Hit);

    // =========================================================================
    // [설계 요구사항 2] 매직 넘버 외부 노출 (Designer Tweakable)
    // =========================================================================
    UPROPERTY(EditDefaultsOnly, Category = "Traversal|Limits", meta = (ClampMin = "0.0"))
    float MaxJumpReachXY = 700.f;  // 한 번에 점프할 수 있는 최대 XY 거리 (이상을 요구하면 클램핑됨)

    UPROPERTY(EditDefaultsOnly, Category = "Traversal|Limits", meta = (ClampMin = "0.0"))
    float MaxJumpHeightZ = 1000.f;  // 한 번에 극복 가능한 최대 높이

    UPROPERTY(EditDefaultsOnly, Category = "Traversal|Steering", meta = (ClampMin = "0.0"))
    float TakeoffApproachRadius = 250.f; // 이 반경 내에 들어오면 길찾기를 끄고 곡선 조향 시작

    UPROPERTY(EditDefaultsOnly, Category = "Traversal|Steering", meta = (ClampMin = "0.1"))
    float SteerSpeedMultiplier = 2.0f; // 곡선 진입 속도 배율

private:
    ETraversalState CurrentState = ETraversalState::Idle;
    
    FVector CachedTakeoffPoint;
    FVector CachedFinalTarget; // 거리가 클램핑(제한)된 실제 착지 목표점
    
    // ───── 베지어 마이크로 조향(Steering) 캐시 ─────
    FVector CachedApproachStartPoint; // P0
    FVector CachedControlPoint;       // P1
    float SteerAlpha = 0.f;
    
    // ───── 물리 및 회전 족쇄 캐시 ─────
    float SavedBrakingDecelFalling = 0.f;
    float SavedFallingLateralFriction = 0.f;
    bool bSavedOrientRotationToMovement = true;
    
    // ───── 엣지 케이스 타이머 ─────
    float TimeSpentMoving = 0.f;
    float TimeSpentAirborne = 0.f;
    bool bIsOnCooldown = false;
    float CooldownTimer = 0.f;

    bool TryCalculateTakeoffPoint(const FVector& AgentLoc, const FVector& TargetLoc, float CapsuleRadius, FVector& OutTakeoffPoint) const;
    void ExecuteParabolaJump();
};