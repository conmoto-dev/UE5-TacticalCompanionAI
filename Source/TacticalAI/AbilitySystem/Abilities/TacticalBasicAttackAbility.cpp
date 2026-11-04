#include "AbilitySystem/Abilities/TacticalBasicAttackAbility.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystem/Attributes/TacticalCombatAttributeSet.h"
#include "AbilitySystem/Tags/TacticalGameplayTags.h"
#include "AI/Targeting/TargetSelectorComponent.h"
#include "AI/Targeting/Targetable.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "Abilities/Tasks/AbilityTask_WaitInputRelease.h"
#include "Abilities/GameplayAbilityTypes.h"
#include "GameplayAbilitySpec.h"
#include "GameplayEffect.h"
#include "GameFramework/Pawn.h"
#include "Animation/AnimMontage.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Character.h"
#include "Interfaces/MovementInputProvider.h"
#include "TacticalAI.h"

UTacticalBasicAttackAbility::UTacticalBasicAttackAbility()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

	FGameplayTagContainer AssetTags = GetAssetTags();
	AssetTags.AddTag(TacticalGameplayTags::Ability_Attack_Basic);
	SetAssetTags(AssetTags);

	ActivationOwnedTags.AddTag(TacticalGameplayTags::State_Attacking);

	ActivationBlockedTags.AddTag(TacticalGameplayTags::State_Attacking);
	ActivationBlockedTags.AddTag(TacticalGameplayTags::State_Dead);
}

// =======================================================
// 평타 발동 조건
//
// GAS 발동 조건과 필수 공격 설정을 검사한다.
// 타깃 유무는 평타 발동을 제한하지 않는다.
//
// 通常攻撃の発動条件。
// GASの発動条件と必須の攻撃設定を検証する。
// ターゲットの有無では通常攻撃の発動を制限しない。
// =======================================================
bool UTacticalBasicAttackAbility::CanActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayTagContainer* SourceTags,
	const FGameplayTagContainer* TargetTags,
	FGameplayTagContainer* OptionalRelevantTags) const
{
	// [1] 태그·코스트·쿨다운 등 GAS의 기본 발동 조건을 검사한다.
	// [1] タグ・コスト・クールダウンなど、GASの基本発動条件を検証する。
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
	{
		return false;
	}

	// [2] 공격 실행에 필요한 ASC와 Avatar가 유효한지 확인한다.
	// [2] 攻撃の実行に必要なASCとAvatarの有効性を確認する。
	if (!ActorInfo || !ActorInfo->AbilitySystemComponent.IsValid() || !ActorInfo->AvatarActor.IsValid())
	{
		return false;
	}

	// [3] 몽타주·타수 목록·피해 Effect가 설정되어 있는지 확인한다.
	// [3] Montage・攻撃段一覧・ダメージEffectの設定を確認する。
	if (!AttackMontage || AttackSteps.IsEmpty() || !DamageEffectClass)
	{
		return false;
	}

	for (const FTacticalBasicAttackStep& AttackStep : AttackSteps)
	{
		if (AttackStep.MontageSection.IsNone())
		{
			return false;
		}
	}

	return true;
}

