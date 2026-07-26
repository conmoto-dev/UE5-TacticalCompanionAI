#include "Characters/EnemyCharacter.h"
#include "Enemies/Group/EnemyGroup.h"
#include "AI/Targeting/EnemyTargetSelectorComponent.h"

AEnemyCharacter::AEnemyCharacter()
{
	PrimaryActorTick.bCanEverTick = false;

	// 스폰 후 AI가 빙의하도록 — 슬롯 좌표로 이동(MoveToLocation)하려면 컨트롤러가 필요.
	// 적 전용 AIController·회피(crowd) 연결은 그룹 배치 단계에서. 지금은 기본 컨트롤러로 충분.
	// スポーン後にAIが憑依。スロット移動にコントローラーが要る。敵専用化は配置段階で。
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
	
	TargetSelector = CreateDefaultSubobject<UEnemyTargetSelectorComponent>(TEXT("TargetSelector"));
}

void AEnemyCharacter::SetEnemyGroup(AEnemyGroup* InGroup)
{ OwningGroup = InGroup; }

AEnemyGroup* AEnemyCharacter::GetEnemyGroup() const
{ return OwningGroup.Get(); }

UTargetSelectorComponent* AEnemyCharacter::GetTargetSelector() const 
{ return TargetSelector; }