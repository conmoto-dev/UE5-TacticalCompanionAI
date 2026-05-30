#include "AI/Components/PlayerCrowdAgentComponent.h"
#include "AI/CrowdAvoidanceTypes.h"
#include "Navigation/CrowdManager.h"
#include "GameFramework/Character.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"

UPlayerCrowdAgentComponent::UPlayerCrowdAgentComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

void UPlayerCrowdAgentComponent::BeginPlay()
{
    Super::BeginPlay();
    CachedOwnerChar = Cast<ACharacter>(GetOwner());
    // 자동 등록 안 함. 컨트롤러가 빙의 시 SetObstacleActive(true)로 켠다.
}

void UPlayerCrowdAgentComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    SetObstacleActive(false); // 파괴 시 안전 해제
    Super::EndPlay(EndPlayReason);
}

void UPlayerCrowdAgentComponent::SetObstacleActive(bool bActive)
{
    UCrowdManager* CrowdManager = UCrowdManager::GetCurrent(GetWorld());
    if (!CrowdManager) return;

    if (bActive && !bIsRegistered)
    {
        CrowdManager->RegisterAgent(this);
        bIsRegistered = true;
    }
    else if (!bActive && bIsRegistered)
    {
        CrowdManager->UnregisterAgent(this);
        bIsRegistered = false;
    }
}

FVector UPlayerCrowdAgentComponent::GetCrowdAgentLocation() const
{
    return CachedOwnerChar.IsValid() ? CachedOwnerChar->GetActorLocation() : FVector::ZeroVector;
}

FVector UPlayerCrowdAgentComponent::GetCrowdAgentVelocity() const
{
    return CachedOwnerChar.IsValid() ? CachedOwnerChar->GetVelocity() : FVector::ZeroVector;
}

void UPlayerCrowdAgentComponent::GetCrowdAgentCollisions(float& CylinderRadius, float& CylinderHalfHeight) const
{
    if (CachedOwnerChar.IsValid())
    {
        CachedOwnerChar->GetCapsuleComponent()->GetScaledCapsuleSize(CylinderRadius, CylinderHalfHeight);
    }
    else
    {
        CylinderRadius = 0.f;      // invalid 시 out 파라미터 미초기화 방지
        CylinderHalfHeight = 0.f;
    }
}

float UPlayerCrowdAgentComponent::GetCrowdAgentMaxSpeed() const
{
    return CachedOwnerChar.IsValid() ? CachedOwnerChar->GetCharacterMovement()->MaxWalkSpeed : 0.f;
}

// ───── 권력 계층(Priority) ─────
int32 UPlayerCrowdAgentComponent::GetCrowdAgentAvoidanceGroup() const
{
    return CrowdGroupBits::Leader; // 그룹 1
}

int32 UPlayerCrowdAgentComponent::GetCrowdAgentGroupsToAvoid() const
{
    return 0; // 아무도 안 피함
}

int32 UPlayerCrowdAgentComponent::GetCrowdAgentGroupsToIgnore() const
{
    // 0xFFFF 금지: 정의된 그룹만 무시. 미래 조합 그룹까지 무차별 무시하는 침묵 버그 방지.
    return CrowdGroupBits::Normal | CrowdGroupBits::Yielding;
}