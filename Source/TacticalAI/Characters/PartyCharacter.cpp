// Fill out your copyright notice in the Description page of Project Settings.

#include "Characters/PartyCharacter.h"
#include "Controllers/CompanionAIController.h"
#include "AIController.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"

APartyCharacter::APartyCharacter()
{
	PrimaryActorTick.bCanEverTick = false;
	
	AIControllerClass = ACompanionAIController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
	
	// RVO Avoidance: enables smooth avoidance between companions.
	// 仲間同士のスムーズな回避のためRVOを有効化。
	GetCharacterMovement()->bUseRVOAvoidance = true;
	GetCharacterMovement()->AvoidanceConsiderationRadius = 200.f;
	
	
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	GetMesh()->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
}

void APartyCharacter::BeginPlay()
{
	Super::BeginPlay();
	CurrentTargetLocation = GetActorLocation();
}

void APartyCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

void APartyCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
}

void APartyCharacter::UpdateTargetSlotLocation(const FVector& NewTarget)
{
	AAIController* AIC = Cast<AAIController>(GetController());
	if (!AIC) return;

	// Filter out tiny target changes. Calling MoveTo every frame is expensive
	// (full path replanning), so we only re-issue the command past the threshold.
	// 毎フレームMoveToを呼ぶとパス再計算で高コストなので、しきい値を超えた時のみ再発行。
	const float DistSq = FVector::DistSquared(NewTarget, CurrentTargetLocation);
	if (DistSq < FMath::Square(UpdateThreshold)) return;
	
	CurrentTargetLocation = NewTarget;
	AIC->MoveToLocation(NewTarget, AcceptanceRadius);
}