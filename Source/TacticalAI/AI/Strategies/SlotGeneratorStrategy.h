#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "SlotGeneratorStrategy.generated.h"

class AActor;

// =========================================================================
// 슬롯 생성 입력 컨텍스트.
// 계약은 "1인 → 1슬롯". 멤버 순회는 호출자(컴포넌트)가 돌고, Strategy는 1건만 답한다.
// 한 틱 안에서 소비되는 스택 임시 객체 — 리플렉션·GC 추적 불필요(평범한 struct).
// =========================================================================
// スロット生成の入力コンテキスト。契約は「1人→1スロット」。巡回は呼び出し側。
// 集合型(Arc)はTotalSlots・SlotIndexで、個別型(RangedSafe)はAttackRange・敵分布で算出。
struct FSlotGenContext
{
	// 해석은 Strategy 소관 — 호의 반지름일 수도, 선호 교전 거리일 수도, 무시할 수도.
	// 타겟 표면 기준 이격 (타겟 EncircleRadius + 역할별 RadiusOffset).
	// ターゲット表面基準の間隔。表面距離＝中心距離−この値。生成と判定が同座標系。
	float BaseRadius = 0.f;

	// 리더(플레이어) 위치. 전선 방향 산출의 기준 — "적 무리에서 리더를 향하는 쪽"이
	// 아군 전선이다. 원거리가 적 뒤로 넘어가지 않도록 friendly sector를 정의하는 데 쓴다.
	// リーダー(プレイヤー)位置。前線方向の基準 — 「敵集団からリーダーへ向かう側」が味方前線。
	FVector LeaderLocation = FVector::ZeroVector;
	
	// 기준 프레임. Arc류는 이걸로 로컬→월드 변환, 공간 평가류는 참고만.
	FTransform Anchor;

	// 이 멤버 1명의 기준 교전 사거리. 모든 포메이션이 참조할 수 있는 범용 파라미터.
	// このメンバー1人の基準射程。全フォーメーションが参照しうる汎用パラメータ。
	float AttackRange = 0.f;

	// 현재 교전 타겟. / 現在の交戦ターゲット。
	TWeakObjectPtr<const AActor> PrimaryTarget;

	// 인지한 적 전체 (타겟 포함). 원거리 안전 위치 평가의 핵심 입력.
	// 知覚した敵全体（ターゲット含む）。遠距離の安全位置評価の中核入力。
	TArray<TWeakObjectPtr<const AActor>> PerceivedEnemies;

	// 이미 배치 확정된 슬롯들 (이 그룹 이전 + 이 그룹 내 앞선 멤버).
	// "이미 찬 자리"를 피하기 위한 입력.
	// 他メンバーの「コミット済み」スロット。
	TArray<FVector> OccupiedSlots;
	
	// 이 슬롯을 요청한 멤버의 현재 위치. 접근 방향·이동 거리·현위치 후보 평가의 기준.
	// このスロットを要求したメンバーの現在位置。
	FVector RequesterLocation = FVector::ZeroVector;
	
	// 명시 전달 — Instanced UObject는 Outer 체인으로 GetWorld() 불가.
	// Instanced UObjectはOuter経由でGetWorld()不可のため明示的に渡す。
	UWorld* World = nullptr;
};

/**
 * 추상 슬롯 생성 전략. 컨텍스트 1건을 받아 슬롯 1개를 "월드 좌표"로 생성한다.
 * 멤버 순회는 호출자(컴포넌트)가 돌므로 Strategy는 1인분만 계산 — 집합형/개별형이
 * 같은 계약을 공유하되, 집합형은 TotalSlots·SlotIndex로 자기 몫을 안다.
 * 환경보정(NavMesh·벽·슬로프)은 여전히 호출부 소관. 입력을 전부 Context로 받아 stateless.
 *
 * 抽象スロット生成戦略。Context1件からスロット1個を「ワールド座標」で生成。
 * コミット済みスロットの有効性判定も担当（無効条件は生成ロジックの従系）。
 * ゲートループ・コミット管理は呼び出し側。statelessを維持。
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
	
	/**
	 * 커밋된 슬롯이 아직 유효한지 판정. true = 재배치 필요.
	 * 기본 구현 = 사거리 이탈(하드): 타겟을 때리지 못하는 슬롯 커밋 방지.
	 *
	 * コミット済みスロットの有効性判定。true＝再配置が必要。
	 * デフォルト実装＝射程逸脱（表面基準ハード）。
	 *
	 * @param Context         생성 때와 동일한 입력 컨텍스트
	 * @param CommittedSlot   현재 커밋된 슬롯 (검증 대상은 이 목적지 — live 위치 아님)
	 * @param TimeSinceCommit 커밋 후 경과 시간(초). reluctance류 판정의 입력.
	 */
	virtual bool ShouldReposition(const FSlotGenContext& Context,
		const FVector& CommittedSlot, float TimeSinceCommit) const;
};