// =======================================================
// 평타 실행 초기화
//
// ASC가 전달한 실행 방식을 확인하고 첫 타수를 준비한다.
// InputBound 실행에서만 입력 해제를 기다린다.
// 타격·타수 종료 이벤트는 두 실행 방식에서 공통으로 사용한다.
//
// 通常攻撃の実行初期化。
// ASCから受け取った実行方式を確認し、初段を準備する。
// InputBoundでの実行時のみ入力解放を待機する。
// ヒット・各段終了イベントは両方の実行方式で共通して使用する。
// =======================================================
void UTacticalBasicAttackAbility::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	// [1] 전달받은 실행 방식을 확인한다.
	// 누락되거나 지원하지 않는 방식이면 코스트·쿨다운을 확정하기 전에 중단한다.
	// [1] 受け取った実行方式を確認する。
	// 未指定または未対応の方式なら、コスト・クールダウンの確定前に中断する。
	if (!InitializeExecutionMode(TriggerEventData))
	{
		FinishAbility(true);
		return;
	}

	// [2] 코스트·쿨다운을 확정한다. 실패하면 발동을 취소한다.
	// [2] コスト・クールダウンを確定する。失敗した場合は発動を取り消す。
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		FinishAbility(true);
		return;
	}

	// [3] TargetSelector가 있으면 캐싱한다. 없어도 공격은 진행한다.
	// [3] TargetSelectorが存在する場合はキャッシュする。存在しなくても攻撃は継続する。
	AActor* AvatarActor = ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr;
	TargetSelector = AvatarActor ? AvatarActor->FindComponentByClass<UTargetSelectorComponent>() : nullptr;

	// [4] 첫 타수의 실행 상태를 초기화한다.
	// 실행 방식은 이미 결정되어 있으며, Spec에서는 현재 입력 상태만 읽는다.
	// [4] 初段の実行状態を初期化する。
	// 実行方式はすでに決定済みであり、Specからは現在の入力状態のみを読み取る。
	CurrentStepIndex = 0;
	bAttackInputHeld = false;
	bDamageAppliedForCurrentStep = false;
	CurrentStepTarget.Reset();

	if (ExecutionMode == EExecutionMode::InputBound)
	{
		const FGameplayAbilitySpec* Spec = GetCurrentAbilitySpec();
		bAttackInputHeld = Spec && Spec->InputPressed;
	}

	// [5] InputBound 실행에서 입력 해제를 기다린다.
	// [5] InputBoundでの実行時に入力解放を待機する。
	if (ExecutionMode == EExecutionMode::InputBound)
	{
		StartInputReleaseTask();
	}

	// [6] 애니메이션 Gameplay Event 수신을 준비한다.
	// [6] Animation Gameplay Eventの受信を準備する。
	HitEventTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
		this,
		TacticalGameplayTags::Event_Attack_Basic_Hit,
		nullptr,
		false,
		true);

	if (HitEventTask)
	{
		HitEventTask->EventReceived.AddDynamic(this, &UTacticalBasicAttackAbility::HandleHitEvent);
		HitEventTask->ReadyForActivation();
	}

	StepEndEventTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
		this,
		TacticalGameplayTags::Event_Attack_Basic_SectionEnd,
		nullptr,
		false,
		true);

	if (StepEndEventTask)
	{
		StepEndEventTask->EventReceived.AddDynamic(this, &UTacticalBasicAttackAbility::HandleStepEndEvent);
		StepEndEventTask->ReadyForActivation();
	}

	// [7] 첫 타수의 몽타주 재생을 시작한다.
	// [7] 初段のMontage再生を開始する。
	PlayCurrentAttackStep();
}

