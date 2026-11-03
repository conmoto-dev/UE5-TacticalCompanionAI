#include "AI/Components/CombatMicroMovementComponent.h"

#include "AIController.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "AI/Interfaces/HomeSlotProvider.h"
#include "AI/Targeting/TargetSelectorComponent.h"
#include "Navigation/PathFollowingComponent.h"
#include "Characters/PartyCharacter.h"
#include "DrawDebugHelpers.h"

// 미세 이동 디버그 표시. 홈 슬롯 반경 / 이동 방향 / 개별 이동 실행 상태.
// 微細移動デバッグ表示。ホームスロット半径／移動方向／個別移動の実行状態。
static TAutoConsoleVariable<bool> CVarDebugMicroMovement(
	TEXT("TacticalAI.DebugMicroMovement"), false,
	TEXT("Show combat micro movement debug: home slot radius, strafe direction, execution state."));

UCombatMicroMovementComponent::UCombatMicroMovementComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UCombatMicroMovementComponent::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// [1] 동작 조건을 잃으면 이동과 주시를 중단한다.
	// [1] 動作条件を失った場合は移動と注視を中断する。
	FVector HomeSlot = FVector::ZeroVector;
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
		CancelStrafe();
		EndCombatFacing();
		return;
	}

	ACharacter* OwnerCharacter = Cast<ACharacter>(GetOwner());
	if (!OwnerCharacter) return;

	// [2] 이동 여부와 관계없이 조건을 만족하는 동안 대상을 주시한다.
	// [2] 移動の有無に関わらず、条件を満たす間は対象を注視する。
	if (!bCombatFacing)
	{
		BeginCombatFacing();
	}

	const FVector MyLoc = OwnerCharacter->GetActorLocation();
	const FVector ToTarget = (Target->GetActorLocation() - MyLoc).GetSafeNormal2D();
	if (!ToTarget.IsNearlyZero())
	{
		const FRotator DesiredYaw(0.f, ToTarget.Rotation().Yaw, 0.f);
		const FRotator NewRotation = FMath::RInterpTo(
			OwnerCharacter->GetActorRotation(), DesiredYaw, DeltaTime, FaceTargetInterpSpeed);
		OwnerCharacter->SetActorRotation(NewRotation);
	}

	// [3] 외부에서 요청한 개별 이동만 실행한다.
	// [3] 外部から要求された個別移動のみ実行する。
	if (StrafeState != ECombatStrafeState::Running) return;

	StrafeTimeRemaining -= DeltaTime;
	if (StrafeTimeRemaining <= 0.f)
	{
		FinishStrafe(true);
		return;
	}

	OwnerCharacter->AddMovementInput(StrafeDirection, StrafeSpeedScale);
}

// =======================================================
// 횡이동 한 번 시작
// 실행 중인 이동은 교체하지 않는다.
// 방향과 지속시간은 요청이 성공할 때 확정한다.
//
// 一回の横移動を開始する。
// 実行中の移動は置き換えない。
// 方向と持続時間は要求成功時に確定する。
// =======================================================
bool UCombatMicroMovementComponent::StartStrafe()
{
	// [1] 중복 요청과 동작 조건을 검사한다.
	// [1] 重複要求と動作条件を検証する。
	if (StrafeState == ECombatStrafeState::Running) return false;

	FVector HomeSlot = FVector::ZeroVector;
	const AActor* Target = nullptr;
	if (!PassesActivationChecks(HomeSlot, Target)) return false;

	// [2] 현재 홈 슬롯과 대상을 기준으로 이동 방향을 선정한다.
	// [2] 現在のホームスロットと対象を基準に移動方向を選択する。
	FVector NewDirection = FVector::ZeroVector;
	if (!TryPickStrafeDirection(HomeSlot, *Target, NewDirection)) return false;

	StrafeDirection = NewDirection;
	StrafeTimeRemaining = FMath::FRandRange(
		FMath::Min(StrafeDurationRange.X, StrafeDurationRange.Y),
		FMath::Max(StrafeDurationRange.X, StrafeDurationRange.Y));
	StrafeState = ECombatStrafeState::Running;

	// [3] 次の再配置が以前の移動命令キャッシュによって省略されないようにする。
	// [3] 다음 재배치가 이전 이동 명령 캐시 때문에 생략되지 않도록 한다.
	if (APartyCharacter* PartyChar = Cast<APartyCharacter>(GetOwner()))
	{
		PartyChar->InvalidateMoveCache();
	}

	return true;
}

void UCombatMicroMovementComponent::CancelStrafe()
{
	if (StrafeState != ECombatStrafeState::Running) return;

	FinishStrafe(false);
}

