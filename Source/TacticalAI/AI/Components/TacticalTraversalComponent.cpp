#include "AI/Components/TacticalTraversalComponent.h"
#include "GameFramework/Character.h"
#include "AIController.h"
#include "Kismet/GameplayStatics.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Characters/PartyCharacter.h"
#include "DrawDebugHelpers.h"

UTacticalTraversalComponent::UTacticalTraversalComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
}

void UTacticalTraversalComponent::BeginPlay()
{
    Super::BeginPlay();

    // 엔진 네이티브 착지 이벤트 구독.
    if (ACharacter* OwnerCharacter = Cast<ACharacter>(GetOwner()))
    {
        OwnerCharacter->LandedDelegate.AddDynamic(this, &UTacticalTraversalComponent::OnLanded);
    }
}

bool UTacticalTraversalComponent::RequestTacticalTraversal(const FVector& FinalTargetLoc)
{
    // 이미 다른 행동 중이거나 쿨다운(실패 페널티) 중이면 거부.
    if (CurrentState != ETraversalState::Idle || bIsOnCooldown) return false;

    ACharacter* OwnerChar = Cast<ACharacter>(GetOwner());
    if (!OwnerChar || !OwnerChar->GetCharacterMovement()->IsMovingOnGround()) return false;

    const float HalfHeight = OwnerChar->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
    const FVector AgentFootLoc = OwnerChar->GetActorLocation() - FVector(0.f, 0.f, HalfHeight);
    const float Radius = OwnerChar->GetCapsuleComponent()->GetScaledCapsuleRadius();

    if (TryCalculateTakeoffPoint(AgentFootLoc, FinalTargetLoc, Radius, CachedTakeoffPoint))
    {
        CachedFinalTarget = FinalTargetLoc;
        CurrentState = ETraversalState::MovingToTakeoff;
        TimeSpentMoving = 0.f;

        if (AAIController* AICon = Cast<AAIController>(OwnerChar->GetController()))
        {
            AICon->MoveToLocation(CachedTakeoffPoint, 30.0f, true, true);
        }
        return true;
    }
    return false;
}

void UTacticalTraversalComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (CurrentState == ETraversalState::MovingToTakeoff)
    {
        TimeSpentMoving += DeltaTime;

        // [방어] 길찾기 버그 등으로 3초 안에 못 가면 강제 취소 + 쿨다운.
        if (TimeSpentMoving > 3.0f)
        {
            AbortTraversal();
            bIsOnCooldown = true;
            CooldownTimer = 2.0f;
            return;
        }

        ACharacter* OwnerChar = Cast<ACharacter>(GetOwner());
        if (!OwnerChar) return;

        const float HalfHeight = OwnerChar->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
        const FVector AgentFootLoc = OwnerChar->GetActorLocation() - FVector(0.f, 0.f, HalfHeight);
        const float DistSq2D = FVector::DistSquaredXY(AgentFootLoc, CachedTakeoffPoint);
        if (DistSq2D <= FMath::Square(100.0f))
        {
            ExecuteParabolaJump();
        }
    }
    else if (CurrentState == ETraversalState::Airborne)
    {
        TimeSpentAirborne += DeltaTime;

        // [방어] 벽 끼임 등 OnLanded 미발화 케이스.
        if (TimeSpentAirborne > 5.0f)
        {
            AbortTraversal();
        }
    }

    if (bIsOnCooldown)
    {
        CooldownTimer -= DeltaTime;
        if (CooldownTimer <= 0.f) bIsOnCooldown = false;
    }
}

bool UTacticalTraversalComponent::TryCalculateTakeoffPoint(const FVector& AgentFootLoc, const FVector& TargetLoc, float AgentRadius, FVector& OutTakeoffPoint) const
{
    UWorld* World = GetWorld();
    if (!World) return false;

    const float MaxJumpReach = 1800.0f;
    const float MaxJumpHeight = 1700.0f;

    const float ZDiff = TargetLoc.Z - AgentFootLoc.Z;
    if (FMath::Abs(ZDiff) > MaxJumpHeight) return false;

    const FVector ProjectedTargetXY(TargetLoc.X, TargetLoc.Y, AgentFootLoc.Z);
    if (FVector::DistSquared(AgentFootLoc, ProjectedTargetXY) > FMath::Square(MaxJumpReach)) return false;

    FVector DirectionToTarget = (ProjectedTargetXY - AgentFootLoc);
    if (DirectionToTarget.IsNearlyZero()) return false;
    DirectionToTarget.Normalize();

    // [1] 도약점 물리 스윕.
    const float TraceHeight = 50.0f;
    const FVector TraceStart = AgentFootLoc + FVector(0.f, 0.f, TraceHeight);
    const FVector TraceEnd = ProjectedTargetXY + FVector(0.f, 0.f, TraceHeight);

    FHitResult WallHit;
    const FCollisionShape Sphere = FCollisionShape::MakeSphere(AgentRadius * 0.8f);
    FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(CalcTakeoff), false, GetOwner());

    if (World->SweepSingleByChannel(WallHit, TraceStart, TraceEnd, FQuat::Identity, ECC_Visibility, Sphere, QueryParams))
    {
        // [다이내믹 오프셋 연산]
        // 기본 여유분(AgentRadius + 30)에, 단차 높이(ZDiff)의 절반만큼을 수평 거리로 추가 확보합니다.
        // ex) 1m 벽이면 50cm 물러나고, 2m 벽이면 1m 물러나서 뜁니다.
        const float DynamicOffset = AgentRadius + 30.0f + FMath::Max(0.0f, ZDiff * 0.5f);
        
        // 올라가는 점프 (벽면 감지).
        OutTakeoffPoint = WallHit.Location + (WallHit.ImpactNormal * DynamicOffset);
        OutTakeoffPoint.Z = AgentFootLoc.Z;
    }
    else
    {
        // 내려가는 점프 (낭떠러지).
        if (ZDiff < -20.0f)
        {
            OutTakeoffPoint = ProjectedTargetXY - (DirectionToTarget * (AgentRadius + 15.0f));
            OutTakeoffPoint.Z = AgentFootLoc.Z;
        }
        else return false;
    }

    // [2] 머리 위 천장 검증.
    ACharacter* OwnerChar = Cast<ACharacter>(GetOwner());
    if (OwnerChar)
    {
        const float HalfHeight = OwnerChar->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
        FHitResult CeilingHit;
        const FVector CeilingCheckStart = OutTakeoffPoint + FVector(0.f, 0.f, HalfHeight);
        const FVector CeilingCheckEnd = CeilingCheckStart + FVector(0.f, 0.f, FMath::Max(ZDiff + 100.0f, 200.0f));

        if (World->SweepSingleByChannel(CeilingHit, CeilingCheckStart, CeilingCheckEnd, FQuat::Identity, ECC_Visibility, Sphere, QueryParams))
        {
            return false;
        }
    }

    return true;
}