void UTacticalBasicAttackAbility::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled)
{
	// 정상 종료와 취소 모두 루트모션 이동 배율을 기본값으로 복원한다.
	// 正常終了・キャンセルのどちらでもルートモーション移動倍率を既定値へ戻す。
	ResetRootMotionScale();
	
	MontageTask = nullptr;
	HitEventTask = nullptr;
	StepEndEventTask = nullptr;
	InputReleaseTask = nullptr;

	TargetSelector.Reset();
	CurrentStepTarget.Reset();

	CurrentStepIndex = INDEX_NONE;
	ExecutionMode = EExecutionMode::AutoComplete;
	bAttackInputHeld = false;
	bDamageAppliedForCurrentStep = false;

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

// =======================================================
// 실행 중 추가 입력 누름
//
// InputBound 실행에서 입력 유지 상태를 다시 기록한다.
// 입력 해제 후 현재 타격이 끝나기 전에 다시 누르면 공격을 이어갈 수 있다.
// AutoComplete 실행에는 입력 누름을 반영하지 않는다.
//
// 発動中の追加入力押下。
// InputBoundでの実行時に入力維持状態を再度記録する。
// 入力解放後、現在段が終了する前に再度押すと攻撃を継続できる。
// AutoCompleteでの実行には入力押下を反映しない。
// =======================================================
void UTacticalBasicAttackAbility::InputPressed(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo)
{
	Super::InputPressed(Handle, ActorInfo, ActivationInfo);

	if (!IsActive() || ExecutionMode != EExecutionMode::InputBound)
	{
		return;
	}

	bAttackInputHeld = true;
	StartInputReleaseTask();
}

// =======================================================
// 현재 타수의 대상 결정
// 플레이어의 대상 AI와 별도로 판단한다.
//
// 現在段の対象決定。
// プレイヤーの対象選定はAIとは別に判断する。
// =======================================================
AActor* UTacticalBasicAttackAbility::ResolveAttackTarget_Implementation() const
{
	if (!TargetSelector.IsValid())
	{
		return nullptr;
	}

	// [1] AI 조작에서는 사이클 도중 대상을 바꾸지 않는다.
	// [1] AI操作ではサイクル途中で対象を変更しない。
	const APawn* AvatarPawn = Cast<APawn>(GetAvatarActorFromActorInfo());
	if (!AvatarPawn)
	{
		return nullptr;
	}

	if (!AvatarPawn->IsPlayerControlled())
	{
		if (AActor* PreviousTarget = CurrentStepTarget.Get())
		{
			if (TargetSelector->IsValidCandidate(PreviousTarget)) return PreviousTarget;
		}
		return TargetSelector->GetCurrentTarget();
	}

	// [2] 방향 입력이 없으면 이전 대상의 유지 가능 여부를 먼저 확인한다.
	// 더 가까운 후보가 생겼는지는 비교하지 않는다.
	// [2] 方向入力がなければ前段の対象を維持できるか先に確認する。
	// より近い候補が現れたかは比較しない。
	const FVector InputDirection2D = GetDesiredInputDirection2D();

	if (InputDirection2D.IsNearlyZero())
	{
		if (AActor* PreviousTarget = CurrentStepTarget.Get())
		{
			if (CanKeepCurrentStepTarget(AvatarPawn, PreviousTarget))
			{
				return PreviousTarget;
			}
		}
	}

	// [3] 입력 방향 또는 캐릭터 정면을 기준으로 새 대상을 검색한다.
	// 검색 결과가 없으면 nullptr를 반환하여 허공 공격으로 진행한다.
	// [3] 入力方向またはキャラクター前方を基準に新しい対象を検索する。
	// 対象がなければnullptrを返し、空振り攻撃として続行する。
	return TargetSelector->FindPlayerAttackTarget(
		AvatarPawn->GetActorLocation(),
		AvatarPawn->GetActorForwardVector(),
		InputDirection2D,
		TargetSearchDistance,
		TargetSearchHalfAngleDeg);
}

// =======================================================
// 이동 입력 방향 조회.
// 移動入力方向の取得。
// =======================================================
FVector UTacticalBasicAttackAbility::GetDesiredInputDirection2D() const
{
	const IMovementInputProvider* InputProvider =
		Cast<IMovementInputProvider>(GetAvatarActorFromActorInfo());

	return InputProvider
		? InputProvider->GetMovementInputDirection2D()
		: FVector::ZeroVector;
}

// =======================================================
// 무입력 상태에서 이전 대상의 유지 가능 여부 확인
// 方向入力がない場合の前段ターゲット維持判定。
// =======================================================
bool UTacticalBasicAttackAbility::CanKeepCurrentStepTarget(
	const AActor* AvatarActor,
	const AActor* TargetActor) const
{
	// [1] 대상의 현재 후보 자격을 확인한다.
	// [1] 対象の現在の候補資格を確認する。
	if (!AvatarActor || !TargetSelector.IsValid() ||
		!TargetSelector->IsValidCandidate(TargetActor))
	{
		return false;
	}

	// [2] 신규 검색과 같은 수평 중심 거리 기준을 사용한다.
	// [2] 新規検索と同じ水平中心距離の基準を使用する。
	FVector ToTarget2D = TargetActor->GetActorLocation() - AvatarActor->GetActorLocation();
	ToTarget2D.Z = 0.0;

	const double DistanceSquared = ToTarget2D.SizeSquared();
	const double MaxDistance = FMath::Max(0.0, static_cast<double>(TargetSearchDistance));

	if (MaxDistance <= 0.0 || DistanceSquared > FMath::Square(MaxDistance))
	{
		return false;
	}

	// 이미 선정된 대상이 같은 수평 위치에 있다면 방향만으로 해제하지 않는다.
	// 選定済みの対象が同一水平位置にいる場合、方向だけを理由に解除しない。
	if (DistanceSquared <= KINDA_SMALL_NUMBER)
	{
		return true;
	}

	// [3] 캐릭터 정면 기준 허용 반각 안에 있는지 확인한다.
	// [3] キャラクター前方を基準に許容半角内か確認する。
	const double HalfAngleDeg = FMath::Clamp(
		static_cast<double>(TargetSearchHalfAngleDeg), 0.0, 180.0);

	if (HalfAngleDeg >= 180.0)
	{
		return true;
	}

	const FVector FacingDirection2D = AvatarActor->GetActorForwardVector().GetSafeNormal2D();
	const FVector TargetDirection2D = ToTarget2D / FMath::Sqrt(DistanceSquared);
	const double DirectionDot = FVector::DotProduct(FacingDirection2D, TargetDirection2D);
	const double CosThreshold = FMath::Cos(FMath::DegreesToRadians(HalfAngleDeg));

	return DirectionDot >= CosThreshold;
}

// =======================================================
// 평타 타수 시작 방향 정렬
// 대상이 있으면 대상 방향, 없으면 이동 입력 방향을 사용한다.
// 타수 도중에는 이 함수에서 방향을 갱신하지 않는다.
//
// 通常攻撃の各段開始時の方向整列。
// 対象があれば対象方向、なければ移動入力方向を使用する。
// 段の途中ではこの関数による方向更新を行わない。
// =======================================================
void UTacticalBasicAttackAbility::UpdateCurrentStepFacing()
{
	if (!bFaceTargetOnStepStart) return;

	APawn* AvatarPawn = Cast<APawn>(GetAvatarActorFromActorInfo());
	if (!AvatarPawn) return;

	// [1] 확정된 대상 또는 플레이어 입력에서 공격 방향을 구한다.
	// [1] 確定した対象またはプレイヤー入力から攻撃方向を求める。
	FVector AttackDirection2D = FVector::ZeroVector;

	if (const AActor* TargetActor = CurrentStepTarget.Get())
	{
		AttackDirection2D =
			(TargetActor->GetActorLocation() - AvatarPawn->GetActorLocation()).GetSafeNormal2D();
	}
	else if (AvatarPawn->IsPlayerControlled())
	{
		AttackDirection2D = GetDesiredInputDirection2D();
	}

	// 대상 방향도 입력 방향도 없으면 현재 회전을 유지한다.
	// 対象方向も入力方向もなければ現在の回転を維持する。
	if (AttackDirection2D.IsNearlyZero()) return;

	// [2] 수평 회전만 변경한다.
	// [2] 水平回転のみ変更する。
	FRotator AttackRotation = AvatarPawn->GetActorRotation();
	AttackRotation.Yaw = AttackDirection2D.Rotation().Yaw;
	AvatarPawn->SetActorRotation(AttackRotation);
}

// =======================================================
// 현재 타수의 루트모션 이동 배율 계산
// 대상이 없거나 보정 조건을 만족하지 않으면 원본 배율을 반환한다.
//
// 現在段のルートモーション移動倍率を計算する。
// 対象がない場合、または補正条件を満たさない場合は元の倍率を返す。
// =======================================================
float UTacticalBasicAttackAbility::CalculateCurrentStepRootMotionScale(
	const FName& SectionName) const
{
	// [1] 대상 방향으로 정렬하는 공격의 이동량을 보정한다.
	// [1] 対象方向へ整列する攻撃の移動量を補正する。
	const ACharacter* AvatarCharacter =
		Cast<ACharacter>(GetAvatarActorFromActorInfo());

	const ACharacter* TargetCharacter =
		Cast<ACharacter>(CurrentStepTarget.Get());

	if (!AvatarCharacter || !TargetCharacter ||
		!bFaceTargetOnStepStart || !AttackMontage)
	{
		return 1.f;
	}

	const UCapsuleComponent* AvatarCapsule = AvatarCharacter->GetCapsuleComponent();
	const UCapsuleComponent* TargetCapsule = TargetCharacter->GetCapsuleComponent();
	const USkeletalMeshComponent* AvatarMesh = AvatarCharacter->GetMesh();

	if (!AvatarCapsule || !TargetCapsule || !AvatarMesh)
	{
		return 1.f;
	}

	// [2] 양쪽 캡슐 반경과 여유 거리로 이동 가능한 거리를 계산한다.
	// 이미 정지 거리 안에 있다면 뒤로 이동시키지 않고 이동량을 0으로 한다.
	// [2] 両者のカプセル半径と余白から移動可能距離を計算する。
	// 停止距離内なら後退させず、移動量を0にする。
	const double CenterDistance = FVector::Dist2D(
		AvatarCharacter->GetActorLocation(),
		TargetCharacter->GetActorLocation());

	const double StopDistance =
		AvatarCapsule->GetScaledCapsuleRadius() +
		TargetCapsule->GetScaledCapsuleRadius() +
		FMath::Max(0.f, AttackStopDistanceMargin);

	const double AvailableMoveDistance = FMath::Max(0.0, CenterDistance - StopDistance);
	if (AvailableMoveDistance <= KINDA_SMALL_NUMBER)
	{
		return 0.f;
	}

	// [3] 현재 타수 섹션의 원본 루트모션 수평 이동량을 월드 단위로 구한다.
	// [3] 現在段Sectionの元のルートモーション水平移動量をワールド単位で求める。
	double OriginalMoveDistance = 0.0;
	if (!TryExtractSectionRootMotionDistance2D(SectionName, *AvatarMesh, OriginalMoveDistance) ||
		OriginalMoveDistance <= KINDA_SMALL_NUMBER)
	{
		return 1.f;
	}

	// [4] 원본 이동량 이내에서 적 앞까지 필요한 비율만 사용한다.
	// [4] 元の移動量を上限に、対象の手前まで必要な割合だけ使用する。
	return static_cast<float>(FMath::Clamp(
		AvailableMoveDistance / OriginalMoveDistance, 0.0, 1.0));
}

void UTacticalBasicAttackAbility::ResetRootMotionScale()
{
	if (ACharacter* AvatarCharacter = Cast<ACharacter>(GetAvatarActorFromActorInfo()))
	{
		AvatarCharacter->SetAnimRootMotionTranslationScale(1.f);
	}
}

// =======================================================
// 섹션 원본 루트모션의 수평 이동량
// 타수별 이동 배율과 AI 교전 거리가 같은 추출 규칙을 사용한다.
//
// Section元ルートモーションの水平移動量。
// 段ごとの移動倍率とAI交戦距離が同じ抽出規則を使用する。
// =======================================================
bool UTacticalBasicAttackAbility::TryExtractSectionRootMotionDistance2D(
	const FName& SectionName,
	const USkeletalMeshComponent& Mesh,
	double& OutDistance) const
{
	OutDistance = 0.0;
	if (!AttackMontage) return false;

	// [1] 섹션 구간을 확인한다.
	// [1] Section区間を確認する。
	const int32 SectionIndex = AttackMontage->GetSectionIndex(SectionName);
	if (SectionIndex == INDEX_NONE) return false;

	float StartTime = 0.f;
	float EndTime = 0.f;
	AttackMontage->GetSectionStartAndEndTime(SectionIndex, StartTime, EndTime);
	if (EndTime <= StartTime) return false;

	// [2] 원본 루트모션 변위에 메시의 월드 변환을 반영한다.
	//     캡슐 거리와 같은 월드 단위로 비교하기 위해 메시 스케일도 반영한다.
	// [2] 元のルートモーション変位にMeshのワールド変換を適用する。
	//     カプセル間距離と同じワールド単位で比較するため、Meshのスケールも反映する。
	const FTransform RootMotionTransform =
		AttackMontage->ExtractRootMotionFromTrackRange(StartTime, EndTime);

	const FVector WorldTranslation = Mesh.GetComponentTransform().TransformVector(
		RootMotionTransform.GetTranslation());

	OutDistance = WorldTranslation.Size2D();
	return true;
}

// =======================================================
// AI 교전 거리 산출 (IAbilityRangeProvider)
// 전체 타수의 루트모션 수평 이동량 합계에 비율을 곱한다.
// 타수마다 대상 방향으로 다시 정렬하므로 순 변위가 아니라 타수별 이동량을 합산한다.
// 가까운 위치에서 시작해도 타수별 이동 배율이 대상 앞에서 이동을 멈추게 한다.
// CDO에서 호출될 수 있으므로 상태를 변경하지 않는다.
//
// AI交戦距離の算出（IAbilityRangeProvider）。
// 全段のルートモーション水平移動量の合計に割合を掛ける。
// 段ごとに対象方向へ再整列するため、正味の変位ではなく段ごとの移動量を合算する。
// 近い位置から開始しても、段ごとの移動倍率が対象の手前で移動を止める。
// CDOから呼ばれ得るため状態を変更しない。
// =======================================================
bool UTacticalBasicAttackAbility::TryGetEngageDistance(
	const FGameplayAbilityActorInfo& ActorInfo,
	float& OutDistance) const
{
	// [1] 산출에 필요한 공격 설정과 시전자 메시를 확인한다.
	// [1] 算出に必要な攻撃設定と術者のMeshを確認する。
	if (!AttackMontage || AttackSteps.IsEmpty()) return false;

	const ACharacter* AvatarCharacter = Cast<ACharacter>(ActorInfo.AvatarActor.Get());
	const USkeletalMeshComponent* AvatarMesh = AvatarCharacter ? AvatarCharacter->GetMesh() : nullptr;
	if (!AvatarMesh) return false;

	// [2] 타수별 섹션의 원본 이동량을 합산한다. 섹션 설정 오류가 있으면 산출하지 않는다.
	// [2] 段ごとのSectionの元移動量を合算する。Section設定に誤りがあれば算出しない。
	double TotalMoveDistance = 0.0;
	for (const FTacticalBasicAttackStep& AttackStep : AttackSteps)
	{
		double StepMoveDistance = 0.0;
		if (!TryExtractSectionRootMotionDistance2D(AttackStep.MontageSection, *AvatarMesh, StepMoveDistance))
		{
			return false;
		}
		TotalMoveDistance += StepMoveDistance;
	}

	// [3] 제자리 공격이면 루트모션으로 교전 거리를 정할 수 없으므로 호출 측 기본값에 맡긴다.
	// [3] その場攻撃ならルートモーションで交戦距離を決められないため、呼び出し側の既定値に任せる。
	if (TotalMoveDistance <= KINDA_SMALL_NUMBER) return false;

	OutDistance = static_cast<float>(TotalMoveDistance * EngageRootMotionRatio);
	return true;
}

// =======================================================
// 실행 방식 설정
//
// 발동 시 전달받은 EventTag를 평타 내부 실행 방식으로 변환한다.
// 입력 상태나 Controller 종류로 실행 방식을 추측하지 않는다.
//
// 実行方式の設定。
// 発動時に受け取ったEventTagを通常攻撃内部の実行方式へ変換する。
// 入力状態やControllerの種類から実行方式を推測しない。
// =======================================================
bool UTacticalBasicAttackAbility::InitializeExecutionMode(const FGameplayEventData* TriggerEventData)
{
	if (!TriggerEventData)
	{
		UE_LOG(LogTacticalAI, Warning, TEXT("通常攻撃: 実行方式を含む発動データがありません。"));
		return false;
	}

	const FGameplayTag& ExecutionModeTag = TriggerEventData->EventTag;

	if (ExecutionModeTag == TacticalGameplayTags::Event_Ability_Activate_InputBound)
	{
		ExecutionMode = EExecutionMode::InputBound;
		return true;
	}

	if (ExecutionModeTag == TacticalGameplayTags::Event_Ability_Activate_AutoComplete)
	{
		ExecutionMode = EExecutionMode::AutoComplete;
		return true;
	}

	UE_LOG(LogTacticalAI, Warning, TEXT("通常攻撃: 未対応の実行方式です。Mode=%s"),
		*ExecutionModeTag.ToString());
	return false;
}

// =======================================================
// 입력 해제 대기
//
// InputBound 실행에서 입력 해제를 기다리는 Task를 생성한다.
// 이미 대기 중이면 기존 Task를 유지한다.
// 해제 이벤트를 받은 Task는 종료되므로, 재입력 시 새로 생성한다.
//
// 入力解放の待機。
// InputBoundでの実行時に入力解放を待機するTaskを生成する。
// すでに待機中の場合は既存のTaskを維持する。
// 解放イベントを受けたTaskは終了するため、再入力時に新しく生成する。
// =======================================================
void UTacticalBasicAttackAbility::StartInputReleaseTask()
{
	if (!IsActive() || ExecutionMode != EExecutionMode::InputBound || InputReleaseTask)
	{
		return;
	}

	InputReleaseTask = UAbilityTask_WaitInputRelease::WaitInputRelease(this, true);
	if (InputReleaseTask)
	{
		InputReleaseTask->OnRelease.AddDynamic(this, &UTacticalBasicAttackAbility::HandleInputReleased);
		InputReleaseTask->ReadyForActivation();
	}
}

// =======================================================
// 현재 타수 준비
//
// 타수 인덱스를 검증하고 대상과 피해 적용 상태를 갱신한다.
// 대상이 없어도 현재 타수의 애니메이션은 재생한다.
//
// 現在段の準備。
// 段インデックスを検証し、対象とダメージ適用状態を更新する。
// 対象が存在しない場合も現在段のアニメーションは再生する。
// =======================================================
bool UTacticalBasicAttackAbility::PrepareCurrentAttackStep()
{
	// [1] 재생할 타수 데이터가 유효한지 확인한다.
	// [1] 再生する攻撃段のデータが有効か確認する。
	if (!AttackSteps.IsValidIndex(CurrentStepIndex))
	{
		UE_LOG(LogTacticalAI, Warning, TEXT("通常攻撃: 無効なAttackStepです。Index=%d"), CurrentStepIndex);
		return false;
	}

	// [2] 이번 타수의 대상을 조회하고 중복 피해 방지 상태를 초기화한다.
	// 대상이 없는 경우도 정상적인 공격 실행으로 처리한다.
	// [2] 現在段の対象を取得し、重複ダメージ防止状態を初期化する。
	// 対象が存在しない場合も通常の攻撃実行として扱う。
	CurrentStepTarget = ResolveAttackTarget();
	bDamageAppliedForCurrentStep = false;

	// [3] 이번 타수에서 대상이 확정된 뒤 공격 방향을 적용한다.
	// [3] 現在段の対象を確定した後、攻撃方向を適用する。
	UpdateCurrentStepFacing();
	
	return true;
}

// =======================================================
// 현재 타수 재생
// 現在段の再生。
// =======================================================
void UTacticalBasicAttackAbility::PlayCurrentAttackStep()
{
	if (!PrepareCurrentAttackStep())
	{
		FinishAbility(false);
		return;
	}

	// [1] 대상과 공격 방향이 확정된 상태에서 이번 타수의 이동 배율을 계산한다.
	// [1] 対象と攻撃方向を確定した状態で、現在段の移動倍率を計算する。
	const FTacticalBasicAttackStep& CurrentStep = AttackSteps[CurrentStepIndex];
	const float RootMotionScale =
		CalculateCurrentStepRootMotionScale(CurrentStep.MontageSection);

	// [2] 첫 재생에서는 Task가 사용할 루트모션 이동 배율을 함께 전달한다.
	// [2] 初回再生ではTaskが使用するルートモーション移動倍率も渡す。
	if (!MontageTask)
	{
		MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
			this,
			NAME_None,
			AttackMontage,
			1.0f,
			CurrentStep.MontageSection,
			true,
			RootMotionScale);

		if (!MontageTask)
		{
			UE_LOG(LogTacticalAI, Warning, TEXT("通常攻撃: MontageTaskの作成に失敗しました。"));
			FinishAbility(true);
			return;
		}

		MontageTask->OnCompleted.AddDynamic(this, &UTacticalBasicAttackAbility::HandleMontageCompleted);
		MontageTask->OnInterrupted.AddDynamic(this, &UTacticalBasicAttackAbility::HandleMontageInterrupted);
		MontageTask->OnCancelled.AddDynamic(this, &UTacticalBasicAttackAbility::HandleMontageCancelled);
		MontageTask->ReadyForActivation();

		return;
	}

	UAbilitySystemComponent* SourceASC = GetAbilitySystemComponentFromActorInfo();
	if (!SourceASC)
	{
		UE_LOG(LogTacticalAI, Warning, TEXT("通常攻撃: Source ASCがありません。"));
		FinishAbility(true);
		return;
	}

	// [3] 다음 타수에서는 기존 Task를 유지하므로 캐릭터의 배율을 직접 갱신한다.
	// 허공 공격으로 전환되면 계산 결과 1.0이 적용된다.
	// [3] 次段では既存Taskを維持するため、Characterの倍率を直接更新する。
	// 空振り攻撃へ切り替わった場合は計算結果の1.0が適用される。
	if (ACharacter* AvatarCharacter = Cast<ACharacter>(GetAvatarActorFromActorInfo()))
	{
		AvatarCharacter->SetAnimRootMotionTranslationScale(RootMotionScale);
	}

	SourceASC->CurrentMontageJumpToSection(CurrentStep.MontageSection);
}

