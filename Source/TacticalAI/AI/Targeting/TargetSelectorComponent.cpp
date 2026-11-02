#include "AI/Targeting/TargetSelectorComponent.h"
#include "AI/Targeting/Targetable.h"
#include "DrawDebugHelpers.h"

DEFINE_LOG_CATEGORY_STATIC(LogTargetSelector, Log, All);

// 타겟 전환 로그·타겟 라인 표시. 콘솔: TacticalAI.DebugTargetSelector 1
// ターゲット遷移ログ・ライン表示。コンソール: TacticalAI.DebugTargetSelector 1
static TAutoConsoleVariable<bool> CVarDebugTargetSelector(
	TEXT("TacticalAI.DebugTargetSelector"), false,
	TEXT("Log target transitions and draw a line to the current target."));

UTargetSelectorComponent::UTargetSelectorComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UTargetSelectorComponent::BeginPlay()
{
	Super::BeginPlay();
	// 첫 평가 시점을 0~주기 사이 랜덤으로 — 스폰 프레임이 같아도 위상이 갈라진다.
	// 初回評価を0〜周期のランダムに — スポーンフレームが同じでも位相が割れる。
	TimeUntilNextEvaluation = FMath::FRandRange(0.f, EvaluateInterval);
}

void UTargetSelectorComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	//살아있는 타겟에 남은 바인딩 해제.
	//生存ターゲットへのバインド解除。
	if (AActor* Target = CurrentTarget.Get())
	{
		Target->OnDestroyed.RemoveDynamic(this, &UTargetSelectorComponent::HandleTargetDestroyed);
	}
	Super::EndPlay(EndPlayReason);
}

void UTargetSelectorComponent::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// [1] 자격 상실 감시 (소멸은 OnDestroyed 이벤트가 잡고, 여기는 "살아있는데 못 잡게 된 상황 감지").
	// 資格喪失の監視（消滅はイベント、ここは「生存中の資格喪失」）。
	if (AActor* Target = CurrentTarget.Get())
	{
		if (Target->Implements<UTargetable>() && !ITargetable::Execute_IsTargetable(Target))
		{
			Target->OnDestroyed.RemoveDynamic(this, &UTargetSelectorComponent::HandleTargetDestroyed);
			CurrentTarget = nullptr;
			ScheduleReactionReevaluation();
		}
	}

	// [2] 주기 평가. 홀드 중엔 스킵 — 결정 주도 정책이 선언한 시간만큼 타겟을 신뢰한다.
	//     홀드·평가 타이머는 병행 감소: 홀드 만료 후 다음 주기 만료 시점에 재평가
	//     (홀드 경계 동시 전환 방지 — 어긋난 박동이 홀드 뒤에도 유지된다).
	// 周期評価。ホールド中はスキップ。両タイマー並行減算で位相のずれを維持。
	TimeUntilNextEvaluation -= DeltaTime;
	RemainingHoldTime -= DeltaTime;
	if (TimeUntilNextEvaluation <= 0.f)
	{
		if (RemainingHoldTime <= 0.f)
		{
			EvaluateTargets(PendingReason);
			PendingReason = ETargetChangeReason::Periodic;
		}
		TimeUntilNextEvaluation = RollNextInterval();
	}

#if ENABLE_DRAW_DEBUG
	if (CVarDebugTargetSelector.GetValueOnGameThread())
	{
		if (const AActor* Target = CurrentTarget.Get())
		{
			const FVector ZOffset(0.f, 0.f, DebugTargetLineZOffset);
			DrawDebugDirectionalArrow(GetWorld(),
				GetOwner()->GetActorLocation() + ZOffset,
				CurrentTarget->GetActorLocation() + ZOffset,
				120.f, DebugTargetLineColor, false, -1.f, 0, 2.f);
		}
	}
#endif
}

