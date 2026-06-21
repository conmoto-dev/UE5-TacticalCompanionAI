// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Characters/TacticalCharacterBase.h"
#include "EnemyCharacter.generated.h"

class UEnemyArchetypeDataAsset;

// =========================================================================
// 적 캐릭터 베이스 (APartyCharacter 대칭, 단 빙의 안 됨 → 카메라/입력 없음).
// 진형에서 수동적 — 슬롯 좌표를 받아 이동만. 그룹 전략·배치 판단은 컴포넌트가 한다.
// "종류" 정체성(기질)은 직접 들지 않고 Archetype Asset을 참조 — 같은 종류가 공유(Flyweight).
//
// APartyCharacter와의 의도된 비대칭: 적은 역할 태그(CombatRole)를 들지 않는다. 동료는
// 4인 고정이라 디자이너가 역할을 직접 지정하지만, 적의 전열/후열은 지정이 아니라 배치 시
// 교전거리(AttackRange)에서 정렬로 산출되기 때문(ADR-0006). 즉 적에겐 역할 축이 아예 없다.
// =========================================================================
// 敵キャラ基底（APartyCharacter対称、ただし憑依されない＝カメラ/入力なし）。
// 種類の正体性(気質)はArchetype Assetを参照＝同種が共有。
// 意図的な非対称：敵は役割タグを持たない。前列/後列は配置時に射程から整列で算出するため。
UCLASS()
class TACTICALAI_API AEnemyCharacter : public ATacticalCharacterBase
{
	GENERATED_BODY()

public:
	AEnemyCharacter();

	// 이 개체의 종류 정의 조회. 배치 정렬이 여기서 교전거리를 읽는다.
	// この個体の種類定義を返す。配置整列がここから射程を読む。
	const UEnemyArchetypeDataAsset* GetArchetype() const { return Archetype; }

	// 슬롯 생성 Context로 전달될 교전 사거리. APartyCharacter와 같은 이름·시그니처라
	// 공유 Strategy(_RangedSafe 등)가 적·아군 구분 없이 Context.AttackRange를 받는다.
	// (다형성 아님 — 각 컴포넌트가 자기 타입에서 값을 뽑아 Context에 넣을 뿐. 읽기 일관성용.)
	// プレイヤーと同一シグネチャ。共有Strategyが敵味方を区別せず射程を受け取る。
	float GetAttackRange() const;

private:
	// 이 적의 종류 정의 (기질 묶음). 스폰 시 마커가 종류별로 지정한다.
	// 같은 종류 여러 마리가 같은 Asset을 가리킴 — 개별 복제 아님(Flyweight).
	// この敵の種類定義。スポーン時にマーカーが種類ごとに割り当てる。
	UPROPERTY(EditAnywhere, Category="Enemy")
	TObjectPtr<UEnemyArchetypeDataAsset> Archetype;

	// Archetype 미지정 시 폴백 사거리. 정상 운용에선 Archetype이 항상 있어야 하나,
	// BP에 미할당 채로 PIE를 돌렸을 때 배치가 0으로 깨지지 않게 하는 안전망(튜닝값 아님).
	// Archetype未割当時のフォールバック射程。配置が0で壊れないための安全網。
	static constexpr float FallbackAttackRange = 200.f;
};