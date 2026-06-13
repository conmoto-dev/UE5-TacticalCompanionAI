// Fill out your copyright notice in the Description page of Project Settings.


#include "AI/Strategies/SlotGeneratorStrategy_Arc.h"


FVector USlotGeneratorStrategy_Arc::GenerateSlot(const FSlotGenContext& Context) const
{
	// 엣지 케이스: 1명이면 호 중심(로컬 +X)에 배치. (N-1 나눗셈 폭발 방지)
	// 1명일 땐 펼칠 호가 없으니 정면 한 점이 자연스럽다.
	// 1人なら展開するアークが無いので正面1点。
	if (Context.TotalSlots <= 1)
	{
		return Context.Anchor.TransformPosition(FVector::ForwardVector * Context.BaseRadius);
	}

	// N >= 2: [-ArcAngle/2, +ArcAngle/2]를 N등분(양 끝 포함)한 중 SlotIndex번째 각도.
	// 호 중심 = 로컬 +X. XY 평면에서 +X로부터 angle 회전한 방향 × 반경. Z=0(높이는 환경보정).
	// 순회는 컴포넌트가 하므로 여기선 SlotIndex 한 점만 계산.
	// 巡回は呼び出し側。ここではSlotIndexの1点のみ算出。
	const float HalfArc = ArcAngleDeg * 0.5f;
	const float Step = ArcAngleDeg / static_cast<float>(Context.TotalSlots - 1);
	const float AngleDeg = -HalfArc + Step * static_cast<float>(Context.SlotIndex);
	const float AngleRad = FMath::DegreesToRadians(AngleDeg);

	// +X 기준 XY 평면 회전: X=cos, Y=sin. forward에서 좌우(±Y)로 벌어짐.
	const FVector Direction(FMath::Cos(AngleRad), FMath::Sin(AngleRad), 0.f);
	return Context.Anchor.TransformPosition(Direction * Context.BaseRadius);
}