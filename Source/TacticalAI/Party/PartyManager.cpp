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

void APartyManager::SwapLeader(int32 NewLeaderIndex)
{
	if (NewLeaderIndex == CurrentLeaderIndex) return;
	if (!Members.IsValidIndex(NewLeaderIndex)) return;

	// Actual controller swap logic comes in a later step.
	// For now we just update the index.
	CurrentLeaderIndex = NewLeaderIndex;
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

TArray<AActor*> APartyManager::GetPerceivedEnemies() const
{
	// IsValid 필터 — 에디터 지정 적이 런타임에 파괴될 수 있음 (처치 등).
	// 호출자는 항상 살아있는 적만 받는다는 계약.
	TArray<AActor*> Result;
	Result.Reserve(DebugPerceivedEnemies.Num());
	for (AActor* Enemy : DebugPerceivedEnemies)
	{
		if (IsValid(Enemy))
		{
			Result.Add(Enemy);
		}
	}
	return Result;
}


//** Formation Change Algorithm *//
void APartyManager::SetFormationMode(EPartyFormationMode NewMode)
{
	if (CurrentFormationMode == NewMode) return;   // 전환될 때만.
	CurrentFormationMode = NewMode;
	const bool bFollow = (NewMode == EPartyFormationMode::Follow);
	
	// SetActive + Tick 둘 다 꺼야 비활성 컴포넌트가 push 안 함 (토글 함정 방지).
	FollowComponent->SetActive(bFollow);
	FollowComponent->SetComponentTickEnabled(bFollow);

	BattleComponent->SetActive(!bFollow);
	BattleComponent->SetComponentTickEnabled(!bFollow);
}

void APartyManager::TickModeDecision()
{
	const APartyCharacter* Leader = GetLeader();
	if (!Leader) return;

	// [1] 최근접 적과의 거리 산출. 적이 하나도 없으면 판단 안 함 (현 모드 유지).
	//     모드 판단 기준은 리더(=플레이어) — 동료가 아니라 플레이어의 교전 상태가 파티 모드를 결정.
	// 判断基準はリーダー（＝プレイヤー）。プレイヤーの交戦状態がパーティのモードを決める。
	float NearestDistSq = TNumericLimits<float>::Max();
	bool bAnyEnemy = false;
	for (const AActor* Enemy : GetPerceivedEnemies())
	{
		NearestDistSq = FMath::Min(NearestDistSq,
			FVector::DistSquared(Leader->GetActorLocation(), Enemy->GetActorLocation()));
		bAnyEnemy = true;
	}
	if (!bAnyEnemy) return;

	// [2] 히스테리시스: 진입은 가까이, 이탈은 멀리. 사이 구간은 현상유지(깜빡임 방지).
	if (NearestDistSq < FMath::Square(EnterBattleDistance))
	{
		SetFormationMode(EPartyFormationMode::Battle);
	}
	else if (NearestDistSq > FMath::Square(ExitBattleDistance))
	{
		SetFormationMode(EPartyFormationMode::Follow);
	}
	// 사이 구간: 아무것도 안 함 → 현재 모드 유지.
}