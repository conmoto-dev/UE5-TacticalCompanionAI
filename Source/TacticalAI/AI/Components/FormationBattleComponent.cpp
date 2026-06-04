#include "AI/Components/FormationBattleComponent.h"
#include "AI/Strategies/SlotGeneratorStrategy.h"
#include "Enemies/TargetDummy.h"   // TODO: 인터페이스로 분리 (구현체 2개째 생기면)
#include "DrawDebugHelpers.h"
#include "NavigationSystem.h"
#include "Characters/PartyCharacter.h"

UFormationBattleComponent::UFormationBattleComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}
void UFormationBattleComponent::BeginPlay()
{
	Super::BeginPlay();
	CurrentTarget = DebugTargetActor;
}

void UFormationBattleComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// [a] anchor 획득. 타겟 소멸 시 미반환 → 파이프라인 정지 (크래시 방지).
	const TOptional<FTransform> AnchorOpt = GetFormationAnchor();
	if (!AnchorOpt.IsSet()) return;
	if (!SlotGenerator) return;

	const FTransform& Anchor = AnchorOpt.GetValue();
	const FVector AnchorOrigin = Anchor.GetLocation();
	
	// [b] 로컬 슬롯 생성 (Strategy — 월드·타겟 무지).
	TArray<FVector> LocalOffsets;
	SlotGenerator->GenerateSlots(NumSlots, ComputeBaseRadius(), LocalOffsets);

	// [c] 로컬→월드 + 환경보정 + push.
	for (int32 i = 0; i < LocalOffsets.Num(); ++i)
	{
		const FVector World = Anchor.TransformPosition(LocalOffsets[i]);
		const FVector Adjusted = AdjustLocationForEnvironment(World, AnchorOrigin);

		// push: 인덱스순 단순 배정 (Hungarian은 스텝 9). 동료 부족하면 스킵.
		if (DebugCompanions.IsValidIndex(i) && DebugCompanions[i])
		{
			DebugCompanions[i]->UpdateTargetSlotLocation(Adjusted, false);
		}

		// [임시 드로우] 스텝 후반에 제거.
		DrawDebugSphere(GetWorld(), Adjusted, 30.f, 12, FColor::Cyan, false, -1.f, 0, 2.f);
	}

	// [임시] anchor forward 시각화.
	DrawDebugDirectionalArrow(GetWorld(), AnchorOrigin,
		AnchorOrigin + Anchor.GetRotation().GetForwardVector() * 150.f,
		60.f, FColor::Red, false, -1.f, 0, 3.f);
}



TOptional<FTransform> UFormationBattleComponent::GetFormationAnchor() const
{
	// 약참조 유효성 = 타겟 살아있음. 죽었으면 빈 Optional → 호출부가 파이프라인 정지.
	if (const AActor* Target = CurrentTarget.Get())
	{
		return Target->GetActorTransform();
	}
	return TOptional<FTransform>();
}

float UFormationBattleComponent::ComputeBaseRadius() const
{
	float TargetRadius = 0.f;

	// TODO: ATargetDummy 직접 캐스트 = 결합. 진짜 적 추가 시 IEncircleTarget 인터페이스로 분리.
	//       지금은 구현체 하나라 조기 추상화 회피 (캐스트 유지).
	if (const ATargetDummy* Dummy = Cast<ATargetDummy>(CurrentTarget.Get()))
	{
		TargetRadius = Dummy->GetEncircleRadius();
	}

	return DesignerBaseRadius + TargetRadius;
}

// === 환경보정 파이프라인 (Follow 복사. 공통 부모 추출 시 통합 예정.) ===