// =======================================================
// 현재 타수의 명중 가능 여부
// 대상은 다시 선정하지 않고, 타격 시점의 위치와 자격만 검사한다.
//
// 現在段の命中可否。
// 対象は再選定せず、ヒット時点の位置と資格のみ検証する。
// =======================================================
bool UTacticalBasicAttackAbility::CanHitCurrentStepTarget() const
{
	// [1] 공격자와 대상의 유효성 및 타격 대상 자격을 확인한다.
	// [1] 攻撃者と対象の有効性、および対象資格を確認する。
	const AActor* AvatarActor = GetAvatarActorFromActorInfo();
	AActor* TargetActor = CurrentStepTarget.Get();

	if (!IsValid(AvatarActor) || !IsValid(TargetActor) || AvatarActor == TargetActor)
	{
		return false;
	}

	if (!TargetActor->Implements<UTargetable>() ||
		!ITargetable::Execute_IsTargetable(TargetActor))
	{
		return false;
	}

	// [2] 수평 거리와 별도로 중심 간 높이 차이를 제한한다.
	// [2] 水平距離とは別に、中心間の高低差を制限する。
	FVector ToTarget2D = TargetActor->GetActorLocation() - AvatarActor->GetActorLocation();

	if (FMath::Abs(ToTarget2D.Z) >
		FMath::Max(0.0, static_cast<double>(MaxHitHeightDifference)))
	{
		return false;
	}

	ToTarget2D.Z = 0.0;

	// [3] 양쪽 캡슐 반경을 더해 표면 간격 기준의 명중 거리를 계산한다.
	// [3] 両者のカプセル半径を加え、表面間距離を基準とする命中距離を計算する。
	double AvatarRadius = 0.0;
	double TargetRadius = 0.0;

	if (const ACharacter* AvatarCharacter = Cast<ACharacter>(AvatarActor))
	{
		if (const UCapsuleComponent* Capsule = AvatarCharacter->GetCapsuleComponent())
		{
			AvatarRadius = Capsule->GetScaledCapsuleRadius();
		}
	}

	if (const ACharacter* TargetCharacter = Cast<ACharacter>(TargetActor))
	{
		if (const UCapsuleComponent* Capsule = TargetCharacter->GetCapsuleComponent())
		{
			TargetRadius = Capsule->GetScaledCapsuleRadius();
		}
	}

	const double MaxCenterDistance =
		AvatarRadius + TargetRadius + FMath::Max(0.0, static_cast<double>(HitRange));

	const double DistanceSquared = ToTarget2D.SizeSquared();
	if (DistanceSquared > FMath::Square(MaxCenterDistance))
	{
		return false;
	}

	// 같은 수평 위치에서는 방향을 판정할 수 없으므로 거리 판정만 사용한다.
	// 同一水平位置では方向を判定できないため、距離判定のみ使用する。
	if (DistanceSquared <= KINDA_SMALL_NUMBER)
	{
		return true;
	}

	// [4] 타격 순간의 캐릭터 정면을 기준으로 명중 각도를 검사한다.
	// [4] ヒット時点のキャラクター前方を基準に命中角度を検証する。
	const double HalfAngleDeg = FMath::Clamp(
		static_cast<double>(HitHalfAngleDeg), 0.0, 180.0);

	if (HalfAngleDeg >= 180.0)
	{
		return true;
	}

	const FVector FacingDirection2D = AvatarActor->GetActorForwardVector().GetSafeNormal2D();
	const FVector TargetDirection2D = ToTarget2D / FMath::Sqrt(DistanceSquared);
	const double DirectionDot = FVector::DotProduct(FacingDirection2D, TargetDirection2D);
	const double CosThreshold = FMath::Cos(FMath::DegreesToRadians(HalfAngleDeg));

	return DirectionDot >= CosThreshold;
}

