#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AI/Targeting/TargetScorePolicy.h"
#include "TargetSelectorComponent.generated.h"

// ターゲット遷移の理由。ログと再評価経路の区別用。
UENUM()
enum class ETargetChangeReason : uint8
{
	Periodic,       // 주기 평가에서 더 좋은 후보 발견
	TargetLost,
	NoCandidates,
};

// =========================================================================
// Target Selector Component (추상 베이스)
//
// 캐릭터별 타겟 "선정"의 주인. 선정만 하고 위치는 절대 정하지 않는다 (그건 Formation).
// 평가 루프(지터 주기·무효화 재평가·반응 지연·커밋·전환 로그)를 소유하며,
// 진영을 모른다 — 측별 차이는 두 훅(BuildContext / GatherCandidates)에만 존재.
// 데이터는 평가 시점에 공급원에서 Pull, 타겟 소멸만 이벤트(Push)로 받는다.
//
// キャラ別ターゲット「選定」の所有者。位置決定はFormationの責務。
// 評価ループを所有し陣営を知らない — 陣営差は2フックのみ。
// データは評価時にPull、ターゲット消滅のみイベントで受ける。
// =========================================================================
UCLASS(Abstract)
class TACTICALAI_API UTargetSelectorComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UTargetSelectorComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** 현재 커밋된 타겟. 무효 시 nullptr. */
	/** 現在コミット中のターゲット。無効時はnullptr。 */
	UFUNCTION(BlueprintCallable, Category = "Targeting")
	AActor* GetCurrentTarget() const { return CurrentTarget.Get(); }

protected:
	virtual void BeginPlay() override;

	// =========================================================================
	// 측별 차이 훅. 자식(파티/적)은 이 둘만 구현한다.
	//
	// PURE_VIRTUAL인 이유: UCLASS는 C++ 순수 가상을 못 쓴다 — Abstract여도
	// CDO(클래스 기본 객체)는 생성돼야 해서 인스턴스화 가능해야 함.
	// 이 매크로는 "본체 없음"을 런타임 assert로 대체하는 UE 관용구.
	//
	// 陣営差フック。子(パーティ/敵)はこの2つのみ実装。
	// UCLASSはC++純粋仮想不可（CDO生成のため）— UE慣用のマクロで代替。
	// =========================================================================
	virtual FTargetingContext BuildContext() const
		PURE_VIRTUAL(UTargetSelectorComponent::BuildContext, return FTargetingContext(););

	virtual TArray<AActor*> GatherCandidates() const
		PURE_VIRTUAL(UTargetSelectorComponent::GatherCandidates, return TArray<AActor*>(););

private:
	void EvaluateTargets(ETargetChangeReason Reason);
	void CommitTarget(AActor* NewTarget, ETargetChangeReason Reason);
	void ScheduleReactionReevaluation();
	float RollNextInterval() const;

	UFUNCTION()
	void HandleTargetDestroyed(AActor* DestroyedActor);

private:
	// ターゲット採点ポリシー構成。キャラ個性の表現点。
	UPROPERTY(EditAnywhere, Category = "Targeting")
	TArray<FWeightedTargetPolicy> Policies;

	// 재평가 기본 주기 (초).
	// 再評価の基本周期（秒）。短いほど機敏にターゲットを切り替える。
	UPROPERTY(EditAnywhere, Category = "Targeting", meta = (ClampMin = "0.05", Units = "s"))
	float EvaluateInterval = 0.5;

	// 주기 지터 비율. 매 사이클 주기를 ±이 비율로 흔들어 파티 전원의 평가 동기화를 방지.
	// 周期ジッター率。毎サイクル周期を揺らし、パーティ全員の評価同期を防ぐ。
	UPROPERTY(EditAnywhere, Category = "Targeting", meta = (ClampMin = "0.0", ClampMax = "0.9"))
	float IntervalJitter = 0.25f;

	// 타겟 상실 시 재선정까지의 반응 지연 (X=최소, Y=최대에서 랜덤).
	// 전원이 같은 프레임에 갈아타는 기계적 동기화를 막는 "사람 반응 시간" 연출.
	// ターゲット喪失時の反応遅延。同一フレーム切替の機械感を防ぐ「人間の反応時間」。
	UPROPERTY(EditAnywhere, Category = "Targeting")
	FVector2D ReactionDelayRange = FVector2D(0.1f, 0.35f);

	// 현재 타겟. 약참조 — 소멸 시 자동 무효화, 소유 아님.
	// 現在のターゲット。弱参照 — 消滅時に自動無効化。
	TWeakObjectPtr<AActor> CurrentTarget;

	// 다음 평가까지 남은 시간.
	// 次評価までの残時間。TickIntervalの位相整列を避けるため手動累積。
	float TimeUntilNextEvaluation = 0.f;

	// 현재 타겟 강제 유지 잔여 시간. 결정 주도 정책의 HoldDuration과 연계.
	// 양수인 동안 주기 평가를 스킵한다. 타겟 상실 시엔 즉시 0.
	// ターゲット強制維持の残時間。正の間は周期評価をスキップ。喪失時は即0。
	float RemainingHoldTime = 0.f;
	
	// 다음 평가에 적용할 사유.
	// 次評価に適用する理由。
	ETargetChangeReason PendingReason = ETargetChangeReason::Periodic;
};