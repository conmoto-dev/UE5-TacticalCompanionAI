#include "AI/Components/CombatMicroMovementComponent.h"

#include "AIController.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "AI/Interfaces/HomeSlotProvider.h"
#include "AI/Targeting/TargetSelectorComponent.h"
#include "Navigation/PathFollowingComponent.h"
#include "Characters/PartyCharacter.h"
#include "DrawDebugHelpers.h"

// 미세 이동 디버그 표시. 홈 슬롯 반경(원) / 이동 목표(구+선) / 대기·정지 상태(색).
// 微細移動デバッグ表示。ホームスロット半径／移動目標／待機・停止状態。
static TAutoConsoleVariable<bool> CVarDebugMicroMovement(
	TEXT("TacticalAI.DebugMicroMovement"), false,
	TEXT("Show combat micro movement debug: home slot radius, strafe goal, state."));

UCombatMicroMovementComponent::UCombatMicroMovementComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UCombatMicroMovementComponent::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// [1] 동작 조건 검사. 실패 시 진행 중이던 이동도 중단 — MoveTo(재배치 등)와 공격이 항상 우선.
	// [1] 動作条件検査。失敗時は進行中の移動も中断 — MoveToと攻撃が常に優先。
	FVector HomeSlot;
	const AActor* Target = nullptr;
	const bool bActive = PassesActivationChecks(HomeSlot, Target);

#if ENABLE_DRAW_DEBUG
	if (CVarDebugMicroMovement.GetValueOnGameThread())
	{
		DrawDebug(bActive, HomeSlot);
	}
#endif

	if (!bActive)
	{
		if (bStrafing)
		{
			bStrafing = false;
			EndStrafeFacing();
		}
		return;
	}
	
	ACharacter* OwnerCharacter = Cast<ACharacter>(GetOwner());
	if (!OwnerCharacter) return;
	
	// [2] 대기 중: 타이머 소진 후 방향·지속시간 확정하고 이동 시작.
	// [2] 待機中：タイマー消化後、方向と持続時間を確定して移動開始。
	if (!bStrafing)
	{
		TimeUntilNextStrafe -= DeltaTime;
		if (TimeUntilNextStrafe > 0.f) return;

		TimeUntilNextStrafe = FMath::FRandRange(
			FMath::Min(StrafeIntervalRange.X, StrafeIntervalRange.Y),
			FMath::Max(StrafeIntervalRange.X, StrafeIntervalRange.Y));

		if (!TryPickStrafeDirection(HomeSlot, *Target, StrafeDirection)) return;

		// 이동으로 위치가 틀어지면 이동 명령 캐시(UpdateThreshold) 때문에
		// 다음 재배치 명령이 스킵될 수 있다 → 이동을 가로채는 기능의 기존 규약대로 캐시 무효화.
		// 位置がずれると移動命令キャッシュで次の再配置命令がスキップされうる → 既存規約通り無効化。
		if (APartyCharacter* PartyChar = Cast<APartyCharacter>(OwnerCharacter))
		{
			PartyChar->InvalidateMoveCache();
		}

		StrafeTimeRemaining = FMath::FRandRange(
			FMath::Min(StrafeDurationRange.X, StrafeDurationRange.Y),
			FMath::Max(StrafeDurationRange.X, StrafeDurationRange.Y));
		bStrafing = true;
		BeginStrafeFacing();
		return;
	}

	// [3] 이동 실행: 확정된 방향으로 지속시간 동안 걷는다. 중간에 자르지 않음 —
	//     반경 밖으로 나가도 완주하고, 되돌리기는 다음 이동의 방향 규칙이 담당한다.
	//     몸은 이동 방향이 아니라 타겟 방향 — 옆걸음/뒷걸음이 여기서 나온다.
	// [3] 移動実行：確定方向へ持続時間だけ歩く。途中で切らない。体はターゲット方向。
	StrafeTimeRemaining -= DeltaTime;
	if (StrafeTimeRemaining <= 0.f)
	{
		bStrafing = false;
		EndStrafeFacing();
		return;
	}

	OwnerCharacter->AddMovementInput(StrafeDirection, StrafeSpeedScale);

	const FVector MyLoc = OwnerCharacter->GetActorLocation();
	const FVector ToTarget = (Target->GetActorLocation() - MyLoc).GetSafeNormal2D();
	if (!ToTarget.IsNearlyZero())
	{
		const FRotator DesiredYaw(0.f, ToTarget.Rotation().Yaw, 0.f);
		const FRotator NewRotation = FMath::RInterpTo(
			OwnerCharacter->GetActorRotation(), DesiredYaw, DeltaTime, FaceTargetInterpSpeed);
		OwnerCharacter->SetActorRotation(NewRotation);
	}
}

