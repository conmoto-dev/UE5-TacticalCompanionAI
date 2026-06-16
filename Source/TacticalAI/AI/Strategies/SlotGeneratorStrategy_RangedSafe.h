#pragma once

#include "CoreMinimal.h"
#include "AI/Strategies/SlotGeneratorStrategy.h"
#include "SlotGeneratorStrategy_RangedSafe.generated.h"

/**
 * 원거리 안전 위치 배치. 적 전체 분포·타겟·사거리를 근거로 "충분히 안전하고,
 * 전선을 유지하며, 타겟을 계속 칠 수 있는" 위치를 고른다.
 * Arc와 달리 anchor가 아니라 PrimaryTarget을 사거리 평가의 중심으로 쓴다.
 *
 * 개별형(member-specific): 멤버 1명을 기준으로 후보를 평가하므로,
 * 슬롯이 곧 그 멤버의 것 → 그룹 헝가리안 불필요(컴포넌트가 skip).
 *
 * 동작 모델: 후보를 이산 샘플링(현재 위치 + 타겟 둘레 friendly sector 호)
 * 다축 점수로 최선을 고른다. 모든 소프트 축은 [0,1]로 정규화 → 가중치가 서로 비교 가능.
 * "능동 회피(카이팅)"와 "안 움직이려는 관성(hysteresis)"은 여기서 하지 않는다 —
 * 전자는 상위 행동 레이어, 후자는 컴포넌트의 재배치 커밋 게이트(ADR-0003)가 소유.
 *
 * 遠距離の安全位置配置。全ソフト軸を[0,1]に正規化 → 重みが相互比較可能。
 * カイティングとhysteresisはここで持たない（後者は再配置コミットゲートが所有）。
 */
UCLASS(BlueprintType, DisplayName = "Slot Gen - Ranged Safe (원거리 안전)")
class TACTICALAI_API USlotGeneratorStrategy_RangedSafe : public USlotGeneratorStrategy
{
	GENERATED_BODY()

public:
	virtual FVector GenerateSlot(const FSlotGenContext& Context) const override;

	// 멤버 1명(RequesterLocation·AttackRange) 기준으로 슬롯을 고르므로 멤버별 배정.
	// メンバー基準で生成するため個別割当。
	virtual ESlotAssignmentPolicy GetAssignmentPolicy() const override
	{
		return ESlotAssignmentPolicy::MemberSpecific;
	}

	// ───── 후보 생성 파라미터 ─────

	// 선호 교전 거리 = AttackRange × 이 비율. 후보 호를 이 반경으로 그린다.
	// 選好交戦距離 = AttackRange × この比率。射程ギリギリ(1.0)を避け少し内側に。
	UPROPERTY(EditAnywhere, Category = "RangedSafe|Candidates", meta = (ClampMin = "0.1", ClampMax = "1.0"))
	float PreferredRangeRatio = 0.8f;

	// friendly sector 반각(도). 전선 방향 기준 좌우로 이만큼 펼쳐 후보를 깐다.
	// friendly sectorの半角。広すぎると円になり前方が無意味化。
	UPROPERTY(EditAnywhere, Category = "RangedSafe|Candidates", meta = (ClampMin = "10.0", ClampMax = "180.0"))
	float SectorHalfAngleDeg = 30.f;

	// sector를 펼칠 후보 개수 (현재 위치 후보 0번 제외). 호 등분 점의 수.
	// 候補数（現在位置の0番を除く）。アーク等分点の数。
	UPROPERTY(EditAnywhere, Category = "RangedSafe|Candidates", meta = (ClampMin = "1", ClampMax = "32"))
	int32 SectorCandidateCount = 7;

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

	// 이 거리 안의 점유 슬롯은 감점(소프트). 가장 가까운 점유까지 거리로 [0,1] 감점.
	// この距離内の占有スロットは減点（ソフト、最近接で[0,1]）。
	UPROPERTY(EditAnywhere, Category = "RangedSafe|Scoring", meta = (ClampMin = "0.0"))
	float OccupancySoftRadius = 70.f;

	// ───── 점수: 가중치 (모든 축이 [0,1]이므로 가중치끼리 직접 비교 가능) ─────

	// 사거리 적합성 가중치. 클수록 Preferred 거리를 강하게 선호.
	UPROPERTY(EditAnywhere, Category = "RangedSafe|Weights", meta = (ClampMin = "0.0"))
	float RangeWeight = 1.2f;

	// 안전성(적 위협장 회피) 가중치. 클수록 적에게서 멀어지려 함.
	UPROPERTY(EditAnywhere, Category = "RangedSafe|Weights", meta = (ClampMin = "0.0"))
	float ThreatWeight = 1.5f;

	// 점유 회피(소프트) 가중치. 이미 찬 슬롯에 가까운 후보 감점 강도.
	UPROPERTY(EditAnywhere, Category = "RangedSafe|Weights", meta = (ClampMin = "0.0"))
	float OccupancyWeight = 0.8f;

	// 전선 유지(아군 쪽 가점) 가중치. 클수록 전선 기준 아군 쪽 후보를 강하게 선호.
	// 前線維持（味方側加点）の重み。
	UPROPERTY(EditAnywhere, Category = "RangedSafe|Weights", meta = (ClampMin = "0.0"))
	float FrontlineWeight = 3.0f;

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
		float DebugFrontline = 0.f;
		FName DebugRejectReason;
	};

	// 후보 1개 종합 평가 (각 축 함수를 호출해 합산). 합산·가중치만 담당.
	// 候補1個の総合評価。各軸関数を呼び合算するオーケストレーター。
	FCandidateScore ScoreCandidate(
		const FSlotGenContext& Context,
		const FVector& Candidate,
		const FVector& TargetLoc,
		const FVector& EnemyCenter,
		const FVector& FrontlineDir) const;

	// ───── 점수축 (각자 독립적으로 튜닝/교체되는 단위, 전부 [0,1] 출력) ─────

	// 타겟까지 거리가 Band 어디냐로 평가. Band 밖이면 0(탈락 신호), Preferred에서 1.
	float ComputeRangeScore(float DistToTarget, float AttackRange) const;

	// 최근접 K명의 위협을 soft saturation 후 확률적 OR로 합성 → [0,1] 위협도(클수록 위험).
	float ComputeThreatPenalty(const FSlotGenContext& Context, const FVector& Candidate) const;

	// 점유 슬롯 회피. 하드 반경 안이면 reject(true), 소프트 구간이면 최근접 기준 [0,1] 감점.
	float ComputeOccupancyPenalty(const FSlotGenContext& Context, const FVector& Candidate, bool& bOutHardRejected) const;

	// 적 무리 무게중심 산출. 유효 적 없으면 Fallback 반환.
	FVector ComputeEnemyCenter(const FSlotGenContext& Context, const FVector& Fallback) const;
	
	// 전선 점수(가점). 후보가 전선 기준 아군 쪽일수록 [0,1] 가점 (아군 1, 수직 0.5, 적 0).
	float ComputeFrontlineScore(const FVector& Candidate, const FVector& EnemyCenter, const FVector& FrontlineDir)const;
};