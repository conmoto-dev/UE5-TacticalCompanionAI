// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PartyManager.generated.h"

class APartyCharacter;
class UFormationFollowComponent;

UCLASS()
class TACTICALAI_API APartyManager : public AActor
{
	GENERATED_BODY()
	
public:	
	APartyManager();

	/** Returns the current leader character. */
	APartyCharacter* GetLeader() const;

	/** Returns all members except the current leader. */
	TArray<APartyCharacter*> GetFollowers() const;

	/** Swaps the current leader to a new index. */
	UFUNCTION(BlueprintCallable, Category="Party")
	void SwapLeader(int32 NewLeaderIndex);

protected:
	virtual void BeginPlay() override;

	/** All party members. Manually assigned in editor. */
	// パーティ全員。エディタで手動割り当て。
	UPROPERTY(EditAnywhere, Category="Party")
	TArray<TObjectPtr<APartyCharacter>> Members;

	/** Index into Members that points to the current leader. */
	UPROPERTY(EditAnywhere, Category="Party")
	int32 CurrentLeaderIndex = 0;
	
	/** Formation system component. Performs slot calculation and pushes targets to followers. */
	UPROPERTY(VisibleAnywhere, Category="Formation")
	TObjectPtr<UFormationFollowComponent> FormationComponent;
};