bool UCombatMicroMovementComponent::PassesActivationChecks(FVector& OutHomeSlot, const AActor*& OutTarget) const
{
	// 검사 순서는 싼 것부터. 주석 번호는 판정 흐름도의 조건 순서와 대응.
	// 検査順は安い順。番号は判定フロー図の条件順と対応。

	// [1] 외부 일시 정지 (공격 중 등).
	if (bSuppressed) return false;

	const ACharacter* OwnerCharacter = Cast<ACharacter>(GetOwner());
	if (!OwnerCharacter) return false;

	// [2] AI 이동 명령이 없을 것. MoveTo(재배치·추종·트래버설) 진행 중이면 무조건 양보.
	//     이 검사 하나가 이동 명령 충돌의 해결책 전부 — 기존 경로가 항상 이긴다.
	// [2] AI移動命令が無いこと。MoveTo進行中は無条件で譲る。
	const AAIController* AIC = Cast<AAIController>(OwnerCharacter->GetController());
	if (!AIC) return false;
	if (AIC->GetMoveStatus() != EPathFollowingStatus::Idle) return false;

	// [3] 홈 슬롯 존재 (전투 모드 + 커밋 완료 — 판단은 캐릭터의 IHomeSlotProvider 구현부 소관).
	// [3] ホームスロット存在（判断はキャラのIHomeSlotProvider実装側）。
	const IHomeSlotProvider* HomeProvider = Cast<IHomeSlotProvider>(GetOwner());
	if (!HomeProvider) return false;
	if (!HomeProvider->TryGetHomeSlot(OutHomeSlot)) return false;

	// [4] 타겟 존재. 셀렉터는 추상 베이스 타입으로만 조회 — 진영 비의존 유지.
	// [4] ターゲット存在。セレクタは抽象基底型のみで照会。
	const APartyCharacter* PartyChar = Cast<APartyCharacter>(GetOwner());
	if (!PartyChar) return false;

	const UTargetSelectorComponent* Selector = PartyChar->GetTargetSelector();
	if (!Selector) return false;

	OutTarget = Selector->GetCurrentTarget();
	if (!OutTarget) return false;

	return true;
}

bool UCombatMicroMovementComponent::TryPickStrafeDirection(const FVector& HomeSlot,
	const AActor& Target, FVector& OutDirection) const
{
	const ACharacter* OwnerCharacter = Cast<ACharacter>(GetOwner());
	if (!OwnerCharacter) return false;

	const FVector MyLoc = OwnerCharacter->GetActorLocation();

	const FVector ToTarget = (Target.GetActorLocation() - MyLoc).GetSafeNormal2D();
	if (ToTarget.IsNearlyZero()) return false;

	// [1] 반경 밖 = 복귀 이동.
	//     "적 방향 ~ 홈 슬롯 방향" 사이 각 중 랜덤.
	//     부호 있는 사이각 × 랜덤 비율만큼 적 방향에서 회전 (비율 0 = 적 쪽, 1 = 홈 슬롯 쪽).
	// [1] 半径外＝復帰移動。「敵方向〜ホームスロット方向」の間の角からランダム。
	const float DistToHome = FVector::Dist2D(MyLoc, HomeSlot);
	if (DistToHome > HomeTolerance)
	{
		const FVector ToHome = (HomeSlot - MyLoc).GetSafeNormal2D();
		if (ToHome.IsNearlyZero()) return false;

		const float SignedSectorDeg = FMath::RadiansToDegrees(FMath::Atan2(
			FVector::CrossProduct(ToTarget, ToHome).Z,
			FVector::DotProduct(ToTarget, ToHome)));

		const float Alpha = FMath::FRand();
		OutDirection = ToTarget.RotateAngleAxis(SignedSectorDeg * Alpha, FVector::UpVector);
		return true;
	}

	// [2] 반경 안 = 자유 이동: 적 방향 0도 기준, 크기 50~100도 랜덤 × 좌/우 랜덤.
	//     하한이 적 정면 직진(타겟을 뚫는 방향)을 제외한다.
	// [2] 半径内＝自由移動。下限が敵正面への直進を除外する。
	const float AngleDeg = FMath::FRandRange(FreeMoveAngleMinDeg, FreeMoveAngleMaxDeg)
		* (FMath::RandBool() ? 1.f : -1.f);
	OutDirection = ToTarget.RotateAngleAxis(AngleDeg, FVector::UpVector);
	return true;
}

