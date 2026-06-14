#pragma once

#include "CoreMinimal.h"
#include "AI/Strategies/SlotGeneratorStrategy.h"
#include "SlotGeneratorStrategy_RangedSafe.generated.h"

/**
 * 원거리 안전 위치 배치. 적 전체 분포·타겟·사거리를 근거로 "충분히 안전하고,
 * 전선을 유지하며, 타겟을 계속 칠 수 있고, 덜 움직이는" 위치를 고른다.
 * Arc와 달리 anchor가 아니라 PrimaryTarget을 사거리 평가의 중심으로 쓴다.
 *
 * 개별형(member-specific): 멤버 1명을 기준으로 후보를 평가하므로,
 * 슬롯이 곧 그 멤버의 것 → 그룹 헝가리안 불필요(컴포넌트가 skip).
 *
 * 동작 모델: 후보를 이산 샘플링(현재 위치 + 타겟 둘레 friendly sector 호)
 * 다축 점수로 최선을 고른다. EQS식 공간 샘플링이며, 조율 없는 자율 배치다.
 * "능동 회피(카이팅)"는 여기서 하지 않는다 — 위치 선정만, 회피는 상위 행동 레이어.
 *
 * 遠距離の安全位置配置。Arcと違いPrimaryTargetを射程評価の中心とする。
 * 候補を離散サンプリングし多軸スコアで最良を選ぶ（EQS式・自律配置）。
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

	// ───── 점수: Range Band (사거리 평가) ─────

	// 최소 안전 거리 = AttackRange × 이 비율. 이보다 가까우면 사거리 평가 탈락.
	// 最小安全距離 = AttackRange × この比率。これより近いと射程評価で脱落。
	UPROPERTY(EditAnywhere, Category = "RangedSafe|Scoring", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float MinRangeRatio = 0.45f;

	// 최대 유효 거리 = AttackRange × 이 비율. 이보다 멀면 공격 불가 탈락.
	// 最大有効距離 = AttackRange × この比率。これより遠いと攻撃不可で脱落。
	UPROPERTY(EditAnywhere, Category = "RangedSafe|Scoring", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float MaxRangeRatio = 0.95f;

	// ───── 점수: Safety (적 위협장) ─────

	// 위협 합산에 포함할 최근접 적 수(K). 전체(Σ)는 적 수에 위협이 비례해
	// 개별 접근에 둔감해짐 → 가까운 K명만 봐서 배경 위협을 절단. (min=1, 둘러싸임 못 봄.)
	// 脅威合算に含める最近接敵数(K)。Σ全体は敵数依存、minは囲まれ見逃し → Kで折衷。
	UPROPERTY(EditAnywhere, Category = "RangedSafe|Scoring", meta = (ClampMin = "1", ClampMax = "16"))
	int32 ThreatConsiderCount = 3;

	// 위협 거리 분모 하한. 적과 거의 겹칠 때 1/dist² 폭발 방지.
	// 脅威距離の下限。重なり時の発散防止。
	UPROPERTY(EditAnywhere, Category = "RangedSafe|Scoring", meta = (ClampMin = "1.0"))
	float ThreatDistanceFloor = 100.f;

	// ───── 점수: friendly side (전선 유지) ─────

	// 이 깊이 이상으로 적진 뒤(전선 반대편)로 넘어간 후보는 탈락.
	// 살짝 벗어남은 탈락이 아니라 감점(아래 가중치) — 특정 배치에서 후보가 전멸하는 것 방지.
	// この深さ以上に敵陣の裏へ回る候補は脱落。軽い逸脱は減点のみ。
	UPROPERTY(EditAnywhere, Category = "RangedSafe|Scoring", meta = (ClampMin = "0.0"))
	float ForbiddenBackDepth = 150.f;

	// ───── 점수: 점유 회피 (동료 슬롯) ─────

	// 이 거리 안에 이미 찬 슬롯이 있으면 후보 탈락(하드). 물리적 겹침 방지.
	// この距離内に占有スロットがあれば脱落（ハード）。
	UPROPERTY(EditAnywhere, Category = "RangedSafe|Scoring", meta = (ClampMin = "0.0"))
	float OccupancyHardRadius = 50.f;

	// 이 거리 안의 점유 슬롯은 감점(소프트). 하드 반경 밖~이 반경 안 구간에서 거리 비례 감점.
	// この距離内の占有スロットは減点（ソフト）。
	UPROPERTY(EditAnywhere, Category = "RangedSafe|Scoring", meta = (ClampMin = "0.0"))
	float OccupancySoftRadius = 100.f;

	// ───── 점수: 가중치 ─────

	// 사거리 적합성 가중치. 클수록 Preferred 거리를 강하게 선호.
	UPROPERTY(EditAnywhere, Category = "RangedSafe|Weights", meta = (ClampMin = "0.0"))
	float RangeWeight = 1.2f;

	// 안전성(적 위협장 회피) 가중치. 클수록 적에게서 멀어지려 함.
	UPROPERTY(EditAnywhere, Category = "RangedSafe|Weights", meta = (ClampMin = "0.0"))
	float ThreatWeight = 2.f;

	UPROPERTY(EditAnywhere, Category = "RangedSafe|Scoring", meta = (ClampMin = "1.0"))
	float NearestThreatMultiplier = 2.f;
	
	// 현재 위치 유지 보너스. 후보 0번(현재 위치)에만 가산 → 충분히 좋으면 안 움직임(떨림 방지).
	// 고정값이다(safety 종속 아님): 종속시키면 끈질긴 적 상대로 무한 카이팅이 되므로,
	// "쫓기면 어떻게"는 상위 행동 레이어(StateTree)에 맡긴다.
	// 現在位置維持ボーナス。固定値（safety非依存 — 依存させると無限カイティング）。
	UPROPERTY(EditAnywhere, Category = "RangedSafe|Weights", meta = (ClampMin = "0.0"))
	float StickinessBonus = 1.2f;

	// 이동 비용 가중치. 현재 위치에서 먼 후보일수록 감점 → 과도한 재배치 억제.
	UPROPERTY(EditAnywhere, Category = "RangedSafe|Weights", meta = (ClampMin = "0.0"))
	float MoveCostWeight = 1.7f;

	// 점유 회피(소프트) 가중치. 이미 찬 슬롯에 가까운 후보 감점 강도.
	UPROPERTY(EditAnywhere, Category = "RangedSafe|Weights", meta = (ClampMin = "0.0"))
	float OccupancyWeight = 1.0f;

	// 전선 이탈(소프트) 가중치. 적진 쪽으로 살짝 벗어난 후보 감점 강도.
	UPROPERTY(EditAnywhere, Category = "RangedSafe|Weights", meta = (ClampMin = "0.0"))
	float FrontlinePenaltyWeight = 1.0f;

	// ───── 디버그 ─────

	// 후보·점수 시각화 토글. 켜면 각 후보에 점수와 탈락 사유를 그린다.
	UPROPERTY(EditAnywhere, Category = "RangedSafe|Debug")
	bool bDrawDebug = true;

private:
	// =========================================================================
	// 후보 1개의 평가 결과. 점수 합과 탈락 여부, 디버그용 축별 분해를 함께 담는다.
	// reject와 점수를 같이 반환해야 하므로 구조체로 묶음.
	// 候補1個の評価結果。スコア合計・脱落可否・デバッグ内訳。
	// =========================================================================
	struct FCandidateScore
	{
		FVector Location = FVector::ZeroVector;
		float   Total    = -FLT_MAX;   // 미평가/탈락 후보가 최선이 되지 않도록 최저값.
		bool    bRejected = false;

		// 디버그 breakdown용 축별 점수 (PIE에서 "왜 이 점수?" 추적).
		float DebugRange      = 0.f;
		float DebugThreat     = 0.f;
		float DebugStickiness = 0.f;
		float DebugMoveCost   = 0.f;
		float DebugOccupancy  = 0.f;
		float DebugFrontline  = 0.f;
		FName DebugRejectReason;
	};

	// 후보 1개 종합 평가 (각 축 함수를 호출해 합산). 합산·가중치만 담당.
	// 候補1個の総合評価。各軸関数を呼び合算するオーケストレーター。
	FCandidateScore ScoreCandidate(
		const FSlotGenContext& Context,
		const FVector& Candidate,
		const FVector& TargetLoc,
		const FVector& EnemyCenter,
		const FVector& FrontlineDir,
		bool bIsCurrentLocation) const;

	// ───── 점수축 (각자 독립적으로 튜닝/교체되는 단위) ─────

	// 타겟까지 거리가 Band 어디냐로 평가. Band 밖이면 0(탈락 신호).
	float ComputeRangeScore(float DistToTarget, float AttackRange) const;

	// 최근접 K명의 적 위협 합산 → 위협 벌점(클수록 위험). 호출부에서 감점.
	float ComputeThreatPenalty(const FSlotGenContext& Context, const FVector& Candidate) const;

	// 점유 슬롯 회피. 하드 반경 안이면 reject(true), 소프트 구간이면 감점값 반환.
	float ComputeOccupancyPenalty(const FSlotGenContext& Context, const FVector& Candidate, bool& bOutHardRejected) const;

	// 적 무리 무게중심 산출. 유효 적 없으면 Fallback 반환.
	FVector ComputeEnemyCenter(const FSlotGenContext& Context, const FVector& Fallback) const;
};