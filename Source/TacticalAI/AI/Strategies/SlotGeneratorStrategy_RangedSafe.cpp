#include "AI/Strategies/SlotGeneratorStrategy_RangedSafe.h"

#include "DrawDebugHelpers.h"
#include "GameFramework/Actor.h"

FVector USlotGeneratorStrategy_RangedSafe::GenerateSlot(const FSlotGenContext& Context) const
{
	// [1] 사거리 평가의 중심. Arc는 anchor 기준이지만 RangedSafe는 타겟 기준.
	//     (타겟과 anchor가 지금은 같지만, 분리될 때를 대비해 타겟을 명시적으로 쓴다.)
	// 射程評価の中心。RangedSafeはanchorでなくターゲット基準。
	const AActor* Target = Context.PrimaryTarget.Get();
	const FVector TargetLoc = Target ? Target->GetActorLocation() : Context.Anchor.GetLocation();

	// [2] 적 무게중심 + 전선 방향. 후보를 아군 쪽에 깔고 safety를 평가하는 기준축.
	// 敵重心と前線方向。候補配置と評価の基準軸。
	const FVector EnemyCenter = ComputeEnemyCenter(Context, TargetLoc);

	FVector FrontlineDir = (Context.LeaderLocation - EnemyCenter).GetSafeNormal2D();
	if (FrontlineDir.IsNearlyZero())
	{
		// 리더 위치가 비었으면 anchor 뒤쪽을 아군 쪽으로 간주 (드문 폴백).
		FrontlineDir = (-Context.Anchor.GetRotation().GetForwardVector()).GetSafeNormal2D();
	}
	if (FrontlineDir.IsNearlyZero())
	{
		FrontlineDir = FVector::ForwardVector;
	}

	// [3] 후보 생성. 현재 위치(0번) + 타겟 둘레 Preferred 호(friendly sector).
	// 候補生成。現在位置 + ターゲット周りのPreferredアーク。
	const float PreferredRange = Context.AttackRange * PreferredRangeRatio;

	TArray<FVector> Candidates;
	Candidates.Reserve(SectorCandidateCount + 1);

	// 후보 0: 현재 위치. 트리거로 재배치가 떴어도 현재 위치가 여전히 최선이면 거기 머문다.
	//   (예전의 Stickiness 보너스는 제거 — "안 움직이려는 관성"은 컴포넌트의 커밋 게이트가 소유.)
	// 候補0：現在位置。hysteresisはゲートが持つのでボーナス無し、公平に競争。
	Candidates.Add(Context.RequesterLocation);

	// 후보 1..N: FrontlineDir 중심축으로 ±SectorHalfAngle를 등분한 각도에 배치.
	const int32 SampleCount = FMath::Max(SectorCandidateCount, 1);
	for (int32 i = 0; i < SampleCount; ++i)
	{
		// 등분 비율 t: N=1이면 0(정면), N>=2면 [-1,+1] 균등.
		const float T = (SampleCount == 1) ? 0.f
			: (static_cast<float>(i) / static_cast<float>(SampleCount - 1)) * 2.f - 1.f;
		const float AngleDeg = T * SectorHalfAngleDeg;

		const FVector Dir = FrontlineDir.RotateAngleAxis(AngleDeg, FVector::UpVector);
		Candidates.Add(TargetLoc + Dir * PreferredRange);
	}

	// [4] 후보별 점수 → 최고점 선택.
	// 候補ごとにスコア → 最高点を選択。
	FCandidateScore Best;
	Best.Location = Context.RequesterLocation; // 전원 탈락 시 제자리 유지(안전 폴백).

	for (int32 i = 0; i < Candidates.Num(); ++i)
	{
		const FCandidateScore Score = ScoreCandidate(Context, Candidates[i], TargetLoc, EnemyCenter, FrontlineDir);

		if (!Score.bRejected && Score.Total > Best.Total)
		{
			Best = Score;
		}

#if ENABLE_DRAW_DEBUG
		if (bDrawDebug && Context.World)
		{
			const FColor Color = Score.bRejected ? FColor::Red
				: (i == 0 ? FColor::Yellow : FColor::Green);
			DrawDebugSphere(Context.World, Candidates[i], 20.f, 8, Color, false, 0.f, 0, 1.5f);

			// 탈락이면 사유, 아니면 총점 표시.
			const FString Label = Score.bRejected
				? Score.DebugRejectReason.ToString()
				: FString::Printf(TEXT("%.2f"), Score.Total);
			DrawDebugString(Context.World, Candidates[i] + FVector(0, 0, 40.f), Label, nullptr, FColor::White, 0.f, true);
		}
#endif
	}

#if ENABLE_DRAW_DEBUG
	if (bDrawDebug && Context.World)
	{
		DrawDebugSphere(Context.World, EnemyCenter, 40.f, 12, FColor::Purple, false, 0.f, 0, 2.f);
		DrawDebugDirectionalArrow(Context.World, EnemyCenter, EnemyCenter + FrontlineDir * 300.f,
			80.f, FColor::Cyan, false, 0.f, 0, 4.f);
		// 선택된 자리: 굵은 흰 구.
		DrawDebugSphere(Context.World, Best.Location, 35.f, 12, FColor::White, false, 0.f, 0, 3.f);
	}
#endif

	// 환경보정(NavMesh·벽·슬로프)은 호출부(컴포넌트 [4a])가 수행한다.
	// 環境補正は呼び出し側。
	return Best.Location;
}