void UCombatMicroMovementComponent::BeginStrafeFacing()
{
	// 이동 방향 자동 회전을 끄고 타겟 방향 고정으로. 원본 값 저장 → 종료 시 복원 (캐릭터 전역 설정 존중).
	// 移動方向の自動回転を切りターゲット方向固定へ。原本保存→終了時復元。
	ACharacter* OwnerCharacter = Cast<ACharacter>(GetOwner());
	if (!OwnerCharacter) return;

	UCharacterMovementComponent* MoveComp = OwnerCharacter->GetCharacterMovement();
	if (!MoveComp) return;

	bSavedOrientRotationToMovement = MoveComp->bOrientRotationToMovement;
	MoveComp->bOrientRotationToMovement = false;
}

void UCombatMicroMovementComponent::EndStrafeFacing()
{
	ACharacter* OwnerCharacter = Cast<ACharacter>(GetOwner());
	if (!OwnerCharacter) return;

	UCharacterMovementComponent* MoveComp = OwnerCharacter->GetCharacterMovement();
	if (!MoveComp) return;

	MoveComp->bOrientRotationToMovement = bSavedOrientRotationToMovement;
}

void UCombatMicroMovementComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// 이동 도중 파괴/제거 대비 — 회전 방식 원상 복구.
	// 移動中の破棄に備え、回転方式を復元。
	if (bStrafing)
	{
		EndStrafeFacing();
	}
	Super::EndPlay(EndPlayReason);
}


#if ENABLE_DRAW_DEBUG
void UCombatMicroMovementComponent::DrawDebug(bool bActive, const FVector& HomeSlot) const
{
	const ACharacter* OwnerCharacter = Cast<ACharacter>(GetOwner());
	if (!OwnerCharacter) return;

	UWorld* World = GetWorld();
	if (!World) return;

	// [1] 홈 슬롯 반경. 흰색 = 검사 통과(동작 가능), 회색 = 어떤 검사에서 정지 중.
	//     HomeSlot이 무효(검사 [3] 이전 실패)면 좌표가 쓰레기이므로 반경은 통과 시에만.
	// [1] ホームスロット半径。白＝検査通過、灰＝停止中。座標が有効な時のみ描画。
	if (bActive)
	{
		DrawDebugCircle(World, HomeSlot + FVector(0, 0, 5.f), HomeTolerance, 32,
			FColor::White, false, -1.f, 0, 1.5f, FVector::ForwardVector, FVector::RightVector);
		DrawDebugSphere(World, HomeSlot, 12.f, 8, FColor::White, false, -1.f, 0, 1.5f);
	}

	// [2] 이동 중: 이동 방향 화살표 (남은 시간 비례 길이). 대기 중: 머리 위 남은 대기 시간.
	// [2] 移動中：移動方向の矢印。待機中：頭上に残り待機時間。
	const FVector MyLoc = OwnerCharacter->GetActorLocation();
	if (bStrafing)
	{
		DrawDebugDirectionalArrow(World, MyLoc,
			MyLoc + StrafeDirection * (100.f + StrafeTimeRemaining * 60.f),
			60.f, FColor::Cyan, false, -1.f, 0, 2.f);
	}
	else if (bActive)
	{
		DrawDebugString(World, MyLoc + FVector(0, 0, 120.f),
			FString::Printf(TEXT("next strafe %.1fs"), FMath::Max(TimeUntilNextStrafe, 0.f)),
			nullptr, FColor::Cyan, 0.f, true);
	}
}
#endif