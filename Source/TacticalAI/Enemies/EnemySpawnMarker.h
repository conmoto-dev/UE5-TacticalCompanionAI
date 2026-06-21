// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "EnemySpawnMarker.generated.h"

class AEnemyCharacter;
class AEnemyGroup;

// =========================================================================
// 스폰 구성 한 줄 — "이 종류를 몇 마리". 마커가 직접 든다.
// 종류 = CharacterClass(BP) 하나에 외형+기질이 한 몸.
// =========================================================================
// スポーン構成1行 — 「この種類を何体」。マーカーが直接保有。
USTRUCT(BlueprintType)
struct FSpawnEntry
{
	GENERATED_BODY()

	// 스폰할 적 종류 (BP — 외형+기질 한 몸).
	// スポーンする敵種類（BP＝外見+気質一体）。
	UPROPERTY(EditAnywhere, Category="Spawn")
	TSubclassOf<AEnemyCharacter> CharacterClass;

	// 이 종류를 몇 마리.
	// この種類を何体。
	UPROPERTY(EditAnywhere, Category="Spawn", meta=(ClampMin="1"))
	int32 Count = 1;
};

// =========================================================================
// 적 스폰 마커 — 레벨에 수동 배치하는 정적 마커. 스폰하고 빠진다(멤버 소유는 그룹이).
// 마커 위치에 그룹 1개를 스폰하고, 그룹이 멤버를 그 주변에 펼친다(2단계 스폰).
//
// 스폰 트리거(BeginPlay)와 스폰 로직(SpawnGroup)을 분리한 게 핵심 — 지금은 BeginPlay가
// 무조건 호출하지만, 나중에 거리 최적화 Subsystem(ADR-0005)이 "근처일 때만 SpawnGroup()"
// 으로 트리거만 갈아끼우면 된다. 마커 아래(그룹·진형)는 무변경. (PartyManager의
// GetPerceivedEnemies를 "내부 구현만 교체, API 유지"로 둔 그 패턴과 동일.)
// =========================================================================
// 敵スポーンマーカー — レベルに手動配置する静的マーカー。スポーンして退く（所有はグループ）。
// トリガー(BeginPlay)とロジック(SpawnGroup)を分離 — 後で距離最適化がトリガーだけ差し替える。
UCLASS()
class TACTICALAI_API AEnemySpawnMarker : public AActor
{
	GENERATED_BODY()

public:
	AEnemySpawnMarker();

protected:
	virtual void BeginPlay() override;

	// 스폰 로직 본체. 트리거와 분리 — 나중에 Subsystem이 거리 보고 이 함수를 호출.
	// Entries를 Count만큼 평탄화 → 그룹 1개 스폰 → 그룹에 멤버 스폰 위임.
	// スポーンロジック本体。トリガーと分離。後でSubsystemが距離を見て呼ぶ。
	void SpawnGroup();

	// 무엇을 몇 마리 (마커에 직접).
	// 何を何体（マーカーに直接）。再利用が要るならDataAsset化（今は不要）。
	UPROPERTY(EditAnywhere, Category="Spawn")
	TArray<FSpawnEntry> Entries;

	// 스폰할 그룹 클래스. 진형 컴포넌트를 단 BP 파생을 지정 가능(조각4). 비우면 AEnemyGroup 기본.
	// スポーンするグループクラス。隊形コンポーネント付きのBP派生を指定可（配置段階）。
	UPROPERTY(EditAnywhere, Category="Spawn")
	TSubclassOf<AEnemyGroup> GroupClass;
};