#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "SlotGeneratorStrategy.generated.h"

class AActor;

// =========================================================================
// 슬롯 생성 입력 컨텍스트 (계약 개정의 핵심).
// 구(舊)계약은 "Strategy는 월드를 모른다"였으나, 원거리처럼 월드 공간 평가가
// 본질인 Strategy가 생기며 공식 폐기. 입력을 struct로 묶어 멤버 추가가
// 시그니처 변경(=전체 구현 파손) 없이 가능하게 한다.
// 한 틱 안에서 소비되는 스택 임시 객체 — 리플렉션·GC 추적 불필요(평범한 struct).
// =========================================================================
// スロット生成の入力コンテキスト。旧契約「Strategyはワールドを知らない」を公式に改定。
// メンバー追加がシグネチャ変更なしで可能（既存実装を壊さない）。
struct FSlotGenContext
{
	// 배치 인원.
	int32 NumSlots = 0;

	// 호출부가 산출한 최종 반경 (기본값 + 타겟 크기 보정 + 역할별 오프셋).
	// 해석은 Strategy 소관 — 호의 반지름일 수도, 선호 교전 거리일 수도, 무시할 수도.
	// 解釈はStrategy側の裁量（アーク半径／好戦距離／無視も可）。
	float BaseRadius = 0.f;

	// 기준 프레임. Arc류는 이걸로 로컬→월드 변환, 공간 평가류는 참고만.
	FTransform Anchor;

	// 현재 교전 타겟. / 現在の交戦ターゲット。
	TWeakObjectPtr<const AActor> PrimaryTarget;

	// 인지한 적 전체 (타겟 포함). 원거리 안전 위치 평가의 핵심 입력.
	// weak — Strategy가 BP 파생 가능하므로 계약 차원에서 수명 실수를 차단.
	// 知覚した敵全体（ターゲット含む）。遠距離の安全位置評価の中核入力。
	TArray<TWeakObjectPtr<const AActor>> PerceivedEnemies;

	// 명시 전달 — Instanced UObject는 Outer 체인으로 GetWorld() 불가.
	// NavMesh 투영·가시성 체크 등에 사용. 스택 수명이라 raw로 충분.
	// Instanced UObjectはOuter経由でGetWorld()不可のため明示的に渡す。
	UWorld* World = nullptr;
};

/**
 * 추상 슬롯 생성 전략. 컨텍스트를 받아 슬롯 위치를 "월드 좌표"로 생성한다.
 * 환경보정(NavMesh·벽·슬로프)은 여전히 호출부(공통 파이프라인) 소관 —
 * Strategy는 이상 위치만 낸다. 입력은 전부 Context로 받으므로 stateless 유지.
 * Yield Strategy와 동일 패턴: stateless, DataAsset Flyweight 공유 안전.
 *
 * 抽象スロット生成戦略。スロットを「ワールド座標」で生成する。
 * 環境補正は呼び出し側の責務 — Strategyは理想位置のみ。statelessを維持。
 */
UCLASS(Abstract, EditInlineNew, Blueprintable, BlueprintType)
class TACTICALAI_API USlotGeneratorStrategy : public UObject
{
	GENERATED_BODY()

public:
	/**
	 * 슬롯 위치 N개를 월드 좌표로 생성.
	 * @param Context           생성에 필요한 전체 입력 (인원·반경·anchor·적 정보·월드)
	 * @param OutSlotLocations  [out] 월드 좌표 결과. 환경보정 전의 이상 위치.
	 */
	virtual void GenerateSlots(const FSlotGenContext& Context, TArray<FVector>& OutSlotLocations) const
		PURE_VIRTUAL(USlotGeneratorStrategy::GenerateSlots, );
};