void UTacticalTraversalComponent::ExecuteParabolaJump()
{
    ACharacter* OwnerChar = Cast<ACharacter>(GetOwner());
    if (!OwnerChar) return;

    CurrentState = ETraversalState::Airborne;
    TimeSpentAirborne = 0.f;

    const float HalfHeight = OwnerChar->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
    const FVector FootLocation = OwnerChar->GetActorLocation() - FVector(0.f, 0.f, HalfHeight);
    const float LedgeClearance = OwnerChar->GetCapsuleComponent()->GetScaledCapsuleRadius() + 20.0f;
    FVector SafeTarget = CachedFinalTarget + FVector(0.f, 0.f, LedgeClearance);
    
    FVector LaunchVelocity;
    const bool bSuccess = UGameplayStatics::SuggestProjectileVelocity_CustomArc(
        this, LaunchVelocity, FootLocation, SafeTarget,
        GetWorld()->GetGravityZ(), 0.4f
    );

    if (!bSuccess)
    {
        AbortTraversal();
        return;
    }

    // [디버그] 발사 시작점 — 시안.
    DrawDebugSphere(GetWorld(), FootLocation, 30.0f, 12, FColor::Cyan, false, 5.0f, 0, 2.0f);
    // [디버그] 목표 착지점 — 빨강.
    DrawDebugSphere(GetWorld(), CachedFinalTarget, 30.0f, 12, FColor::Red, false, 5.0f, 0, 2.0f);

    if (UCharacterMovementComponent* MoveComp = OwnerChar->GetCharacterMovement())
    {
        // ───── 공중 마찰 임시 제거 ─────
        // 템플릿 기본값 BrakingDecelerationFalling=1500이 XY velocity를 0.17초 안에 소진시켜
        // 사실상 제자리 점프가 됨. 발사 동안만 0으로 만들고 OnLanded/AbortTraversal에서 복원.
        // FallingLateralFriction도 같은 이유로 차단.
        SavedBrakingDecelFalling = MoveComp->BrakingDecelerationFalling;
        SavedFallingLateralFriction = MoveComp->FallingLateralFriction;
        MoveComp->BrakingDecelerationFalling = 0.f;
        MoveComp->FallingLateralFriction = 0.f;
    }
    
    if (AAIController* AICon = Cast<AAIController>(OwnerChar->GetController()))
    {
        AICon->StopMovement();
    }
    OwnerChar->LaunchCharacter(LaunchVelocity, true, true);
}

void UTacticalTraversalComponent::OnLanded(const FHitResult& Hit)
{
    if (CurrentState != ETraversalState::Airborne) return;

    if (ACharacter* OwnerChar = Cast<ACharacter>(GetOwner()))
    {
        // [디버그] 실제 착지 위치 — 초록.
        const float HalfHeight = OwnerChar->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
        const FVector ActualFoot = OwnerChar->GetActorLocation() - FVector(0.f, 0.f, HalfHeight);
        DrawDebugSphere(GetWorld(), ActualFoot, 30.0f, 12, FColor::Green, false, 5.0f, 0, 2.0f);

        // 공중 마찰 복원.
        if (UCharacterMovementComponent* MoveComp = OwnerChar->GetCharacterMovement())
        {
            MoveComp->BrakingDecelerationFalling = SavedBrakingDecelFalling;
            MoveComp->FallingLateralFriction = SavedFallingLateralFriction;
        }
    }

    CurrentState = ETraversalState::Idle;
    TimeSpentAirborne = 0.f;
}

void UTacticalTraversalComponent::AbortTraversal()
{
    CurrentState = ETraversalState::Idle;
    TimeSpentMoving = 0.f;
    TimeSpentAirborne = 0.f;

    if (APartyCharacter* OwnerChar = Cast<APartyCharacter>(GetOwner()))
    {
        // 공중 마찰 복원 (Airborne에서 abort된 경우 대비).
        if (UCharacterMovementComponent* MoveComp = OwnerChar->GetCharacterMovement())
        {
            MoveComp->BrakingDecelerationFalling = SavedBrakingDecelFalling;
            MoveComp->FallingLateralFriction = SavedFallingLateralFriction;
        }

        if (AAIController* AICon = Cast<AAIController>(OwnerChar->GetController()))
        {
            AICon->StopMovement();
        }
    }
}