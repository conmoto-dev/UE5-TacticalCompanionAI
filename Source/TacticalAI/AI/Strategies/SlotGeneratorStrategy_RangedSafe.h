#pragma once

#include "CoreMinimal.h"
#include "AI/Strategies/SlotGeneratorStrategy.h"
#include "SlotGeneratorStrategy_RangedSafe.generated.h"

/**
 * 원거리 안전 위치 배치. 적 분포·타겟·사거리·아군 위치를 근거로
 * "안전하고, 타겟을 칠 수 있고, 아군과 함께 싸우는" 위치를 고른다.
 *
 * 개별형(member-specific): 멤버 1명 기준 평가 → 슬롯이 곧 그 멤버 것(헝가리안 불필요).
 *
 * 동작 모델(영향맵): 방향축을 미리 정하지 않는다. 타겟 둘레 360도에 후보를 촘촘히
 * 깔고, "어디가 좋은가"는 전적으로 점수 함수가 결정한다. 축이 없으므로 플레이어
 * 궤도로 후보장이 회전하던 떨림(구 FrontlineDir 문제)이 구조적으로 사라진다.
 *   - 적 회피: Threat 점수가 적 있는 쪽 후보를 자연히 거른다(방향 안 정해도 적을 피함).
 *   - 아군 응집: PullToAlly 점수가 플레이어 쪽 후보를 가점(후보가 사방에 있어 실제 작동).
 *   - 군집 해소: 동료 점유 페널티가 다른 동료 쪽 후보를 감점(넓은 후보 풀이라 효과적).
 * 카이팅·hysteresis는 여기서 안 한다 — 전자는 상위 행동 레이어, 후자는 컴포넌트 커밋 게이트(ADR-0003).
 *
 * 影響マップ：方向軸を持たない。ターゲット周囲360度に候補を撒き、評価は全てスコア関数。
 * 軸が無いためプレイヤー軌道での候補場回転（旧FrontlineDir問題）が構造的に消える。
 */
UCLASS(BlueprintType, DisplayName = "Slot Gen - Ranged Safe (원거리 안전)")
class TACTICALAI_API USlotGeneratorStrategy_RangedSafe : public USlotGeneratorStrategy
{
	GENERATED_BODY()

public:
	virtual FVector GenerateSlot(const FSlotGenContext& Context) const override;

	// 멤버 1명 기준으로 슬롯을 고르므로 멤버별 배정.
	// メンバー基準で生成するため個別割当。
	virtual ESlotAssignmentPolicy GetAssignmentPolicy() const override
	{
		return ESlotAssignmentPolicy::MemberSpecific;
	}

	// ───── 후보 생성 (타겟 둘레 360도 링) ─────

	// 한 링을 몇 등분해 후보를 깔지. 360도를 이 수로 나눈 각도마다 한 점.
	// 1リングを何分割するか。360度をこの数で割った角度ごとに1点。
	UPROPERTY(EditAnywhere, Category = "RangedSafe|Candidates", meta = (ClampMin = "3", ClampMax = "64"))
	int32 RingSampleCount = 16;

	// 가장 안쪽 링 반경 = AttackRange × 이 비율. 선호 교전 거리.
	// 内側リング半径 = AttackRange × この比率。選好交戦距離。
	UPROPERTY(EditAnywhere, Category = "RangedSafe|Candidates", meta = (ClampMin = "0.1", ClampMax = "1.0"))
	float PreferredRangeRatio = 0.8f;