void UTargetSelectorComponent::EvaluateTargets(const ETargetChangeReason Reason)
{
	const FTargetingContext Context = BuildContext();
	
	// [1] Targetable 資格のみここでフィルタ。
	AActor* BestTarget = nullptr;
	float BestScore = 0.f;

	for (AActor* Candidate : GatherCandidates())
	{
		if (!Candidate || !Candidate->Implements<UTargetable>() ||
			!ITargetable::Execute_IsTargetable(Candidate))
		{
			continue;
		}

		// [2] 가중 합산. 정책은 0~1 계약이므로 합산 스케일 = 가중치 합.
		// [2] 重み合算。ポリシーは0〜1契約のため合算スケール＝重みの和。
		float Score = 0.f;
		for (const FWeightedTargetPolicy& Entry : Policies)
		{
			if (Entry.Policy)
			{
				Score += Entry.Weight * Entry.Policy->ScoreTarget(Context, Candidate);
			}
		}

		if (Score > BestScore)
		{
			BestScore = Score;
			BestTarget = Candidate;
		}
	}

	// [3] 결정 귀속: 승자에 대한 가중 기여가 최대인 정책이 "결정 주도" — 그 행의 유지시간 채택.
	// 決定帰属：勝者への加重寄与が最大のポリシーが主導 — その行の維持時間を採用。
	RemainingHoldTime = 0.f;
	if (BestTarget)
	{
		float BestContribution = 0.f;
		for (const FWeightedTargetPolicy& Entry : Policies)
		{
			if (!Entry.Policy) continue;

			const float Contribution = Entry.Weight * Entry.Policy->ScoreTarget(Context, BestTarget);
			if (Contribution > BestContribution)
			{
				BestContribution = Contribution;
				RemainingHoldTime = Entry.Policy->GetHoldDuration();
			}
		}
	}

	// [4] 커밋. 주기 단위 교체 마진은 없음 — 억제는 홀드(시간 구조)가 담당.
	// コミット。周期単位の切替マージンは無し — 抑制はホールド（時間構造）の担当。
	CommitTarget(BestTarget, BestTarget ? Reason : ETargetChangeReason::NoCandidates);
}

void UTargetSelectorComponent::CommitTarget(AActor* NewTarget, const ETargetChangeReason Reason)
{
	AActor* OldTarget = CurrentTarget.Get();
	if (OldTarget == NewTarget) return;   // 전환 시에만 바인딩·로그.

	// [1] 이전 타겟 바인딩 해제 → 새 타겟 구독.
	// 旧ターゲットのバインド解除 → 新ターゲット購読。
	if (OldTarget)
	{
		OldTarget->OnDestroyed.RemoveDynamic(this, &UTargetSelectorComponent::HandleTargetDestroyed);
	}
	if (NewTarget)
	{
		NewTarget->OnDestroyed.AddDynamic(this, &UTargetSelectorComponent::HandleTargetDestroyed);
	}
	CurrentTarget = NewTarget;
	
	// 遷移ログ。
	if (CVarDebugTargetSelector.GetValueOnGameThread())
	{
		UE_LOG(LogTargetSelector, Log,
			TEXT("ターゲット遷移: Owner=%s, %s → %s, 理由=%s"),
			*GetNameSafe(GetOwner()), *GetNameSafe(OldTarget), *GetNameSafe(NewTarget),
			*UEnum::GetValueAsString(Reason));
	}
}

void UTargetSelectorComponent::ScheduleReactionReevaluation()
{
	// 즉시 재평가가 아니라 랜덤 반응 지연 후 평가
	// 전원 동일 프레임 전환(기계감)을 방지. 사유는 보관해 다음 평가에 적용.
	// 即時ではなくランダム反応遅延後に評価。理由は保管し次評価に適用。
	RemainingHoldTime = 0.f;
	PendingReason = ETargetChangeReason::TargetLost;
	TimeUntilNextEvaluation = FMath::FRandRange(
		FMath::Max(0.f, (float)ReactionDelayRange.X),
		FMath::Max((float)ReactionDelayRange.X, (float)ReactionDelayRange.Y));
}

float UTargetSelectorComponent::RollNextInterval() const
{
	return EvaluateInterval * FMath::FRandRange(1.f - IntervalJitter, 1.f + IntervalJitter);
}

void UTargetSelectorComponent::HandleTargetDestroyed(AActor* DestroyedActor)
{
	// 破壊されたActorへのバインドはActorと共に消えるため解除不要。
	CurrentTarget = nullptr;
	ScheduleReactionReevaluation();
}

