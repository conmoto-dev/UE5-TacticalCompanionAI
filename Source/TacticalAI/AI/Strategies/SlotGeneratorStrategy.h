#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "SlotGeneratorStrategy.generated.h"

class AActor;

// =========================================================================
// 슬롯 배정 정책. Strategy가 "내 슬롯을 멤버에게 어떻게 나눠줄지"를 선언한다.
// 배정 방식은 생성 방식의 따름정리 — 생성 주체(Strategy)가 함께 선언해야
// 모순 조합(집합 생성 + 항등 배정 등)이 타입 레벨에서 불가능해진다.
// 실행(헝가리안 호출)은 컴포넌트 소관 — Strategy는 선언만, 컴포넌트가 분기.
// =========================================================================
// スロット割当方針。Strategyが「自分のスロットをどう配るか」を宣言。
// 割当は生成の従系 — 生成主体が宣言することで矛盾組合せを型レベルで排除。
// 実行(ハンガリアン呼出)はコンポーネント側。Strategyは宣言のみ。
UENUM(BlueprintType)
enum class ESlotAssignmentPolicy : uint8
{
	GroupHungarian,

	// 멤버 1명 기준으로 슬롯 생성 → 슬롯이 곧 그 멤버의 것 (항등 배정, 헝가리안 불필요).
	MemberSpecific
};

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

	// 리더(플레이어) 위치. 전선 방향 산출의 기준 — "적 무리에서 리더를 향하는 쪽"이
	// 아군 전선이다. 원거리가 적 뒤로 넘어가지 않도록 friendly sector를 정의하는 데 쓴다.
	// リーダー(プレイヤー)位置。前線方向の基準 — 「敵集団からリーダーへ向かう側」が味方前線。
	FVector LeaderLocation = FVector::ZeroVector;
	
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

	// 이미 배치 확정된 슬롯들 (이 그룹 이전 + 이 그룹 내 앞선 멤버).
	// 원거리 평가가 "이미 찬 자리"를 피하기 위한 입력. 모든 Strategy 공통 — 단,
	// 점유 회피의 "소프트 선호"는 각 Strategy 점수에서, "최소 거리 하드 보장"은
	// 컴포넌트 공통 후처리(push)에서. Strategy가 제멋대로 무시하면 순서 의존 버그.
	// 既に確定したスロット群。遠距離評価が「埋まった位置」を避けるための入力。
	// ソフトな回避は各Strategyのスコア、ハードな最小距離保証はコンポーネント側で。
	TArray<FVector> OccupiedSlots;
	
	// 이 슬롯을 요청한 멤버의 현재 위치. 개별형이 "나"를 기준으로 평가할 때 사용.
	// (Stickiness: 현재 위치가 충분히 좋으면 안 움직임 / 접근성: 후보까지 이동 거리.)
	// 집합형(Arc)은 멤버 개체와 무관하게 anchor·index로만 배치하므로 무시.
	// このスロットを要求したメンバーの現在位置。個別型が「自分」基準で評価する際に使用。
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
	
	/**
	 * 이 Strategy의 슬롯을 멤버에게 배정하는 방식. 기본은 집합형(헝가리안).
	 * 멤버별 생성 Strategy만 MemberSpecific으로 override.
	 * 생성 방식과 짝이므로 Strategy가 선언 — 컴포넌트가 이걸 읽어 배정을 분기한다.
	 */
	// このStrategyのスロット割当方式。デフォルトは集合型(ハンガリアン)。
	virtual ESlotAssignmentPolicy GetAssignmentPolicy() const
	{
		return ESlotAssignmentPolicy::GroupHungarian;
	}
	
	/**
	 * 커밋된 슬롯이 아직 유효한지 판정. true = 재배치 필요.
	 * 유효성 기준은 슬롯을 만든 Strategy가.
	 * 게이트 루프·커밋 관리·첫 커밋 판정은 컴포넌트 소관, 여기선 답만 한다.
	 * 기본 구현 = 사거리 이탈(하드): 커밋 슬롯에서 타겟을 못 때리면 무가치.
	 *
	 * コミット済みスロットの有効性判定。true＝再配置が必要。
	 * 無効条件は生成したStrategyが判定も持つ。
	 * デフォルト実装＝射程逸脱（ハード）。
	 *
	 * @param Context         생성 때와 동일한 입력 컨텍스트 (타겟·사거리·적 분포)
	 * @param CommittedSlot   현재 커밋된 슬롯 (검증 대상은 이 목적지 — live 위치 아님)
	 * @param TimeSinceCommit 커밋 후 경과 시간(초). reluctance류 판정의 입력.
	 */
	virtual bool ShouldReposition(const FSlotGenContext& Context,
		const FVector& CommittedSlot, float TimeSinceCommit) const;
};