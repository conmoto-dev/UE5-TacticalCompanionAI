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

	// 후보 0: 현재 위치. 충분히 좋으면 안 움직이는 게 가장 자연스럽다(stickiness 대상).
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
		const FCandidateScore Score = ScoreCandidate(
			Context, Candidates[i], TargetLoc, EnemyCenter, FrontlineDir, /*bIsCurrentLocation=*/ i == 0);

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
	const FVector& FrontlineDir,
	bool bIsCurrentLocation) const
{
	FCandidateScore Result;
	Result.Location = Candidate;

	// [1] 사거리 하드 필터. Band 완전히 벗어나면 평가할 가치 없음 → 탈락.
	// 射程ハードフィルタ。Band外は脱落。
	const float DistToTarget = FVector::Dist2D(Candidate, TargetLoc);
	const float RangeScore = ComputeRangeScore(DistToTarget, Context.AttackRange);
	if (RangeScore <= 0.f)
	{
		Result.bRejected = true;
		Result.DebugRejectReason = TEXT("OutOfRange");
		return Result;
	}

	// [2] 전선 하드 필터 + 소프트 감점. 깊게 적 뒤면 탈락, 살짝 벗어나면 감점.
	// 前線ハードフィルタ+ソフト減点。
	const float SignedDepth = FVector::DotProduct(Candidate - EnemyCenter, FrontlineDir);
	if (SignedDepth < -ForbiddenBackDepth)
	{
		Result.bRejected = true;
		Result.DebugRejectReason = TEXT("BehindEnemy");
		return Result;
	}
	// FrontSide < 0 = 전선보다 약간 적 쪽 → 그 깊이만큼 감점.
	const float FrontSide = FVector::DotProduct((Candidate - EnemyCenter).GetSafeNormal2D(), FrontlineDir);
	float FrontlinePenalty = 0.f;
	if (!bIsCurrentLocation && FrontSide < 0.f)
	{
		FrontlinePenalty = -FrontSide;
	}

	// [3] 점유 하드 필터 + 소프트 감점. 너무 가까우면 탈락, 소프트 구간이면 감점.
	// 占有ハードフィルタ+ソフト減点。
	bool bHardOccupied = false;
	const float OccupancyPenalty = ComputeOccupancyPenalty(Context, Candidate, bHardOccupied);
	if (bHardOccupied)
	{
		Result.bRejected = true;
		Result.DebugRejectReason = TEXT("Occupied");
		return Result;
	}

	// [4] 연속 점수축.
	const float ThreatPenalty = ComputeThreatPenalty(Context, Candidate);
	const float MoveDist = FVector::Dist2D(Context.RequesterLocation, Candidate);
	const float MoveCost = MoveDist / FMath::Max(Context.AttackRange, 1.f); // 사거리 스케일로 정규화.
	const float Stickiness = bIsCurrentLocation ? StickinessBonus : 0.f;

	// [5] 가중 합산.
	Result.Total =
		  RangeScore        * RangeWeight
		- ThreatPenalty     * ThreatWeight
		- MoveCost          * MoveCostWeight
		- OccupancyPenalty  * OccupancyWeight
		- FrontlinePenalty  * FrontlinePenaltyWeight
		+ Stickiness;
		

	// 디버그 breakdown 기록.
	Result.DebugRange      = RangeScore        * RangeWeight;
	Result.DebugThreat     = ThreatPenalty     * ThreatWeight;
	Result.DebugMoveCost   = MoveCost          * MoveCostWeight;
	Result.DebugOccupancy  = OccupancyPenalty  * OccupancyWeight;
	Result.DebugFrontline  = FrontlinePenalty  * FrontlinePenaltyWeight;
	Result.DebugStickiness = Stickiness;
	
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
	// 후보~각 적 거리² 수집 → 가까운 K명만 위협 합산.
	// 전체 합산은 적 수에 위협이 비례해 개별 접근에 둔감 → 최근접 K로 배경 절단.
	// 반환은 "위협 벌점"(클수록 위험). 호출부에서 감점. 포화 없음 — 가까울수록 강하게 깎는 직관.
	// 最近接K体の脅威合算。返り値は脅威ペナルティ（大きいほど危険、飽和なし）。
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

	float Threat = 0.f;
	for (int32 i = 0; i < Count; ++i)
	{
		const float Dist = FMath::Max(FMath::Sqrt(DistancesSq[i]), ThreatDistanceFloor);
		const float Normalized = Dist / 100.f;
		const float Base = 1.f / FMath::Square(Normalized);

		// 가장 가까운 적(0번)에 가중. "지금 제일 붙은 놈"이 제일 위험.
		// 最近接(0番)に重み付け。
		const float Weight = (i == 0) ? NearestThreatMultiplier : 1.f;
		Threat += Base * Weight;
	}

	return Threat;
}

float USlotGeneratorStrategy_RangedSafe::ComputeOccupancyPenalty(
	const FSlotGenContext& Context, const FVector& Candidate, bool& bOutHardRejected) const
{
	bOutHardRejected = false;

	float Penalty = 0.f;
	for (const FVector& Occupied : Context.OccupiedSlots)
	{
		const float Dist = FVector::Dist2D(Candidate, Occupied);

		// 하드: 너무 가까우면 즉시 탈락 (물리적 겹침 방지).
		if (Dist < OccupancyHardRadius)
		{
			bOutHardRejected = true;
			return 1.f;
		}

		// 소프트: 하드~소프트 구간이면 가까울수록 감점.
		if (Dist < OccupancySoftRadius)
		{
			Penalty += 1.f - (Dist / FMath::Max(OccupancySoftRadius, 1.f));
		}
	}

	return Penalty;
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