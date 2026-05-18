// Fill out your copyright notice in the Description page of Project Settings.


#include "AI/Strategies/YieldStrategy_Standard.h"
#include "AI/Strategies/YieldContextProvider.h"
#include "Characters/PartyCharacter.h"
#include "GameFramework/Pawn.h"
#include "NavigationSystem.h"
#include "DrawDebugHelpers.h"

#if WITH_EDITOR
void UYieldStrategy_Standard::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	// Yield hysteresis: Exit >= Enter 強制。
	if (YieldExitRadius < YieldEnterRadius)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Yield] YieldExitRadius (%.0f) < YieldEnterRadius (%.0f). Auto-corrected."),
			YieldExitRadius, YieldEnterRadius);
		YieldExitRadius = YieldEnterRadius;
	}
}
#endif

bool UYieldStrategy_Standard::TryProjectToNavMesh(UWorld* World, const FVector& Point, FVector& OutResult) const
{
	if (!World) return false;
	
	UNavigationSystemV1* NavSys = UNavigationSystemV1::GetCurrent(World);
	if (!NavSys) return false;

	FNavLocation ProjectedLoc;
	if (NavSys->ProjectPointToNavigation(Point, ProjectedLoc, FVector(50.f, 50.f, 250.f)))
	{
		OutResult = ProjectedLoc.Location;
		return true;
	}
	return false;
}

bool UYieldStrategy_Standard::ShouldYieldForSlot(
	const TScriptInterface<IYieldContextProvider>& Context, int32 SlotIdx) const
{
	if (!Context) return false;

	APartyCharacter* Occupant = Context->GetOccupantAt(SlotIdx);
	if (!Occupant) return false;

	APawn* Target = Context->GetTargetPawn();
	if (!Target) return false;

	// [1] Distance check (3D; height-distant occupants get filtered here).
	// 距離チェック（3D）。高低差のあるoccupantはここでフィルタ。
	const FVector OccupantLoc = Occupant->GetActorLocation();
	const FVector TargetLoc = Target->GetActorLocation();
	const FVector TargetToOccupant = OccupantLoc - TargetLoc;
	if (TargetToOccupant.SizeSquared() > FMath::Square(YieldEnterRadius)) return false;
	
	// [2] Cone check on horizontal plane based on target's facing (not velocity).
	// Camera direction decoupled in this game; only body orientation matters.
	// ターゲットの向きでcone判定。カメラ方向は意図的に未使用。
	const FVector TargetForward  = Target->GetActorForwardVector();
	const FVector TargetDirFlat  = FVector(TargetForward.X, TargetForward.Y, 0.f).GetSafeNormal();
	const FVector ToOccupantFlat = FVector(TargetToOccupant.X, TargetToOccupant.Y, 0.f).GetSafeNormal();

	// Compare cosines instead of angles to avoid acos() cost.
	// acos回避のためコサイン値で比較。
	const float CosAngle = FVector::DotProduct(TargetDirFlat, ToOccupantFlat);
	const float CosThreshold = FMath::Cos(FMath::DegreesToRadians(YieldConeHalfAngleDeg));
	return CosAngle >= CosThreshold;
}

bool UYieldStrategy_Standard::ShouldExitYieldForSlot(
	const TScriptInterface<IYieldContextProvider>& Context, int32 SlotIdx) const
{
	// Exit defaults to TRUE on missing data (recover to Following safely).
	// データ欠損時はTRUE（安全にFollowingへ復帰）。
	if (!Context) return true;

	APartyCharacter* Occupant = Context->GetOccupantAt(SlotIdx);
	if (!Occupant) return true;

	APawn* Target = Context->GetTargetPawn();
	if (!Target) return true;

	// Distance-only check (no cone). Asymmetric with Enter is the intended hysteresis.
	// 距離のみ（コーン無し）。Enterとの非対称性が意図的なヒステリシス。
	const FVector TargetToOccupant = Occupant->GetActorLocation() - Target->GetActorLocation();
	return TargetToOccupant.SizeSquared() > FMath::Square(YieldExitRadius);
}

bool UYieldStrategy_Standard::TryCalculateYieldLocationForSlot(
	const TScriptInterface<IYieldContextProvider>& Context, int32 SlotIdx, FVector& OutLocation) const
{
	if (!Context) return false;

	APartyCharacter* Occupant = Context->GetOccupantAt(SlotIdx);
	if (!Occupant) return false;

	APawn* Target = Context->GetTargetPawn();
	if (!Target) return false;

	// [1] Target travel direction (horizontal only).
	// ShouldYieldForSlot's cone check rejects zero-facing case, so safe here.
	// ターゲット進行方向（水平のみ）。ShouldYieldで弾かれるためここではzero不可。
	const FVector TargetVelocity = Target->GetVelocity();
	const FVector TargetForward = Target->GetActorForwardVector();
	const FVector TargetDirFlat = FVector(TargetForward.X, TargetForward.Y, 0.f).GetSafeNormal();

	const FVector OccupantLoc = Occupant->GetActorLocation();
	const FVector TargetLoc = Target->GetActorLocation();

	// [2] Side direction perpendicular to target facing.
	// Correct right vector (UE: Right = Up × Forward).
	// UEは Up × Forward = Right。
	const FVector SideDir = FVector::CrossProduct(FVector::UpVector, TargetDirFlat);

	// [3] Backward component: project target velocity onto target→occupant direction.
	// Absolute scalar (not normalized) → walking vs running produces visible intensity diff.
	// ターゲット速度をターゲット→occupant方向に射影。絶対値で歩き/走りの強度差を演出。
	const FVector TargetToOccupantDir = (OccupantLoc - TargetLoc).GetSafeNormal();
	const float TowardSpeed = FVector::DotProduct(TargetVelocity, TargetToOccupantDir);
	const FVector BackwardOffset = TargetDirFlat * TowardSpeed * YieldBackwardFactor;

	const FVector CandidateRight = OccupantLoc + SideDir * YieldSideDistance + BackwardOffset;
	const FVector CandidateLeft  = OccupantLoc - SideDir * YieldSideDistance + BackwardOffset;

	// [4] Choose the side that moves AWAY from target's path (not toward slot).
	// Player avoidance is the intent; slot return cost is secondary.
	// プレイヤー経路から「離れる」側を優先。
	const FVector TargetToOccupant = OccupantLoc - TargetLoc;
	const float SideSign = FVector::DotProduct(TargetToOccupant, SideDir);

	const FVector FirstChoice  = (SideSign >= 0) ? CandidateRight : CandidateLeft;
	const FVector SecondChoice = (SideSign >= 0) ? CandidateLeft  : CandidateRight;
	
	// [5] NavMesh validation (yield where reachable; give up if not).
	// Debug spheres show the chosen candidate (red = first, blue = second).
	// NavMesh検証。退避不可ならfalse。
	UWorld* World = Occupant->GetWorld();
	if (TryProjectToNavMesh(World, FirstChoice, OutLocation))
	{
		DrawDebugSphere(Occupant->GetWorld(), OutLocation, 20.f, 8, FColor::Red, false, 1.0f);
		return true;
	}
	if (TryProjectToNavMesh(World, SecondChoice, OutLocation))
	{
		DrawDebugSphere(Occupant->GetWorld(), OutLocation, 20.f, 8, FColor::Blue, false, 1.0f);
		return true;
	}
	return false;
}