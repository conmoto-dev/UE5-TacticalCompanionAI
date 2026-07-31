#include "AI/Strategies/SlotGeneratorStrategy_Encircle.h"

#include "DrawDebugHelpers.h"
#include "GameFramework/Actor.h"

FVector USlotGeneratorStrategy_Encircle::GenerateSlot(const FSlotGenContext& Context) const
{
	// [1] 링 중심 = 타겟, 반경 = BaseRadius(타겟 크기 포함 이격) + 자기 사거리 × 비율.
	//     판정(ShouldReposition)도 같은 BaseRadius를 빼고 보므로 생성·판정이 동일 좌표계 —
	//     "커밋 즉시 사거리 이탈" 재커밋 루프가 구조적으로 불가능.
	// 中心＝ターゲット、半径＝BaseRadius＋射程×比率。生成と判定が同座標系のため再コミットループ不可。
	const AActor* Target = Context.PrimaryTarget.Get();
	const FVector TargetLoc = Target ? Target->GetActorLocation() : Context.Anchor.GetLocation();
	const float Radius = Context.AttackRange * PreferredRangeRatio + Context.BaseRadius;
	
	
	// [2] 희망 각도 = 내 접근 방향(타겟→나). 타겟 facing과 무관 — 적이 회전해도 불변.
	//     퇴화(타겟과 겹침) 시 리더 쪽 폴백 — 아군 진영 쪽에서 진입하는 게 자연스럽다.
	// 希望角度＝自分の接近方向（ターゲット→自分）。ターゲットの向きとは無関係。
	FVector ApproachDir = (Context.RequesterLocation - TargetLoc).GetSafeNormal2D();
	if (ApproachDir.IsNearlyZero())
	{
		ApproachDir = (Context.LeaderLocation - TargetLoc).GetSafeNormal2D();
	}
	if (ApproachDir.IsNearlyZero())
	{
		ApproachDir = FVector::ForwardVector;
	}
	float SlotAngleDeg = FMath::RadiansToDegrees(FMath::Atan2(ApproachDir.Y, ApproachDir.X));

	// [3] 링 위 점유 각도 수집. 링 반경 ± margin 밖(다른 타겟·원거리 커밋)은 경쟁 불참.
	// リング上の占有角度を収集。半径±margin外は競合外。
	TArray<float> OccupiedAngles;
	for (const FVector& Occupied : Context.OccupiedSlots)
	{
		if (FMath::Abs(FVector::Dist2D(Occupied, TargetLoc) - Radius) > RingOccupancyMargin) continue;

		const FVector ToOccupied = (Occupied - TargetLoc).GetSafeNormal2D();
		if (ToOccupied.IsNearlyZero()) continue;

		OccupiedAngles.Add(FMath::RadiansToDegrees(FMath::Atan2(ToOccupied.Y, ToOccupied.X)));
	}

	// [4] 각도 해소: 간격 위반 시 "가장 가까운 충돌 상대의 반대쪽"으로 필요한 만큼만 민다.
	//     기존 점유는 절대 안 움직임 — 새로 들어오는 쪽(나)만 비켜 들어간다.
	//     밀린 곳에서 다른 점유와 또 충돌하면 다음 반복이 처리. 상한 도달 시 마지막 각도 수용.
	// 角度解消：最近接の占有の反対側へ必要分だけずれる。既存占有は不動、新入りが避ける。
	constexpr int32 MaxResolveIterations = 8;
	for (int32 Iteration = 0; Iteration < MaxResolveIterations; ++Iteration)
	{
		bool bConflict = false;
		float NearestAbs = MinSeparationDeg;
		float NearestDelta = 0.f;   // 부호 있는 각도차 (나 → 충돌 상대)

		for (const float OccupiedAngle : OccupiedAngles)
		{
			const float Delta = FMath::FindDeltaAngleDegrees(SlotAngleDeg, OccupiedAngle);
			if (FMath::Abs(Delta) < NearestAbs)
			{
				NearestAbs = FMath::Abs(Delta);
				NearestDelta = Delta;
				bConflict = true;
			}
		}
		if (!bConflict) break;

		// 충돌 상대의 반대 방향으로, 간격이 딱 확보될 만큼만 이동.
		const float PushDir = (NearestDelta >= 0.f) ? -1.f : 1.f;
		SlotAngleDeg += PushDir * (MinSeparationDeg - NearestAbs);
	}
	SlotAngleDeg = FRotator::NormalizeAxis(SlotAngleDeg);

	// [5] 최종 슬롯. Z는 타겟 기준 — 높이 보정은 호출부 환경보정(NavMesh) 소관.
	const float AngleRad = FMath::DegreesToRadians(SlotAngleDeg);
	const FVector Slot = TargetLoc + FVector(FMath::Cos(AngleRad), FMath::Sin(AngleRad), 0.f) * Radius;

#if ENABLE_DRAW_DEBUG
	if (bDrawDebug && Context.World)
	{
		// 커밋 시점에만 호출되므로 지속시간을 줘서 남긴다.
		// 접근 방향(노랑 선) / 해소된 슬롯(흰 구) / 경쟁 점유(주황 구).
		DrawDebugLine(Context.World, TargetLoc, TargetLoc + ApproachDir * Radius, FColor::Yellow, false, 2.f, 0, 1.5f);
		DrawDebugSphere(Context.World, Slot, 25.f, 10, FColor::White, false, 2.f, 0, 2.f);
		for (const float OccupiedAngle : OccupiedAngles)
		{
			const float Rad = FMath::DegreesToRadians(OccupiedAngle);
			DrawDebugSphere(Context.World,
				TargetLoc + FVector(FMath::Cos(Rad), FMath::Sin(Rad), 0.f) * Radius,
				18.f, 8, FColor::Orange, false, 2.f, 0, 1.2f);
		}
	}
#endif

	return Slot;
}