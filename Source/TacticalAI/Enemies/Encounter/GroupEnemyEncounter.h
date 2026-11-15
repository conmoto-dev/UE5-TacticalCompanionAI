#pragma once

#include "CoreMinimal.h"
#include "Enemies/Encounter/EnemyEncounterBase.h"
#include "GroupEnemyEncounter.generated.h"

class AEnemyCharacter;

// =========================================================================
// Group Enemy Encounter
//
// 런타임에 스폰된 적들의 교전 단위.
// 그룹 경계는 스폰 데이터가 정하고(ADR-0006), 생성 주체가 스폰 직후 생성·등록한다.
// 생성 주체는 스포너에 한정하지 않는다 (보스 소환 패턴 등도 같은 경로를 사용).
//
// 교전 상태·알려진 적·전멸 통지는 베이스가 담당하며,
// 이 클래스는 외부 생성 주체의 등록 경로를 연다.
// ランタイムにスポーンされた敵の交戦単位。
// グループ境界はスポーンデータが決め、生成主体がスポーン直後に生成・登録する。
// 生成主体はSpawnerに限定しない（ボスの召喚パターンなども同じ経路を使う）。
//
// 交戦状態・既知敵・全滅通知は基底が担当し、
// このクラスは外部の生成主体からの登録経路を開く。
// =========================================================================
UCLASS(Blueprintable)
class TACTICALAI_API AGroupEnemyEncounter : public AEnemyEncounterBase
{
	GENERATED_BODY()

public:
	// =========================================================================
	// Membership
	// =========================================================================
	void RegisterMember(AEnemyCharacter* Member);
};