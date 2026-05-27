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
	
	// RVO를 끄고, Detour Crowd 기반에서 두 캐릭터가 같은 목표점에 도달했을 때
	// 서로 비비적대지 않고 여유롭게 멈추도록 도달 반경(AcceptanceRadius)을 넉넉하게 늘립니다.
	GetCharacterMovement()->bUseRVOAvoidance = false;
	AcceptanceRadius = 80.f; // (캡슐 반지름 40.f의 2배 수준으로 확장)
	
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