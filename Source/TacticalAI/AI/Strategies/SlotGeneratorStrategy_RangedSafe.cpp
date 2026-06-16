#include "AI/Strategies/SlotGeneratorStrategy_RangedSafe.h"

#include "DrawDebugHelpers.h"
#include "GameFramework/Actor.h"

FVector USlotGeneratorStrategy_RangedSafe::GenerateSlot(const FSlotGenContext& Context) const
{
	// [1] 사거리 평가의 중심. RangedSafe는 anchor가 아니라 타겟 기준.
	const AActor* Target = Context.PrimaryTarget.Get();
	const FVector TargetLoc = Target ? Target->GetActorLocation() : Context.Anchor.GetLocation();

	// [2] 후보 생성 — 타겟 둘레 360도 링. 방향축 없음.
	//     어느 쪽이 좋은지는 후보를 안 가리고, 전적으로 [3] 점수가 결정한다.
	//     축이 없으므로 플레이어 궤도로 후보장이 회전하던 떨림이 구조적으로 없다.
	// 候補は方向を選ばず360度に撒く。良し悪しは全てスコアが決める（軸が無い=回転しない）。
	const float InnerRange = Context.AttackRange * PreferredRangeRatio;
	const float OuterRange = Context.AttackRange * OuterRingRatio;
	const bool  bUseOuter  = OuterRingRatio > KINDA_SMALL_NUMBER && OuterRange > InnerRange + 1.f;

	const int32 Samples = FMath::Max(RingSampleCount, 3);
	TArray<FVector> Candidates;
	Candidates.Reserve(Samples * (bUseOuter ? 2 : 1) + 1);

	// 후보 0: 현재 위치. 재배치 트리거가 떴어도 현재 위치가 여전히 최선이면 머문다.
	//   hysteresis는 컴포넌트 커밋 게이트가 소유(ADR-0003) — 여기선 보너스 없이 공정 경쟁.
	Candidates.Add(Context.RequesterLocation);

	// 내측 링(+ 선택적 외측 링)을 등각으로. 시작 각도 오프셋 없음 — 어느 방향도 특별하지 않다.
	for (int32 i = 0; i < Samples; ++i)
	{
		const float AngleRad = (static_cast<float>(i) / static_cast<float>(Samples)) * 2.f * PI;
		const FVector Dir(FMath::Cos(AngleRad), FMath::Sin(AngleRad), 0.f);

		Candidates.Add(TargetLoc + Dir * InnerRange);
		if (bUseOuter)
		{
			Candidates.Add(TargetLoc + Dir * OuterRange);
		}
	}

	// [3] 후보별 점수 → 최고점 선택.
	FCandidateScore Best;
	Best.Location = Context.RequesterLocation; // 전원 탈락 시 제자리 유지(안전 폴백).

	for (int32 i = 0; i < Candidates.Num(); ++i)
	{
		const FCandidateScore Score = ScoreCandidate(Context, Candidates[i], TargetLoc);

		if (!Score.bRejected && Score.Total > Best.Total)
		{
			Best = Score;
		}

#if ENABLE_DRAW_DEBUG
		if (bDrawDebug && Context.World)
		{
			const FColor Color = Score.bRejected ? FColor::Red
				: (i == 0 ? FColor::Yellow : FColor::Green);
			DrawDebugSphere(Context.World, Candidates[i], 18.f, 6, Color, false, 0.f, 0, 1.2f);

			const FString Label = Score.bRejected
				? Score.DebugRejectReason.ToString()
				: FString::Printf(TEXT("%.2f"), Score.Total);
			DrawDebugString(Context.World, Candidates[i] + FVector(0, 0, 35.f), Label, nullptr, FColor::White, 0.f, true);
		}
#endif
	}

#if ENABLE_DRAW_DEBUG
	if (bDrawDebug && Context.World)
	{
		// 타겟: 보라. 선택된 자리: 굵은 흰 구. 리더 방향: 파란 선(PullToAlly가 끄는 쪽 확인용).
		DrawDebugSphere(Context.World, TargetLoc, 40.f, 12, FColor::Purple, false, 0.f, 0, 2.f);
		DrawDebugSphere(Context.World, Best.Location, 35.f, 12, FColor::White, false, 0.f, 0, 3.f);
		if (!Context.LeaderLocation.IsNearlyZero())
		{
			DrawDebugLine(Context.World, TargetLoc, Context.LeaderLocation, FColor::Blue, false, 0.f, 0, 2.f);
		}
	}
#endif

	// 환경보정(NavMesh·벽·슬로프)은 호출부(컴포넌트)가 수행한다.
	// 環境補正は呼び出し側。
	return Best.Location;
}