void UTacticalBasicAttackAbility::ApplyCurrentStepDamage()
{
	if (bDamageAppliedForCurrentStep)
	{
		return;
	}

	// 타격 시점에 범위나 대상 자격을 벗어났으면 피해를 적용하지 않는다.
	// ヒット時点で範囲外、または対象資格を失っていればダメージを適用しない。
	if (!CanHitCurrentStepTarget())
	{
		return;
	}

	UAbilitySystemComponent* SourceASC = GetAbilitySystemComponentFromActorInfo();
	UAbilitySystemComponent* TargetASC = CurrentStepTarget.IsValid()
		? UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(CurrentStepTarget.Get())
		: nullptr;

	if (!SourceASC || !TargetASC)
	{
		UE_LOG(LogTacticalAI, Verbose, TEXT("通常攻撃: SourceまたはTargetのASCがありません。"));
		return;
	}

	if (!AttackSteps.IsValidIndex(CurrentStepIndex))
	{
		return;
	}

	const float BasicAttackPower = SourceASC->GetNumericAttribute(
		UTacticalCombatAttributeSet::GetBasicAttackPowerAttribute());

	const float DamageAmount = BasicAttackPower * AttackSteps[CurrentStepIndex].DamageMultiplier;
	if (DamageAmount <= 0.f)
	{
		return;
	}

	FGameplayEffectSpecHandle DamageSpecHandle =
		MakeOutgoingGameplayEffectSpec(DamageEffectClass, GetAbilityLevel());

	if (!DamageSpecHandle.IsValid() || !DamageSpecHandle.Data.IsValid())
	{
		UE_LOG(LogTacticalAI, Warning, TEXT("通常攻撃: DamageSpecの作成に失敗しました。"));
		return;
	}

	DamageSpecHandle.Data->SetSetByCallerMagnitude(
		TacticalGameplayTags::Data_Damage,
		DamageAmount);

	SourceASC->ApplyGameplayEffectSpecToTarget(*DamageSpecHandle.Data.Get(), TargetASC);
	bDamageAppliedForCurrentStep = true;
}

