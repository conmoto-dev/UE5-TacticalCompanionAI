#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "SlotGeneratorStrategy.generated.h"

class AActor;

// =========================================================================
// 슬롯 생성 입력 컨텍스트.
// 계약은 "1인 → 1슬롯". 멤버 순회는 호출자(컴포넌트)가 돌고, Strategy는 1건만 답한다.
// 집합형(Arc)은 TotalSlots·SlotIndex로 자기 위치를 계산하고,
// 개별형(RangedSafe)은 그 둘을 무시하고 AttackRange·PerceivedEnemies로 평가한다.
// 한 틱 안에서 소비되는 스택 임시 객체 — 리플렉션·GC 추적 불필요(평범한 struct).
// =========================================================================
// スロット生成の入力コンテキスト。契約は「1人→1スロット」。巡回は呼び出し側。
// 集合型(Arc)はTotalSlots・SlotIndexで、個別型(RangedSafe)はAttackRange・敵分布で算出。
struct FSlotGenContext
{
	// ----- 집합형 입력 (Arc류 사용 / 개별형은 무시) -----
	// 集合型の入力（Arcが使用、個別型は無視）

	// 이 그룹의 전체 인원 N. 부채꼴 등분의 분모.
	int32 TotalSlots = 0;

	// 이 멤버가 전체 N명 중 몇 번째인가 (0-based). 부채꼴 등분의 인덱스.
	int32 SlotIndex = 0;

	// ----- 공통 입력 -----

	// 호출부가 산출한 최종 반경 (기본값 + 타겟 크기 보정 + 역할별 오프셋).
	// 해석은 Strategy 소관 — 호의 반지름일 수도, 선호 교전 거리일 수도, 무시할 수도.
	// 解釈はStrategy側の裁量。
	float BaseRadius = 0.f;

	// 기준 프레임. Arc류는 이걸로 로컬→월드 변환, 공간 평가류는 참고만.
	FTransform Anchor;

	// 이 멤버 1명의 기준 교전 사거리. 모든 포메이션이 참조할 수 있는 범용 파라미터.
	// このメンバー1人の基準射程。全フォーメーションが参照しうる汎用パラメータ。
	float AttackRange = 0.f;

	// ----- 개별형 입력 (RangedSafe류 사용 / Arc는 무시) -----
	// 個別型の入力（RangedSafeが使用、Arcは無視）

	// 현재 교전 타겟. / 現在の交戦ターゲット。
	TWeakObjectPtr<const AActor> PrimaryTarget;

	// 인지한 적 전체 (타겟 포함). 원거리 안전 위치 평가의 핵심 입력.
	// 知覚した敵全体（ターゲット含む）。遠距離の安全位置評価の中核入力。
	TArray<TWeakObjectPtr<const AActor>> PerceivedEnemies;

	// 명시 전달 — Instanced UObject는 Outer 체인으로 GetWorld() 불가.
	// Instanced UObjectはOuter経由でGetWorld()不可のため明示的に渡す。
	UWorld* World = nullptr;
};

/**
 * 추상 슬롯 생성 전략. 컨텍스트 1건을 받아 슬롯 1개를 "월드 좌표"로 생성한다.
 * 멤버 순회는 호출자(컴포넌트)가 돌므로 Strategy는 1인분만 계산 — 집합형/개별형이
 * 같은 계약을 공유하되, 집합형은 TotalSlots·SlotIndex로 자기 몫을 안다.
 * 환경보정(NavMesh·벽·슬로프)은 여전히 호출부 소관. 입력을 전부 Context로 받아 stateless.
 * Yield Strategy와 동일 패턴: stateless, DataAsset Flyweight 공유 안전.
 *
 * 抽象スロット生成戦略。Context1件からスロット1個を「ワールド座標」で生成。
 * 巡回は呼び出し側、Strategyは1人分のみ。環境補正は呼び出し側の責務。statelessを維持。
 */
UCLASS(Abstract, EditInlineNew, Blueprintable, BlueprintType)
class TACTICALAI_API USlotGeneratorStrategy : public UObject
{
	GENERATED_BODY()

public:
	/**
	 * 슬롯 1개를 월드 좌표로 생성.
	 * @param Context  생성에 필요한 전체 입력 (인원·인덱스·반경·anchor·사거리·적 정보·월드)
	 * @return         월드 좌표 1개. 환경보정 전의 이상 위치.
	 */
	virtual FVector GenerateSlot(const FSlotGenContext& Context) const
		PURE_VIRTUAL(USlotGeneratorStrategy::GenerateSlot, return FVector::ZeroVector;);
};