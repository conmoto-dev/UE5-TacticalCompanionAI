// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "EnemyArchetypeDataAsset.generated.h"

// =========================================================================
// 적 "종류"의 기질 묶음 (디자이너 편집, 종류당 1 Asset).
// 같은 종류 N마리가 이 Asset 1개를 공유 (Flyweight) — 개체마다 복제하지 않는다.
// 식별 두 갈래: 이 Asset 레퍼런스 자체가 "정확히 이 종류"의 키, ArchetypeTag가 "분류"의
// 키. 합성(A+B→C)이 전자로 단일 종류를, 후자로 "근접형 전체" 같은 범주를 표현한다.
// (UYieldStrategy·UFormationDataAsset과 같은 데이터-정책 분리 철학.)
// =========================================================================
// 敵「種類」の気質データ。種類ごとに1つ、同種はこれを共有（Flyweight）。
// 識別：Asset参照自体が「正確な種類」のキー、ArchetypeTagが「分類」のキー。
UCLASS(BlueprintType)
class TACTICALAI_API UEnemyArchetypeDataAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	// 이 종류의 분류 태그. 합성 규칙이 "범주"로 묶을 때의 키 (예: Enemy.Type.Melee).
	// 변종(같은 외형 다른 기질)을 한 규칙에 묶고 싶을 때 사용. 단일 종류 규칙엔 불필요.
	// 배치는 이 태그를 보지 않는다 — 오직 합성 규칙만 읽는다.
	// 種類の分類タグ。合成規則が「カテゴリ」で束ねる際のキー。配置はこれを見ない。
	UPROPERTY(EditAnywhere, Category="Identity", meta=(Categories="Enemy.Type"))
	FGameplayTag ArchetypeTag;

	// 선호 교전 사거리. 배치 정렬의 유일한 기준 + 슬롯 생성 Context로 전달.
	// 작을수록 타겟 가까이(전열), 클수록 멀리(후열) 정렬 — 강제 아닌 기본값(ADR-0006 결정3).
	// 디자이너가 배치를 직접 뒤집을 수 있으므로(탱커 앞/근접 딜러 뒤 같은 연출), 이건
	// "합리적 기본"을 깔아줄 뿐이다.
	// 選好交戦距離。配置整列の唯一の基準。小さいほど前、大きいほど後（強制でなく既定値）。
	UPROPERTY(EditAnywhere, Category="Disposition", meta=(ClampMin="0.0"))
	float AttackRange = 200.f;

	// 도주 발동 체력 비율. HP가 이 비율 밑이면 이탈 시도 (행동 레이어에서 사용, 지금은 자리만).
	// 0이면 끝까지 싸우는 종류 — 절대 도망 안 침.
	// 退却する体力割合。これ未満で離脱を試みる。0なら最後まで戦う。
	UPROPERTY(EditAnywhere, Category="Disposition", meta=(ClampMin="0.0", ClampMax="1.0"))
	float FleeHealthRatio = 0.f;
};