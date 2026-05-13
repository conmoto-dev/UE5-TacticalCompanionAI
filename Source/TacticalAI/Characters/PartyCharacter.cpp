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
	
	// RVO Avoidance for smooth companion-vs-companion avoidance.
	// May be replaced with Detour Crowd integration (week 2 evaluation).
	// 仲間同士のスムーズな回避用。Detour Crowd検証時に置換予定。
	GetCharacterMovement()->bUseRVOAvoidance = true;
	GetCharacterMovement()->AvoidanceConsiderationRadius = 200.f;
	
	// Camera passes through capsule + mesh (no obstruction on follower characters).
	// カメラがカプセル・メッシュを貫通（仲間越しのカメラブロック防止）。
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

void APartyCharacter::UpdateTargetSlotLocation(const FVector& NewTarget, bool bForceRefresh)
{
	AAIController* AIC = Cast<AAIController>(GetController());
	if (!AIC) return;

	// Skip re-issue if target hasn't moved enough (cheap MoveTo deduplication).
	// Force path is used during Yielding to guarantee MoveTo refresh on every tick.
	// 目標がほぼ動いていなければ再発行スキップ。
	// Yielding中はforceでキャッシュバイパスし毎Tick確実にMoveTo更新。
	if (!bForceRefresh)
	{
		const float DistSq = FVector::DistSquared(NewTarget, CurrentTargetLocation);
		if (DistSq < FMath::Square(UpdateThreshold)) return;
	}
    
	CurrentTargetLocation = NewTarget;
	AIC->MoveToLocation(NewTarget, AcceptanceRadius);
}