USlotGeneratorStrategy_RangedSafe::FCandidateScore USlotGeneratorStrategy_RangedSafe::ScoreCandidate(
    const FSlotGenContext& Context,
    const FVector& Candidate,
    const FVector& TargetLoc,
    const FVector& EnemyCenter,
    const FVector& FrontlineDir) const
{
    FCandidateScore Result;
    Result.Location = Candidate;

    // [1] 사거리 하드 필터 + 보상축. Band 밖이면 탈락, 안이면 [0,1] 적합도.
    const float DistToTarget = FVector::Dist2D(Candidate, TargetLoc);
    const float RangeScore = ComputeRangeScore(DistToTarget, Context.AttackRange);
    if (RangeScore <= 0.f)
    {
        Result.bRejected = true;
        Result.DebugRejectReason = TEXT("OutOfRange");
        return Result;
    }

    // [2] 전선 점수 (가점 [0,1]).
    const float FrontlineScore = ComputeFrontlineScore(Candidate, EnemyCenter, FrontlineDir);

    // [3] 점유 하드 필터 + 소프트 벌점 [0,1]. 너무 가까우면 탈락, 소프트 구간이면 감점.
    bool bHardOccupied = false;
    const float OccupancyPenalty = ComputeOccupancyPenalty(Context, Candidate, bHardOccupied);
    if (bHardOccupied)
    {
        Result.bRejected = true;
        Result.DebugRejectReason = TEXT("Occupied");
        return Result;
    }

    // [4] 위협 벌점 [0,1].
    const float ThreatPenalty = ComputeThreatPenalty(Context, Candidate);

    // [5] 가중 합산. 모든 축 [0,1]. 보상(Range·Frontline) +, 벌점(Threat·Occupancy) -.
    Result.Total =
          RangeScore        * RangeWeight        // 보상 [0,1]
        - ThreatPenalty     * ThreatWeight        // 벌점 [0,1]
        - OccupancyPenalty  * OccupancyWeight     // 벌점 [0,1]
        + FrontlineScore    * FrontlineWeight;    // 보상 [0,1]

    // 디버그 breakdown (가중 적용 후). Frontline은 이제 + 기여.
    Result.DebugRange     = RangeScore       * RangeWeight;
    Result.DebugThreat    = ThreatPenalty    * ThreatWeight;
    Result.DebugOccupancy = OccupancyPenalty * OccupancyWeight;
    Result.DebugFrontline = FrontlineScore   * FrontlineWeight;

    return Result;
}

float USlotGeneratorStrategy_RangedSafe::ComputeRangeScore(float DistToTarget, float AttackRange) const
{
	const float MinRange       = AttackRange * MinRangeRatio;
	const float MaxRange       = AttackRange * MaxRangeRatio;
	const float PreferredRange = AttackRange * PreferredRangeRatio;

	// Band 밖 = 0 (호출부가 탈락 신호로 사용).
	if (DistToTarget < MinRange || DistToTarget > MaxRange)
	{
		return 0.f;
	}

	// Band 안: Preferred에 가까울수록 1, 멀수록 0 쪽으로. Preferred 기준 양방향 감쇠.
	// 가까운 쪽(Min까지)과 먼 쪽(Max까지) 폭이 다르므로 각 방향 분모를 따로 쓴다.
	// Band内：Preferredに近いほど高得点。両方向で分母を分ける。
	const float Denom = (DistToTarget <= PreferredRange)
		? FMath::Max(PreferredRange - MinRange, 1.f)
		: FMath::Max(MaxRange - PreferredRange, 1.f);

	return FMath::Clamp(1.f - FMath::Abs(DistToTarget - PreferredRange) / Denom, 0.f, 1.f);
}

