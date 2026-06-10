// Fill out your copyright notice in the Description page of Project Settings.

#include "Characters/PartyCharacter.h"
#include "Controllers/CompanionAIController.h"
#include "AIController.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "AI/Components/PlayerCrowdAgentComponent.h"
#include "AI/CombatRoleTags.h"

APartyCharacter::APartyCharacter()
{
	PrimaryActorTick.bCanEverTick = false;
	
	AIControllerClass = ACompanionAIController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
	
	GetCharacterMovement()->bUseRVOAvoidance = false;
	
	// Camera passes through capsule + mesh (no obstruction on follower characters).
	// カメラがカプセル・メッシュを貫通（仲間越しのカメラブロック防止）。
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	GetMesh()->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	
	PlayerAgentComp = CreateDefaultSubobject<UPlayerCrowdAgentComponent>(TEXT("PlayerAgentComp"));
	
	// 역할 기본값. 헤더 초기화 불가(Native Tag는 모듈 로드 후 유효)라 생성자에서 대입.
	CombatRole = CombatRoleTags::Melee;
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

void APartyCharacter::InvalidateMoveCache()
{
	// 캐시를 도달 불가능한 값으로 → 다음 명령이 반드시 threshold 초과 → 강제 재발행.
	CurrentTargetLocation = FVector(FLT_MAX);
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