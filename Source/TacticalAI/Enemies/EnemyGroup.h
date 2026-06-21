// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "EnemyGroup.generated.h"

class AEnemyCharacter;

// =========================================================================
// 적 그룹 — 스폰된 멤버를 소유하는 런타임 단위 (APartyManager의 적 버전 대칭).
// "그룹"이 멤버를 소유해야 진형 컴포넌트가 이 그룹에 붙을 수 있다(조각 4). 마커가
// 멤버를 직접 스폰하지 않고 그룹을 거치는 이유 — 누가 한 진형인지 묶을 주체가 필요(ADR-0006).
//
// 그룹 경계는 스폰 데이터가 만든다(결정1). 지금은 마커 1개 = 그룹 1개의 단순 구조이며,
// 합성(A+B→C)이 들어오면 마커 1개가 그룹 여러 개를 낳는 형태로 확장될 자리.
//
// 위치 자체는 의미가 약하다 — 그룹은 논리적 컨테이너고, 진형 anchor는 플레이어(외부)다.
// 마커 위치에 스폰되어 멤버를 그 주변에 둔다.
// =========================================================================
// 敵グループ — スポーンされたメンバーを所有する実行時の単位（APartyManager対称）。
// 「グループ」がメンバーを所有することで隊形コンポーネントが付けられる（配置段階）。
UCLASS()
class TACTICALAI_API AEnemyGroup : public AActor
{
	GENERATED_BODY()

public:
	AEnemyGroup();

	// 멤버 스폰 + 보유. 마커가 스폰 직후 1회 호출한다.
	// SpawnClasses[i]를 RingRadius 둘레에 등각으로 흩어 스폰(겹침 방지용 임시 배치 —
	// 진형이 들어오면 슬롯으로 재배치되므로 정교할 필요 없다).
	// メンバーをスポーンして保有。マーカーがスポーン直後に1回呼ぶ。
	void InitializeGroup(const TArray<TSubclassOf<AEnemyCharacter>>& SpawnClasses);

	// 이 그룹의 멤버 조회 (진형 컴포넌트가 사용 예정).
	const TArray<TObjectPtr<AEnemyCharacter>>& GetMembers() const { return Members; }

protected:
	// 겹침 방지용 임시 스폰 반경. 멤버를 이 반경 원 둘레에 등각으로 깐다.
	// 진형이 슬롯으로 덮어쓰므로 "초기 겹침만 피하는" 값.
	// 重なり防止用の仮スポーン半径。隊形が上書きするため初期重なり回避のみ。
	UPROPERTY(EditAnywhere, Category="Spawn", meta=(ClampMin="0.0"))
	float SpawnRingRadius = 150.f;

private:
	// 이 그룹이 소유한 멤버. 강참조 — 지금은 사망/전투가 없어 dangling이 안 생긴다.
	// (사망이 들어오면 약참조 전환 또는 사망 시 배열 정리 필요 — 그때 재검토.)
	// このグループが所有するメンバー。今は死亡がないので強参照で十分。
	UPROPERTY()
	TArray<TObjectPtr<AEnemyCharacter>> Members;
};