void UTacticalBasicAttackAbility::FinishAbility(bool bWasCancelled)
{
	if (!IsActive()) return;

	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, bWasCancelled);
}

void UTacticalBasicAttackAbility::HandleHitEvent(FGameplayEventData Payload)
{
	ApplyCurrentStepDamage();
}

// =======================================================
// 현재 타수 종료
//
// InputBound는 입력 유지 여부로 공격 지속을 결정한다.
// AutoComplete는 마지막 타수를 마쳤는지로 종료를 결정한다.
//
// 現在段の終了。
// InputBoundは入力維持状態によって攻撃の継続を判断する。
// AutoCompleteは最終段を完了したかによって終了を判断する。
// =======================================================
void UTacticalBasicAttackAbility::HandleStepEndEvent(FGameplayEventData Payload)
{
	if (!IsActive()) return;

	if (!AttackSteps.IsValidIndex(CurrentStepIndex))
	{
		FinishAbility(true);
		return;
	}

	switch (ExecutionMode)
	{
	case EExecutionMode::InputBound:
		// 입력을 해제했다면 현재 타격을 마친 이 시점에 종료한다.
		// 入力が解放されていれば、現在段を完了したこの時点で終了する。
		if (!bAttackInputHeld)
		{
			FinishAbility(false);
			return;
		}
		break;

	case EExecutionMode::AutoComplete:
		// 마지막 타격을 마쳤다면 한 사이클을 완료한 것이므로 종료한다.
		// 最終段を完了していれば、1サイクルが完了したため終了する。
		if (CurrentStepIndex == AttackSteps.Num() - 1)
		{
			FinishAbility(false);
			return;
		}
		break;
	}

	// 다음 타수를 재생한다.
	// 마지막 타수에서 첫 타수로 돌아가는 것은 입력을 유지 중인 InputBound 실행뿐이다.
	// 次段を再生する。
	// 最終段から初段へ戻るのは、入力維持中のInputBound実行のみ。
	CurrentStepIndex = (CurrentStepIndex + 1) % AttackSteps.Num();
	PlayCurrentAttackStep();
}

void UTacticalBasicAttackAbility::HandleInputReleased(float TimeHeld)
{
	// 해제를 받은 Task는 스스로 종료한다.
	// 참조를 비워 이후 재입력 시 새 Task를 생성할 수 있도록 한다.
	// 解放を受けたTaskは自身で終了する。
	// 参照をクリアし、再入力時に新しいTaskを生成できるようにする。
	InputReleaseTask = nullptr;
	bAttackInputHeld = false;
}

void UTacticalBasicAttackAbility::HandleMontageCompleted()
{
	FinishAbility(false);
}

void UTacticalBasicAttackAbility::HandleMontageInterrupted()
{
	FinishAbility(true);
}

void UTacticalBasicAttackAbility::HandleMontageCancelled()
{
	FinishAbility(true);
}