float USlotGeneratorStrategy_RangedSafe::ComputeThreatPenalty(const FSlotGenContext& Context, const FVector& Candidate) const
{
	// 후보~각 적 거리² 수집 → 가까운 K명만 본다(배경 위협 절단).
	// 각 적의 위협도를 soft saturation으로 [0,1]에 매핑: comfort²/(comfort²+dist²).
	//   dist=0 → 1, dist=comfort → 0.5, dist→∞ → 0. 1/d² 폭발이 없어 floor 불필요.
	// K명을 "확률적 OR"로 합성: threat = 1 - Π(1 - danger_i). 항상 [0,1].
	//   가장 가까운 적이 자연히 지배(예전 NearestThreatMultiplier 불필요),
	//   둘러싸이면(적 多) 1에 수렴(예전 Σ의 "둘러싸임 감지"는 유지, 무한대는 제거).
	// 最近接K体をsoft saturation後、確率的ORで合成 → 常に[0,1]。最近接が自然に支配。
	TArray<float, TInlineAllocator<16>> DistancesSq;
	for (const TWeakObjectPtr<const AActor>& EnemyPtr : Context.PerceivedEnemies)
	{
		if (const AActor* Enemy = EnemyPtr.Get())
		{
			DistancesSq.Add(FVector::DistSquared2D(Candidate, Enemy->GetActorLocation()));
		}
	}

	if (DistancesSq.Num() == 0)
	{
		return 0.f; // 적 없음 = 위협 0.
	}

	// 가까운 순 정렬 후 상위 K개만.
	DistancesSq.Sort();
	const int32 Count = FMath::Min(ThreatConsiderCount, DistancesSq.Num());

	const float ComfortSq = ThreatComfortDistance * ThreatComfortDistance;

	// safeProduct = Π(1 - danger_i). 위협 = 1 - safeProduct.
	float SafeProduct = 1.f;
	for (int32 i = 0; i < Count; ++i)
	{
		const float Danger = ComfortSq / (ComfortSq + DistancesSq[i]); // (0,1]
		SafeProduct *= (1.f - Danger);
	}

	return 1.f - SafeProduct; // [0,1)
}

float USlotGeneratorStrategy_RangedSafe::ComputeOccupancyPenalty(
	const FSlotGenContext& Context, const FVector& Candidate, bool& bOutHardRejected) const
{
	bOutHardRejected = false;

	// 소프트 벌점은 "가장 가까운 점유 슬롯"으로 [0,1] (합산 아님 — 합산은 점유 수에 따라 1 초과).
	// ソフト減点は最近接占有で[0,1]（合算は占有数で1超過するため不可）。
	float MaxPenalty = 0.f;
	for (const FVector& Occupied : Context.OccupiedSlots)
	{
		const float Dist = FVector::Dist2D(Candidate, Occupied);

		// 하드: 너무 가까우면 즉시 탈락 (물리적 겹침 방지).
		if (Dist < OccupancyHardRadius)
		{
			bOutHardRejected = true;
			return 1.f;
		}

		// 소프트: 하드~소프트 구간이면 가까울수록 감점, 최근접 기준 최대값.
		if (Dist < OccupancySoftRadius)
		{
			const float Penalty = 1.f - (Dist / FMath::Max(OccupancySoftRadius, 1.f));
			MaxPenalty = FMath::Max(MaxPenalty, Penalty);
		}
	}

	return MaxPenalty; // [0,1]
}

float USlotGeneratorStrategy_RangedSafe::ComputeFrontlineScore(
	const FVector& Candidate, const FVector& EnemyCenter, const FVector& FrontlineDir) const
{
	const FVector ToCandidate = Candidate - EnemyCenter;

	// 소프트(가점): 방향 코사인 [-1,1] → [0,1]. 아군 쪽 1, 수직 0.5, 적 쪽 0.
	// 감점이 아니라 가점 — "전선 쪽일수록 좋다"를 직접 표현(Threat/Occupancy는 반대로 나쁜 자리 감점).
	// 加点：味方側ほど高得点。減点でなく「前線側ほど良い」を直接表現。
	const float FrontSide = FVector::DotProduct(ToCandidate.GetSafeNormal2D(), FrontlineDir);
	return (FrontSide + 1.f) * 0.5f;
}

FVector USlotGeneratorStrategy_RangedSafe::ComputeEnemyCenter(const FSlotGenContext& Context, const FVector& Fallback) const
{
	FVector Sum = FVector::ZeroVector;
	int32 Count = 0;
	for (const TWeakObjectPtr<const AActor>& EnemyPtr : Context.PerceivedEnemies)
	{
		if (const AActor* Enemy = EnemyPtr.Get())
		{
			Sum += Enemy->GetActorLocation();
			++Count;
		}
	}

	return (Count > 0) ? Sum / static_cast<float>(Count) : Fallback;
}