	// 바깥 링 반경 = AttackRange × 이 비율. 적이 붙었을 때 물러설 여지. 0이면 바깥 링 생략(한 겹).
	// 外側リング半径 = AttackRange × この比率。0なら外リング省略（1重）。
	UPROPERTY(EditAnywhere, Category = "RangedSafe|Candidates", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float OuterRingRatio = 0.92f;

	// ───── 점수: Range Band (사거리 적합도, 보상 [0,1]) ─────

	// 최소 안전 거리 = AttackRange × 이 비율. 이보다 가까우면 사거리 평가 탈락.
	// 最小安全距離 = AttackRange × この比率。これより近いと脱落。
	UPROPERTY(EditAnywhere, Category = "RangedSafe|Scoring", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float MinRangeRatio = 0.45f;

	// 최대 유효 거리 = AttackRange × 이 비율. 이보다 멀면 공격 불가 탈락.
	// 最大有効距離 = AttackRange × この比率。これより遠いと脱落。
	UPROPERTY(EditAnywhere, Category = "RangedSafe|Scoring", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float MaxRangeRatio = 0.95f;

	// ───── 점수: Safety (적 위협장, 벌점 [0,1]) ─────

	// 위협 합산에 포함할 최근접 적 수(K). 가까운 K명만 봐서 배경 위협을 절단.
	// 脅威合算に含める最近接敵数(K)。
	UPROPERTY(EditAnywhere, Category = "RangedSafe|Scoring", meta = (ClampMin = "1", ClampMax = "16"))
	int32 ThreatConsiderCount = 3;

	// 적이 이 거리(cm)에 있으면 위협도 0.5 (반감 거리). 가까울수록 1, 멀수록 0으로 매끄럽게.
	// soft saturation: comfort²/(comfort²+dist²). 1/d² 폭발이 없어 별도 floor 불필요.
	// この距離で脅威0.5（半減距離）。1/d²発散が無いのでfloor不要。
	UPROPERTY(EditAnywhere, Category = "RangedSafe|Scoring", meta = (ClampMin = "1.0"))
	float ThreatComfortDistance = 200.f;

	// ───── 점수: 점유 회피 (동료 슬롯, 벌점 [0,1]) ─────

	// 이 거리 안에 이미 찬 슬롯이 있으면 후보 탈락(하드). 물리적 겹침 방지.
	// この距離内に占有スロットがあれば脱落（ハード）。
	UPROPERTY(EditAnywhere, Category = "RangedSafe|Scoring", meta = (ClampMin = "0.0"))
	float OccupancyHardRadius = 30.f;

	// 이 거리 안의 점유 슬롯은 감점(소프트). 군집 해소의 주력 — 넓은 후보 풀에서 다른 동료 쪽을 깎는다.
	// 가장 가까운 점유까지 거리로 [0,1] 감점. 크게 잡을수록 동료끼리 넓게 흩어진다.
	// この距離内の占有スロットは減点（ソフト）。クラスタ解消の主力。大きいほど広く散る。
	UPROPERTY(EditAnywhere, Category = "RangedSafe|Scoring", meta = (ClampMin = "0.0"))
	float OccupancySoftRadius = 500.f;

	// ───── 점수: 가중치 (모든 축이 [0,1]이므로 가중치끼리 직접 비교 가능) ─────

	// 사거리 적합성 가중치. 클수록 Preferred 거리를 강하게 선호.
	UPROPERTY(EditAnywhere, Category = "RangedSafe|Weights", meta = (ClampMin = "0.0"))
	float RangeWeight = 1.0f;

	// 안전성(적 위협장 회피) 가중치. 클수록 적에게서 멀어지려 함. 적 쪽 후보를 거르는 주력.
	// 安全性の重み。敵側候補を弾く主力。
	UPROPERTY(EditAnywhere, Category = "RangedSafe|Weights", meta = (ClampMin = "0.0"))
	float ThreatWeight = 3.0f;

	// 점유 회피(소프트) 가중치. 동료 군집 해소 강도. 클수록 서로 멀리 흩어진다.
	// 占有回避の重み。クラスタ解消の強さ。
	UPROPERTY(EditAnywhere, Category = "RangedSafe|Weights", meta = (ClampMin = "0.0"))
	float OccupancyWeight = 1.0f;

	// 아군 응집(플레이어 쪽 가점) 가중치. 클수록 플레이어 가까이 모여 함께 싸우는 느낌.
	// 화면 밖 도망 방지. 0으로 두면 순수 안전/사거리만으로 위치 결정(분산 단독 확인용).
	// 味方応集（プレイヤー側加点）の重み。0で純粋な安全/射程のみ（分散単独確認用）。
	UPROPERTY(EditAnywhere, Category = "RangedSafe|Weights", meta = (ClampMin = "0.0"))
	float PullToAllyWeight = 0.5f;

	// ───── 디버그 ─────

	// 후보·점수 시각화 토글. 켜면 각 후보에 점수와 탈락 사유를 그린다.
	UPROPERTY(EditAnywhere, Category = "RangedSafe|Debug")
	bool bDrawDebug = true;

private:
	// =========================================================================
	// 후보 1개의 평가 결과. 점수 합과 탈락 여부, 디버그용 축별 분해를 함께 담는다.
	// 候補1個の評価結果。スコア合計・脱落可否・デバッグ内訳。
	// =========================================================================
	struct FCandidateScore
	{
		FVector Location = FVector::ZeroVector;
		float   Total    = -FLT_MAX;   // 미평가/탈락 후보가 최선이 되지 않도록 최저값.
		bool    bRejected = false;

		// 디버그 breakdown용 축별 점수 (PIE에서 "왜 이 점수?" 추적). 전부 가중 적용 후 값.
		float DebugRange     = 0.f;
		float DebugThreat    = 0.f;
		float DebugOccupancy = 0.f;
		float DebugPull      = 0.f;
		FName DebugRejectReason;
	};

	// 후보 1개 종합 평가 (각 축 함수를 호출해 합산). 합산·가중치만 담당.
	// 候補1個の総合評価。各軸関数を呼び合算するオーケストレーター。
	FCandidateScore ScoreCandidate(
		const FSlotGenContext& Context,
		const FVector& Candidate,
		const FVector& TargetLoc) const;

	// ───── 점수축 (각자 독립적으로 튜닝/교체되는 단위, 전부 [0,1] 출력) ─────

	// 타겟까지 거리가 Band 어디냐로 평가. Band 밖이면 0(탈락 신호), Preferred에서 1.
	float ComputeRangeScore(float DistToTarget, float AttackRange) const;

	// 최근접 K명의 위협을 soft saturation 후 확률적 OR로 합성 → [0,1] 위협도(클수록 위험).
	float ComputeThreatPenalty(const FSlotGenContext& Context, const FVector& Candidate) const;

	// 점유 슬롯 회피. 하드 반경 안이면 reject(true), 소프트 구간이면 최근접 기준 [0,1] 감점.
	float ComputeOccupancyPenalty(const FSlotGenContext& Context, const FVector& Candidate, bool& bOutHardRejected) const;

	// 후보가 리더(플레이어) 쪽일수록 [0,1] 가점. 후보가 360도라 가점이 실제로 우열을 가린다.
	float ComputePullToAllyScore(const FVector& Candidate, const FSlotGenContext& Context) const;
};