FVector UFormationBattleComponent::AdjustLocationForEnvironment(const FVector& IdealLocation, const FVector& AnchorOrigin) const
{
	// [1] 슬로프 Z 보정.
	FVector AdjustedIdeal = IdealLocation;
	float GroundZ;
	if (TryFindGroundZ(IdealLocation, GroundZ))
	{
		AdjustedIdeal.Z = GroundZ;
	}

	// [2] NavMesh 투영 (1차 진실).
	FVector NavResult;
	if (TryProjectToNavMesh(AdjustedIdeal, NavResult))
	{
		return NavResult;
	}

	// [3] 벽 슬라이드 (대체 탐색). 기준점 = anchor 원점.
	FVector SlidLocation;
	if (TryCalculateWallSlide(AnchorOrigin, AdjustedIdeal, SlidLocation))
	{
		if (TryProjectToNavMesh(SlidLocation, NavResult))
		{
			return NavResult;
		}
	}

	// [4] 전부 실패 → anchor 쪽으로 끌어당김.
	return CalculateFallbackLocation(AnchorOrigin, IdealLocation);
}

bool UFormationBattleComponent::TryProjectToNavMesh(const FVector& Point, FVector& OutResult) const
{
	UNavigationSystemV1* NavSys = UNavigationSystemV1::GetCurrent(GetWorld());
	if (!NavSys) return false;
	FNavLocation ProjectedLoc;
	if (NavSys->ProjectPointToNavigation(Point, ProjectedLoc, FVector(50.f, 50.f, 250.f)))
	{
		OutResult = ProjectedLoc.Location;
		return true;
	}
	return false;
}

bool UFormationBattleComponent::TryFindGroundZ(const FVector& Point, float& OutZ) const
{
	const FVector TraceStart = Point + FVector(0.f, 0.f, 500.f);
	const FVector TraceEnd   = Point - FVector(0.f, 0.f, 500.f);

	FHitResult Hit;
	FCollisionQueryParams Params; // Battle엔 ignore할 자기 leader 없음.
	if (GetWorld()->LineTraceSingleByChannel(Hit, TraceStart, TraceEnd, ECC_Visibility, Params))
	{
		OutZ = Hit.Location.Z;
		return true;
	}
	return false;
}

bool UFormationBattleComponent::TryCalculateWallSlide(const FVector& From, const FVector& To, FVector& OutSlidLocation) const
{
	const float ChestHeight = 90.0f;
	const FVector TraceStart = From + FVector(0.f, 0.f, ChestHeight);
	const FVector TraceEnd   = To   + FVector(0.f, 0.f, ChestHeight);

	FHitResult HitResult(ForceInit);
	FCollisionQueryParams QueryParams;
	const FCollisionShape SphereShape = FCollisionShape::MakeSphere(40.0f);
	const bool bHit = GetWorld()->SweepSingleByChannel(HitResult, TraceStart, TraceEnd, FQuat::Identity, ECC_Visibility, SphereShape, QueryParams);

	if (!bHit) return false;

	const float WallThreshold = 0.7f;
	if (FMath::Abs(HitResult.ImpactNormal.Z) >= WallThreshold) return false;

	FVector HitLoc2D = HitResult.Location; HitLoc2D.Z = 0.f;
	FVector To2D = To; To2D.Z = 0.f;
	FVector Normal2D = HitResult.ImpactNormal; Normal2D.Z = 0.f; Normal2D.Normalize();

	const FVector RemainingDir = To2D - HitLoc2D;
	const FVector SlidingDir = FVector::VectorPlaneProject(RemainingDir, Normal2D);

	const float SafeMargin = 70.0f;
	OutSlidLocation = HitLoc2D + SlidingDir + (Normal2D * SafeMargin);
	OutSlidLocation.Z = To.Z;
	return true;
}

FVector UFormationBattleComponent::CalculateFallbackLocation(const FVector& AnchorOrigin, const FVector& IdealLocation) const
{
	const float TowDistance = 150.0f;
	const FVector DirToAnchor = (AnchorOrigin - IdealLocation).GetSafeNormal2D();
	return IdealLocation + (DirToAnchor * TowDistance);
}