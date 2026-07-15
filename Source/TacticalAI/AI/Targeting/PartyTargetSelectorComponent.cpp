#include "AI/Targeting/PartyTargetSelectorComponent.h"
#include "Characters/PartyCharacter.h"
#include "Party/PartyManager.h"

FTargetingContext UPartyTargetSelectorComponent::BuildContext() const
{
	FTargetingContext Context;
	Context.SelfLocation = GetOwner()->GetActorLocation();

	const APartyCharacter* OwnerCharacter = Cast<APartyCharacter>(GetOwner());
	const APartyManager* Manager = OwnerCharacter ? OwnerCharacter->GetPartyManager() : nullptr;
	if (const APartyCharacter* Leader = Manager ? Manager->GetLeader() : nullptr)
	{
		Context.LeaderLocation = Leader->GetActorLocation();
		Context.bHasLeader = true;
	}
	return Context;
}

TArray<AActor*> UPartyTargetSelectorComponent::GatherCandidates() const
{
	const APartyCharacter* OwnerCharacter = Cast<APartyCharacter>(GetOwner());
	const APartyManager* Manager = OwnerCharacter ? OwnerCharacter->GetPartyManager() : nullptr;
	return Manager ? Manager->GetPerceivedEnemies() : TArray<AActor*>();
}