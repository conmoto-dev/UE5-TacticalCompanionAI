// Fill out your copyright notice in the Description page of Project Settings.


#include "AI/Strategies/SlotGeneratorStrategy_Arc.h"


void USlotGeneratorStrategy_Arc::GenerateSlots(int32 NumSlots, float BaseRadius, TArray<FVector>& OutLocalOffsets) const
{
	OutLocalOffsets.Reset();
	if (NumSlots <= 0) return;

	OutLocalOffsets.Reserve(NumSlots);

	// 엣지 케이스: 1명이면 호 중심(로컬 +X)에 1개. (N-1 나눗셈 폭발 방지)
	// 1명일 땐 펼칠 호가 없으니 정면 한 점이 자연스럽다.
	if (NumSlots == 1)
	{
		OutLocalOffsets.Add(FVector::ForwardVector * BaseRadius); // (+X) * R
		return;
	}

	// N >= 2: [-ArcAngle/2, +ArcAngle/2]를 N등분 (양 끝 포함).
	// 호 중심 = 로컬 +X. XY 평면에서 +X로부터 angle 회전한 방향 × 반경.
	// Z=0 (높이는 이후 환경보정이 처리).
	const float HalfArc = ArcAngleDeg * 0.5f;
	const float Step = ArcAngleDeg / static_cast<float>(NumSlots - 1);

	for (int32 i = 0; i < NumSlots; ++i)
	{
		const float AngleDeg = -HalfArc + Step * static_cast<float>(i);
		const float AngleRad = FMath::DegreesToRadians(AngleDeg);

		// +X 기준 XY 평면 회전: X=cos, Y=sin. forward에서 좌우(±Y)로 벌어짐.
		const FVector Dir(FMath::Cos(AngleRad), FMath::Sin(AngleRad), 0.f);
		OutLocalOffsets.Add(Dir * BaseRadius);
	}
}