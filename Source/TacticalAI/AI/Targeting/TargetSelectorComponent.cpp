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