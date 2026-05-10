// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "TacticalAICharacter.h"
#include "PartyCharacter.generated.h"

UCLASS()
class TACTICALAI_API APartyCharacter : public ATacticalAICharacter
{
	GENERATED_BODY()

public:
	APartyCharacter();

	UFUNCTION(BlueprintCallable, Category="Formation")
	void UpdateTargetSlotLocation(const FVector& NewTarget);
	
protected:
	virtual void BeginPlay() override;

public:	
	virtual void Tick(float DeltaTime) override;

	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;
	
private:
	UPROPERTY(VisibleAnywhere, Category="Formation")
	FVector CurrentTargetLocation;

	UPROPERTY(EditAnywhere, Category="Formation")
	float UpdateThreshold = 50.f;

	UPROPERTY(EditAnywhere, Category="Formation")
	float AcceptanceRadius = 30.f;
};