// =======================================================
// 플레이어 공격 대상 검색
//
// 방향 입력이 있으면 입력 방향을 기준으로 방향 일치도를 우선한다.
// 방향 입력이 없으면 캐릭터 정면을 기준으로 가까운 대상을 우선한다.
// 기존 후보 공급원을 사용하며, AI 정책 평가와 CurrentTarget은 변경하지 않는다.
// 거리와 각도는 대상 Actor 위치의 수평 방향을 기준으로 계산한다.
//
// プレイヤーの攻撃対象を検索する。
// 方向入力があれば入力方向との一致度を優先する。
// 方向入力がなければキャラクター前方を基準に近い対象を優先する。
// 既存の候補供給元を使い、AIのポリシー評価とCurrentTargetは変更しない。
// 距離と角度は対象Actor位置の水平方向を基準に計算する。
// =======================================================
AActor* UTargetSelectorComponent::FindPlayerAttackTarget(
	const FVector& Origin,
	const FVector& FacingDirection2D,
	const FVector& InputDirection2D,
	const float MaxDistance,
	const float MaxHalfAngleDeg) const
{
	// [1] 입력 유무에 따라 검색 기준 방향을 결정한다.
	// [1] 入力の有無に応じて検索基準方向を決める。
	const FVector InputDirection = InputDirection2D.GetSafeNormal2D();
	const bool bHasDirectionalInput = !InputDirection.IsNearlyZero();
	const FVector SearchDirection = bHasDirectionalInput
		? InputDirection
		: FacingDirection2D.GetSafeNormal2D();

	if (SearchDirection.IsNearlyZero() || MaxDistance <= 0.f)
	{
		return nullptr;
	}

	const double HalfAngleDeg = FMath::Clamp(
		static_cast<double>(MaxHalfAngleDeg), 0.0, 180.0);
	const bool bCheckAngle = HalfAngleDeg < 180.0;
	const double CosThreshold = FMath::Cos(FMath::DegreesToRadians(HalfAngleDeg));
	const double MaxDistanceSquared = FMath::Square(static_cast<double>(MaxDistance));

	AActor* BestCandidate = nullptr;
	double BestDistanceSquared = TNumericLimits<double>::Max();
	double BestDirectionDot = -2.0;

	for (AActor* Candidate : GatherCandidates())
	{
		// [2] 자신을 제외하고 Actor의 유효성과 타겟 자격을 확인한다.
		// [2] 自身を除外し、Actorの有効性とターゲット資格を確認する。
		if (!IsValid(Candidate) || Candidate == GetOwner() ||
			!Candidate->Implements<UTargetable>() ||
			!ITargetable::Execute_IsTargetable(Candidate))
		{
			continue;
		}

		// [3] 수평 중심 거리를 검사한다. 같은 수평 위치는 방향을 정할 수 없어 제외한다.
		// [3] 水平中心間距離を確認する。同一水平位置は方向を決められないため除外する。
		FVector ToCandidate2D = Candidate->GetActorLocation() - Origin;
		ToCandidate2D.Z = 0.0;

		const double DistanceSquared = ToCandidate2D.SizeSquared();
		if (DistanceSquared > MaxDistanceSquared || DistanceSquared <= KINDA_SMALL_NUMBER)
		{
			continue;
		}

		// [4] 방향 일치도를 계산하고 허용 반각 밖의 후보를 제외한다.
		// 반각이 180도인 경우에도 방향 우선 비교를 위해 내적은 계산한다.
		// [4] 方向一致度を計算し、許容半角外の候補を除外する。
		// 半角が180度でも、方向優先の比較に使う内積は計算する。
		const FVector CandidateDirection = ToCandidate2D / FMath::Sqrt(DistanceSquared);
		const double DirectionDot = FVector::DotProduct(SearchDirection, CandidateDirection);

		if (bCheckAngle && DirectionDot < CosThreshold)
		{
			continue;
		}

		// [5] 입력이 있으면 방향 우선, 없으면 거리 우선으로 선택한다.
		// 주 기준이 같으면 다른 기준으로 비교하고, 둘 다 같으면 기존 후보를 유지한다.
		// [5] 入力があれば方向優先、なければ距離優先で選択する。
		// 主基準が同じなら他方で比較し、両方同じなら既存候補を維持する。
		bool bBetterCandidate;

		if (bHasDirectionalInput)
		{
			bBetterCandidate =
				DirectionDot > BestDirectionDot ||
				(DirectionDot == BestDirectionDot && DistanceSquared < BestDistanceSquared);
		}
		else
		{
			bBetterCandidate =
				DistanceSquared < BestDistanceSquared ||
				(DistanceSquared == BestDistanceSquared && DirectionDot > BestDirectionDot);
		}

		if (bBetterCandidate)
		{
			BestCandidate = Candidate;
			BestDistanceSquared = DistanceSquared;
			BestDirectionDot = DirectionDot;
		}
	}

	return BestCandidate;
}

bool UTargetSelectorComponent::IsValidCandidate(const AActor* TargetActor) const
{
	if (!TargetActor || !TargetActor->Implements<UTargetable>() || !ITargetable::Execute_IsTargetable(TargetActor))
	{
		return false;
	}

	for (const AActor* Candidate : GatherCandidates())
	{
		if (Candidate == TargetActor)
		{
			return true;
		}
	}

	return false;
}