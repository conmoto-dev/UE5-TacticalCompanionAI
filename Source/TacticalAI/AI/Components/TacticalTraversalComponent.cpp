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

    if (ACharacter* OwnerCharacter = Cast<ACharacter>(GetOwner()))
    {
        OwnerCharacter->LandedDelegate.AddDynamic(this, &UTacticalTraversalComponent::OnLanded);
    }
}

bool UTacticalTraversalComponent::RequestTacticalTraversal(const FVector& FinalTargetLoc)
{
    if (CurrentState != ETraversalState::Idle || bIsOnCooldown) return false;

    ACharacter* OwnerChar = Cast<ACharacter>(GetOwner());
    if (!OwnerChar || !OwnerChar->GetCharacterMovement()->IsMovingOnGround()) return false;

    const float HalfHeight = OwnerChar->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
    const FVector AgentFootLoc = OwnerChar->GetActorLocation() - FVector(0.f, 0.f, HalfHeight);
    const float Radius = OwnerChar->GetCapsuleComponent()->GetScaledCapsuleRadius();

    // 1. 점프 XY 거리 클램핑 (목표가 너무 멀면 최대 사거리까지만 점프 타겟 설정)
    FVector ToTarget = FinalTargetLoc - AgentFootLoc;
    const float OriginalZDiff = ToTarget.Z;
    ToTarget.Z = 0.f; // XY 평면 투영
    
    if (ToTarget.SizeSquared() > FMath::Square(MaxJumpReachXY))
    {
        ToTarget = ToTarget.GetSafeNormal() * MaxJumpReachXY;
    }
    
    FVector ClampedTargetLoc = AgentFootLoc + ToTarget;
    ClampedTargetLoc.Z = FinalTargetLoc.Z;

    // 2. 가상 도약점 연산 (이전의 누락된 코드 복원!)
    if (TryCalculateTakeoffPoint(AgentFootLoc, ClampedTargetLoc, Radius, CachedTakeoffPoint))
    {
        CachedFinalTarget = ClampedTargetLoc;
        TimeSpentMoving = 0.f;

        // [엣지 케이스 방어] 이미 도약점 반경 안에 들어와 있는가?
        if (FVector::DistSquaredXY(AgentFootLoc, CachedTakeoffPoint) <= FMath::Square(TakeoffApproachRadius))
        {
            // MoveTo를 생략하고 즉시 Steering(조향) 상태로 직행
            CurrentState = ETraversalState::SteeringToTakeoff;
            SteerAlpha = 0.f;
            CachedApproachStartPoint = AgentFootLoc;

            if (AAIController* AICon = Cast<AAIController>(OwnerChar->GetController()))
            {
                AICon->StopMovement();
                FVector JumpDir = (CachedFinalTarget - CachedTakeoffPoint).GetSafeNormal2D();
                AICon->SetFocalPoint(CachedTakeoffPoint + (JumpDir * 1000.f));
                CachedControlPoint = CachedTakeoffPoint - (JumpDir * (TakeoffApproachRadius * 0.7f));
            }
            if (UCharacterMovementComponent* MoveComp = OwnerChar->GetCharacterMovement())
            {
                bSavedOrientRotationToMovement = MoveComp->bOrientRotationToMovement;
                MoveComp->bOrientRotationToMovement = false;
                MoveComp->bUseControllerDesiredRotation = true;
            }
        }
        else
        {
            // 거리가 멀다면 매크로 길찾기(MoveTo) 상태로 시작
            CurrentState = ETraversalState::MovingToTakeoff;
            if (AAIController* AICon = Cast<AAIController>(OwnerChar->GetController()))
            {
                AICon->MoveToLocation(CachedTakeoffPoint, 30.0f, true, true);
            }
        }
        return true;
    }
    return false;
}

void UTacticalTraversalComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    
    ACharacter* OwnerChar = Cast<ACharacter>(GetOwner());
    if (!OwnerChar) return;

    const float HalfHeight = OwnerChar->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
    const FVector AgentFootLoc = OwnerChar->GetActorLocation() - FVector(0.f, 0.f, HalfHeight);

    // [상태 1] 길찾기로 도약점 접근 중
    if (CurrentState == ETraversalState::MovingToTakeoff)
    {
        TimeSpentMoving += DeltaTime;
        if (TimeSpentMoving > 3.0f)
        {
            AbortTraversal();
            bIsOnCooldown = true;
            CooldownTimer = 2.0f;
            return;
        }

        // 도약점 반경 진입 시 조향(Steering)으로 상태 전이
        if (FVector::DistSquaredXY(AgentFootLoc, CachedTakeoffPoint) <= FMath::Square(TakeoffApproachRadius))
        {
            CurrentState = ETraversalState::SteeringToTakeoff;
            SteerAlpha = 0.f;
            CachedApproachStartPoint = AgentFootLoc;

            AAIController* AICon = Cast<AAIController>(OwnerChar->GetController());
            UCharacterMovementComponent* MoveComp = OwnerChar->GetCharacterMovement();

            if (AICon && MoveComp)
            {
                AICon->StopMovement();
                FVector JumpDir = (CachedFinalTarget - CachedTakeoffPoint).GetSafeNormal2D();
                AICon->SetFocalPoint(CachedTakeoffPoint + (JumpDir * 1000.f));
                
                bSavedOrientRotationToMovement = MoveComp->bOrientRotationToMovement;
                MoveComp->bOrientRotationToMovement = false;
                MoveComp->bUseControllerDesiredRotation = true;

                CachedControlPoint = CachedTakeoffPoint - (JumpDir * (TakeoffApproachRadius * 0.7f));
            }
        }
    }
    // [상태 2] 마이크로 베지어 곡선 조향 중
    else if (CurrentState == ETraversalState::SteeringToTakeoff)
    {
        SteerAlpha += (DeltaTime * SteerSpeedMultiplier);
        SteerAlpha = FMath::Clamp(SteerAlpha, 0.f, 1.0f);

        // 2차 베지어 수학 연산
        FVector DesiredLoc = FMath::Lerp(
            FMath::Lerp(CachedApproachStartPoint, CachedControlPoint, SteerAlpha),
            FMath::Lerp(CachedControlPoint, CachedTakeoffPoint, SteerAlpha),
            SteerAlpha
        );

        FVector MoveDir = (DesiredLoc - AgentFootLoc).GetSafeNormal2D();
        OwnerChar->AddMovementInput(MoveDir, 1.0f);

        DrawDebugPoint(GetWorld(), DesiredLoc + FVector(0,0,HalfHeight), 5.0f, FColor::Yellow, false, 0.5f);

        if (SteerAlpha >= 1.0f || FVector::DistSquaredXY(AgentFootLoc, CachedTakeoffPoint) <= FMath::Square(30.f))
        {
            ExecuteParabolaJump();
        }
    }
    // [상태 3] 공중 체공 중 (Airborne)
    else if (CurrentState == ETraversalState::Airborne)
    {
        TimeSpentAirborne += DeltaTime;
        if (TimeSpentAirborne > 5.0f) // 착지 미발화 엣지 케이스 방어
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

    const float ZDiff = TargetLoc.Z - AgentFootLoc.Z;
    if (FMath::Abs(ZDiff) > MaxJumpHeightZ) return false;

    const FVector ProjectedTargetXY(TargetLoc.X, TargetLoc.Y, AgentFootLoc.Z);
    if (FVector::DistSquared(AgentFootLoc, ProjectedTargetXY) > FMath::Square(MaxJumpReachXY)) return false;

    FVector DirectionToTarget = (ProjectedTargetXY - AgentFootLoc);
    if (DirectionToTarget.IsNearlyZero()) return false;
    DirectionToTarget.Normalize();

    // 1. 도약점 탐색 물리 스윕
    const float TraceHeight = 50.0f;
    const FVector TraceStart = AgentFootLoc + FVector(0.f, 0.f, TraceHeight);
    const FVector TraceEnd = ProjectedTargetXY + FVector(0.f, 0.f, TraceHeight);

    FHitResult WallHit;
    const FCollisionShape Sphere = FCollisionShape::MakeSphere(AgentRadius * 0.8f);
    FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(CalcTakeoff), false, GetOwner());

    if (World->SweepSingleByChannel(WallHit, TraceStart, TraceEnd, FQuat::Identity, ECC_Visibility, Sphere, QueryParams))
    {
        // 다이내믹 오프셋 연산: 벽면 법선(Normal)을 기준으로 물러남
        const float DynamicOffset = AgentRadius + 30.0f + FMath::Max(0.0f, ZDiff * 0.5f);
        OutTakeoffPoint = WallHit.Location + (WallHit.ImpactNormal * DynamicOffset);
        OutTakeoffPoint.Z = AgentFootLoc.Z;
    }
    else
    {
        // 내려가는 점프(낭떠러지) 처리
        if (ZDiff < -20.0f)
        {
            OutTakeoffPoint = ProjectedTargetXY - (DirectionToTarget * (AgentRadius + 15.0f));
            OutTakeoffPoint.Z = AgentFootLoc.Z;
        }
        else return false;
    }

    // 2. 머리 위 천장 검증 (Clearance Check)
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

    if (AAIController* AICon = Cast<AAIController>(OwnerChar->GetController()))
    {
        AICon->ClearFocus(EAIFocusPriority::Gameplay);
    }

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

    DrawDebugSphere(GetWorld(), FootLocation, 30.0f, 12, FColor::Cyan, false, 5.0f, 0, 2.0f);
    DrawDebugSphere(GetWorld(), CachedFinalTarget, 30.0f, 12, FColor::Red, false, 5.0f, 0, 2.0f);

    if (UCharacterMovementComponent* MoveComp = OwnerChar->GetCharacterMovement())
    {
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
        if (UCharacterMovementComponent* MoveComp = OwnerChar->GetCharacterMovement())
        {
            MoveComp->BrakingDecelerationFalling = SavedBrakingDecelFalling;
            MoveComp->FallingLateralFriction = SavedFallingLateralFriction;
            MoveComp->bOrientRotationToMovement = bSavedOrientRotationToMovement;
            MoveComp->bUseControllerDesiredRotation = false;
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
    SteerAlpha = 0.f;

    if (APartyCharacter* OwnerChar = Cast<APartyCharacter>(GetOwner()))
    {
        if (AAIController* AICon = Cast<AAIController>(OwnerChar->GetController()))
        {
            AICon->StopMovement();
            AICon->ClearFocus(EAIFocusPriority::Gameplay);
        }

        if (UCharacterMovementComponent* MoveComp = OwnerChar->GetCharacterMovement())
        {
            MoveComp->BrakingDecelerationFalling = SavedBrakingDecelFalling;
            MoveComp->FallingLateralFriction = SavedFallingLateralFriction;
            MoveComp->bOrientRotationToMovement = bSavedOrientRotationToMovement;
            MoveComp->bUseControllerDesiredRotation = false;
        }
    }
}