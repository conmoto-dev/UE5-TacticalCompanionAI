#include "Characters/EnemyCharacter.h"
#include "Enemies/Encounter/EnemyEncounter.h"
#include "AI/Targeting/EnemyTargetSelectorComponent.h"

AEnemyCharacter::AEnemyCharacter()
{
	PrimaryActorTick.bCanEverTick = false;

	// 레벨에 배치되거나 런타임에 생성된 적에 AIController가 자동으로 빙의하도록 설정한다.
	// レベル配置または実行時生成された敵にAIControllerが自動で憑依するよう設定する。
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
	
	TargetSelector = CreateDefaultSubobject<UEnemyTargetSelectorComponent>(TEXT("TargetSelector"));
}

void AEnemyCharacter::SetEncounter(AEnemyEncounter* InEncounter)
{ OwningEncounter = InEncounter; }

AEnemyEncounter* AEnemyCharacter::GetEncounter() const
{ return OwningEncounter.Get(); }

UTargetSelectorComponent* AEnemyCharacter::GetTargetSelector() const 
{ return TargetSelector; }