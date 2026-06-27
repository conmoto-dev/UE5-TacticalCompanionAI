#pragma once

#include "CoreMinimal.h"
#include "Characters/EnemyCharacter.h"
#include "EnemyEncounterGroupResolverTestTypes.generated.h"

// =========================================================================
// Encounter Resolver 테스트용 적 A.
//
// EnemyClass별 기본 그룹 분리를 검증하기 위한 테스트 전용 타입이다.
// 런타임 게임플레이 로직에서는 사용하지 않는다.
//
// Encounter Resolverテスト用の敵A。
//
// EnemyClass別のデフォルトグループ分割を検証するための
// テスト専用型。ランタイムゲームプレイでは使用しない。
// =========================================================================
UCLASS(NotBlueprintable)
class TACTICALAI_API AEnemyEncounterResolverTestEnemyA
	: public AEnemyCharacter
{
	GENERATED_BODY()
};

// =========================================================================
// Encounter Resolver 테스트용 적 B.
//
// EnemyClass별 기본 그룹 분리를 검증하기 위한 테스트 전용 타입이다.
// 런타임 게임플레이 로직에서는 사용하지 않는다.
//
// Encounter Resolverテスト用の敵B。
//
// EnemyClass別のデフォルトグループ分割を検証するための
// テスト専用型。ランタイムゲームプレイでは使用しない。
// =========================================================================
UCLASS(NotBlueprintable)
class TACTICALAI_API AEnemyEncounterResolverTestEnemyB
	: public AEnemyCharacter
{
	GENERATED_BODY()
};