USlotGeneratorStrategy_RangedSafe::FCandidateScore USlotGeneratorStrategy_RangedSafe::ScoreCandidate(
	const FSlotGenContext& Context,
	const FVector& Candidate,
	const FVector& TargetLoc) const
{
	FCandidateScore Result;
	Result.Location = Candidate;

	// [1] 사거리 하드 필터 + 보상축. Band 밖이면 탈락.
	const float DistToTarget = FVector::Dist2D(Candidate, TargetLoc);
	const float RangeScore = ComputeRangeScore(DistToTarget, Context.AttackRange);
	if (RangeScore <= 0.f)
	{
		Result.bRejected = true;
		Result.DebugRejectReason = TEXT("OutOfRange");
		return Result;
	}

	// [2] 점유 하드 필터 + 소프트 벌점.
	bool bHardOccupied = false;
	const float OccupancyPenalty = ComputeOccupancyPenalty(Context, Candidate, bHardOccupied);
	if (bHardOccupied)
	{
		Result.bRejected = true;
		Result.DebugRejectReason = TEXT("Occupied");
		return Result;
	}

	// [3] 위협 벌점. 적 있는 쪽 후보를 거르는 주력 — 방향을 안 정해도 적을 피하게 만든다.
	const float ThreatPenalty = ComputeThreatPenalty(Context, Candidate);

	// [4] 아군 응집 가점. 후보가 360도라 "플레이어 쪽 후보"가 항상 존재 → 가점이 실제로 작동.
	const float PullScore = ComputePullToAllyScore(Candidate, Context);

	// [5] 가중 합산. 모든 축 [0,1]. 보상(Range·Pull) +, 벌점(Threat·Occupancy) -.
	Result.Total =
		  RangeScore        * RangeWeight
		- ThreatPenalty     * ThreatWeight
		- OccupancyPenalty  * OccupancyWeight
		+ PullScore         * PullToAllyWeight;

	Result.DebugRange     = RangeScore       * RangeWeight;
	Result.DebugThreat    = ThreatPenalty    * ThreatWeight;
	Result.DebugOccupancy = OccupancyPenalty * OccupancyWeight;
	Result.DebugPull      = PullScore        * PullToAllyWeight;

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

	// Band 안: Preferred에 가까울수록 1. 가까운 쪽/먼 쪽 폭이 다르므로 분모를 따로.
	// Band内：Preferredに近いほど高得点。両方向で分母を分ける。
	const float Denom = (DistToTarget <= PreferredRange)
		? FMath::Max(PreferredRange - MinRange, 1.f)
		: FMath::Max(MaxRange - PreferredRange, 1.f);

	return FMath::Clamp(1.f - FMath::Abs(DistToTarget - PreferredRange) / Denom, 0.f, 1.f);
}

float USlotGeneratorStrategy_RangedSafe::ComputeThreatPenalty(const FSlotGenContext& Context, const FVector& Candidate) const
{
	// 후보~각 적 거리² 수집 → 가까운 K명만 본다(배경 위협 절단).
	// 각 적 위협도를 soft saturation으로 [0,1]에: comfort²/(comfort²+dist²).
	//   dist=0 → 1, dist=comfort → 0.5, dist→∞ → 0. 1/d² 폭발 없어 floor 불필요.
	// K명을 확률적 OR로 합성: threat = 1 - Π(1 - danger_i). 항상 [0,1].
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
	// 군집 해소의 주력: SoftRadius를 크게 잡으면 동료끼리 멀리 흩어진다.
	// ソフト減点は最近接占有で[0,1]。SoftRadiusを大きくすると広く散る。
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

float USlotGeneratorStrategy_RangedSafe::ComputePullToAllyScore(
	const FVector& Candidate, const FSlotGenContext& Context) const
{
	// 리더 위치가 비었으면 중립 0.5 — 가점도 감점도 안 함.
	if (Context.LeaderLocation.IsNearlyZero())
	{
		return 0.5f;
	}

	// 후보가 리더(플레이어)에 가까울수록 1, 멀수록 0. 화면 밖(아주 멀면) 0 → 자연히 밀려난다.
	// 기준 거리는 AttackRange의 2배(대략 전선 폭). 단순 역거리 정규화.
	// 候補がリーダーに近いほど加点。画面外（遠い）は0で自然に脱落。
	const float DistToLeader = FVector::Dist2D(Candidate, Context.LeaderLocation);
	const float RefDist = FMath::Max(Context.AttackRange, 1.f) * 2.f;
	return FMath::Clamp(1.f - DistToLeader / RefDist, 0.f, 1.f);
}