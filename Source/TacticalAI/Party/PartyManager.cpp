// Fill out your copyright notice in the Description page of Project Settings.

#include "Party/PartyManager.h"
#include "Characters/PartyCharacter.h"
#include "AI/Components/FormationFollowComponent.h"
#include "Kismet/GameplayStatics.h"

APartyManager::APartyManager()
{
	PrimaryActorTick.bCanEverTick = false; // The manager itself doesn't need to tick.
	FormationComponent = CreateDefaultSubobject<UFormationFollowComponent>(TEXT("FormationComp"));
}

void APartyManager::BeginPlay()
{
	Super::BeginPlay();
	
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