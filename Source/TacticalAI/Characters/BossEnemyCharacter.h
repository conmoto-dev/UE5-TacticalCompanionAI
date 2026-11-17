#pragma once

#include "CoreMinimal.h"

#include "Characters/EnemyCharacter.h"
#include "BossEnemyCharacter.generated.h"

UCLASS(Abstract, Blueprintable)
class TACTICALAI_API ABossEnemyCharacter : public AEnemyCharacter
{
	GENERATED_BODY()
};