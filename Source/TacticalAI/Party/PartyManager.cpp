// Fill out your copyright notice in the Description page of Project Settings.

#include "Party/PartyManager.h"
#include "Characters/PartyCharacter.h"
#include "AI/Components/FormationFollowComponent.h"
#include "AI/Components/FormationBattleComponent.h"
#include "Kismet/GameplayStatics.h"

APartyManager::APartyManager()
{
	// The manager itself doesn't need to tick -> Need to be tick for Detect Battle Status.
	PrimaryActorTick.bCanEverTick = true;
	
	FollowComponent = CreateDefaultSubobject<UFormationFollowComponent>(TEXT("FollowComp"));
	BattleComponent = CreateDefaultSubobject<UFormationBattleComponent>(TEXT("BattleComp"));
}

void APartyManager::BeginPlay()
{
	Super::BeginPlay();
	
	SetFormationMode(EPartyFormationMode::Follow);
	
	APartyCharacter* Leader = GetLeader();
	if (!Leader) return;
    
	APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
	if (PC)
	{
		// If the PlayerController already possesses a default pawn (auto-spawned by GameMode),
		// remove it before possessing the designated leader. Otherwise a stray pawn floats in the world.
		// GameModeが自動生成したデフォルトPawnが残っていると幽霊キャラが浮遊するため、ここで除去する。
		APawn* OldPawn = PC->GetPawn();
        
		PC->Possess(Leader);
        
		if (OldPawn && OldPawn != Leader)
		{
			OldPawn->Destroy();
		}
	}
}

void APartyManager::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	TickModeDecision();
}

APartyCharacter* APartyManager::GetLeader() const
{
	if (!Members.IsValidIndex(CurrentLeaderIndex)) return nullptr;
	return Members[CurrentLeaderIndex];
}

TArray<APartyCharacter*> APartyManager::GetFollowers() const
{
	TArray<APartyCharacter*> Result;
	for (int32 i = 0; i < Members.Num(); ++i)
	{
		if (i == CurrentLeaderIndex) continue;
		if (Members[i]) Result.Add(Members[i]);
	}
	return Result;
}

void APartyManager::SwapLeader(int32 NewLeaderIndex)
{
	if (NewLeaderIndex == CurrentLeaderIndex) return;
	if (!Members.IsValidIndex(NewLeaderIndex)) return;

	// Actual controller swap logic comes in a later step.
	// For now we just update the index.
	CurrentLeaderIndex = NewLeaderIndex;
}

//** Formation Change Algorithm *//
void APartyManager::SetFormationMode(EPartyFormationMode NewMode)
{
	const bool bFollow = (NewMode == EPartyFormationMode::Follow);

	// 두 컴포넌트 상태를 매번 명시적으로 세팅 (멱등 — 같은 모드 재호출 무해).
	// SetActive + Tick 둘 다 꺼야 비활성 컴포넌트가 push 안 함 (토글 함정 방지).
	FollowComponent->SetActive(bFollow);
	FollowComponent->SetComponentTickEnabled(bFollow);

	BattleComponent->SetActive(!bFollow);
	BattleComponent->SetComponentTickEnabled(!bFollow);
}

void APartyManager::TickModeDecision()
{
	const APartyCharacter* Leader = GetLeader();
	const AActor* Target = DebugBattleTarget;
	if (!Leader || !Target) return; // 타겟 없으면 판단 안 함 (현 모드 유지)

	const float DistSq = FVector::DistSquared(Leader->GetActorLocation(), Target->GetActorLocation());

	// 히스테리시스: 진입은 가까이, 이탈은 멀리. 사이 구간은 현상유지(깜빡임 방지).
	if (DistSq < FMath::Square(EnterBattleDistance))
	{
		SetFormationMode(EPartyFormationMode::Battle);
	}
	else if (DistSq > FMath::Square(ExitBattleDistance))
	{
		SetFormationMode(EPartyFormationMode::Follow);
	}
	// 사이 구간: 아무것도 안 함 → 현재 모드 유지.
}