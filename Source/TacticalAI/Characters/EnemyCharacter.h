// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Characters/TacticalCharacterBase.h"
#include "EnemyCharacter.generated.h"

// =========================================================================
// 적 캐릭터 베이스 (APartyCharacter 대칭, 단 빙의 안 됨 → 카메라/입력 없음).
// 진형에서 수동적 — 슬롯 좌표를 받아 이동만. 그룹 전략·배치 판단은 컴포넌트가 한다.
//
// 기질은 "개별 특성 레벨"의 정책이다. 진형(스폰풀이 정함)이 이 값을 덮어쓰지 않는다.
//
// APartyCharacter와의 의도된 비대칭: 적은 역할 태그(CombatRole)를 들지 않는다. 동료는
// 4인 고정이라 디자이너가 역할을 직접 지정하지만, 적의 전열/후열은 배치 시 교전거리에서
// 정렬로 산출되기 때문. 즉 적에겐 역할 축이 아예 없다.
// =========================================================================
// 敵キャラ基底（APartyCharacter対称、ただし憑依されない＝カメラ/入力なし）。
// 種類＝このBPクラス一つに外見と気質が一体。外見と気質を別々に組み合わせない。
// 気質は「個別特性レベル」のポリシー。隊形（スポーンプールが決める）はこれを上書きしない。
UCLASS()
class TACTICALAI_API AEnemyCharacter : public ATacticalCharacterBase
{
	GENERATED_BODY()

public:
	AEnemyCharacter();

	// 슬롯 생성 Context로 전달될 교전 사거리. APartyCharacter와 같은 이름·시그니처라
	// 공유 Strategy(_RangedSafe 등)가 적·아군 구분 없이 Context.AttackRange를 받는다.
	// (다형성 아님 — 각 컴포넌트가 자기 타입에서 값을 뽑아 Context에 넣을 뿐. 읽기 일관성용.)
	// プレイヤーと同一シグネチャ。共有Strategyが敵味方を区別せず射程を受け取る。
	float GetAttackRange() const { return AttackRange; }

	// 도주 발동 체력 비율 조회. 행동 레이어(StateTree)가 도주 판단에 사용(지금은 자리만).
	// 退却発動の体力割合。行動レイヤーが退却判断に使用。
	float GetFleeHealthRatio() const { return FleeHealthRatio; }

private:
	// ───── 개별 기질 (종류가 정하는 정책. 진형이 덮어쓰지 않음) ─────
	// 선호 교전 사거리. 배치 정렬의 유일한 기준 + 슬롯 생성 Context로 전달.
	// 작을수록 타겟 가까이(전열), 클수록 멀리(후열) 정렬 — 강제 아닌 기본값(ADR-0006).
	// 디자이너가 배치를 직접 뒤집을 수 있으므로(탱커 앞/근접 딜러 뒤), 이건 "합리적 기본"일 뿐.
	// 選好交戦距離。配置整列の唯一の基準。小さいほど前、大きいほど後（強制でなく既定値）。
	UPROPERTY(EditAnywhere, Category="Disposition", meta=(ClampMin="0.0"))
	float AttackRange = 200.f;

	// 도주 발동 체력 비율. HP가 이 비율 밑이면 이탈 시도 (행동 레이어에서 사용, 지금은 자리만).
	// 0이면 끝까지 싸우는 종류 — 절대 도망 안 침.
	// 退却する体力割合。これ未満で離脱を試みる。0なら最後まで戦う。前列配置でも不変。
	UPROPERTY(EditAnywhere, Category="Disposition", meta=(ClampMin="0.0", ClampMax="1.0"))
	float FleeHealthRatio = 0.f;
};