// =======================================================
// 개별 횡이동 종료
// 이동 입력 제공을 끝내고 결과를 보관한다.
// 주시 조건이 유지되면 이동 종료 후에도 대상을 바라본다.
//
// 個別の横移動を終了する。
// 移動入力の供給を終了し、結果を保持する。
// 注視条件が維持されていれば、移動終了後も対象を向く。
// =======================================================
void UCombatMicroMovementComponent::FinishStrafe(bool bCompleted)
{
	StrafeState = bCompleted
		? ECombatStrafeState::Completed
		: ECombatStrafeState::Interrupted;

	StrafeDirection = FVector::ZeroVector;
	StrafeTimeRemaining = 0.f;
}

void UCombatMicroMovementComponent::SetSuppressed(bool bInSuppressed)
{
	bSuppressed = bInSuppressed;

	if (bSuppressed)
	{
		CancelStrafe();
		EndCombatFacing();
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

	// [2] 반경 안 = 자유 이동: 적 방향 0도 기준, 설정된 각도 범위에서 좌우 방향을 선정한다.
	//     하한은 적 정면으로 직진하는 방향을 제외한다.
	// [2] 半径内＝自由移動。敵方向を0度とし、設定された角度範囲から左右の方向を選択する。
	//     下限は敵正面へ直進する方向を除外する。
	const float AngleDeg = FMath::FRandRange(FreeMoveAngleMinDeg, FreeMoveAngleMaxDeg)
		* (FMath::RandBool() ? 1.f : -1.f);
	OutDirection = ToTarget.RotateAngleAxis(AngleDeg, FVector::UpVector);
	return true;
}

void UCombatMicroMovementComponent::BeginCombatFacing()
{
	// 이동 방향 자동 회전을 끄고 타겟 방향 고정으로. 원본 값 저장 → 종료 시 복원 (캐릭터 전역 설정 존중).
	// 移動方向の自動回転を切りターゲット方向固定へ。原本保存→終了時復元。
	ACharacter* OwnerCharacter = Cast<ACharacter>(GetOwner());
	if (!OwnerCharacter) return;

	UCharacterMovementComponent* MoveComp = OwnerCharacter->GetCharacterMovement();
	if (!MoveComp) return;

	bSavedOrientRotationToMovement = MoveComp->bOrientRotationToMovement;
	MoveComp->bOrientRotationToMovement = false;

	bCombatFacing = true;
}

void UCombatMicroMovementComponent::EndCombatFacing()
{
	if (!bCombatFacing) return;

	if (ACharacter* OwnerCharacter = Cast<ACharacter>(GetOwner()))
	{
		if (UCharacterMovementComponent* MoveComp = OwnerCharacter->GetCharacterMovement())
		{
			MoveComp->bOrientRotationToMovement = bSavedOrientRotationToMovement;
		}
	}

	bCombatFacing = false;
}

void UCombatMicroMovementComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// 실행 중인 이동을 중단하고 회전 설정을 복원한다.
	// 実行中の移動を中断し、回転設定を復元する。
	CancelStrafe();
	EndCombatFacing();

	Super::EndPlay(EndPlayReason);
}

#if ENABLE_DRAW_DEBUG
void UCombatMicroMovementComponent::DrawDebug(bool bActive, const FVector& HomeSlot) const
{
	const ACharacter* OwnerCharacter = Cast<ACharacter>(GetOwner());
	if (!OwnerCharacter) return;

	UWorld* World = GetWorld();
	if (!World) return;

	// [1] 홈 슬롯 반경.
	// [1] ホームスロット半径。座標が有効な時のみ描画。
	if (bActive)
	{
		DrawDebugCircle(World, HomeSlot + FVector(0, 0, 5.f), HomeTolerance, 32,
			FColor::White, false, -1.f, 0, 1.5f, FVector::ForwardVector, FVector::RightVector);
		DrawDebugSphere(World, HomeSlot, 12.f, 8, FColor::White, false, -1.f, 0, 1.5f);
	}

	// [2] 이동 중에는 방향을, 그 외에는 최근 개별 이동의 실행 상태를 표시한다.
	// [2] 移動中は方向を、それ以外は直近の個別移動の実行状態を表示する。
	const FVector MyLoc = OwnerCharacter->GetActorLocation();
	if (StrafeState == ECombatStrafeState::Running)
	{
		DrawDebugDirectionalArrow(World, MyLoc,
			MyLoc + StrafeDirection * (100.f + StrafeTimeRemaining * 60.f),
			60.f, FColor::Cyan, false, -1.f, 0, 2.f);
	}
	else if (bActive)
	{
		DrawDebugString(World, MyLoc + FVector(0, 0, 120.f),
			UEnum::GetValueAsString(StrafeState),
			nullptr, FColor::Cyan, 0.f, true);
	}
}
#endif