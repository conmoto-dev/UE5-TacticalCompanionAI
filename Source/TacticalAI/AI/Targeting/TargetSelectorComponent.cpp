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

	// [2] 주기 평가. 만료 시 보관된 사유로 평가 후, 사유 리셋 + 다음 주기 지터 재추첨.
	// 周期評価。満了で保管中の理由で評価し、理由リセット＋次周期をジッターで再抽選。
	TimeUntilNextEvaluation -= DeltaTime;
	if (TimeUntilNextEvaluation <= 0.f)
	{
		EvaluateTargets(PendingReason);
		PendingReason = ETargetChangeReason::Periodic;
		TimeUntilNextEvaluation = RollNextInterval();
	}

#if ENABLE_DRAW_DEBUG
	if (CVarDebugTargetSelector.GetValueOnGameThread())
	{
		if (const AActor* Target = CurrentTarget.Get())
		{
			DrawDebugLine(GetWorld(), GetOwner()->GetActorLocation(),
				Target->GetActorLocation(), FColor::Magenta, false, -1.f, 0, 1.5f);
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

	// [3] 커밋. 교체 마진 없음 — 그 시점 최고점을 그대로 채택
	//     타겟을 공격 불가능한 위치일 시 처리는 Formation에서 처리되어있음. 이동 후 바로 타겟 변경은 자연스러운 결과? 인게임 평가 필요.
	// コミット。切替マージンなし — 動的な切替は抑制対象ではない（実測後に再検討）。
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