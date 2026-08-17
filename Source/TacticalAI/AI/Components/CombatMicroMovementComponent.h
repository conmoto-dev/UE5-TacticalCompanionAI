#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CombatMicroMovementComponent.generated.h"

class UTargetSelectorComponent;
class ACharacter;

// =========================================================================
// 슬롯 근처 미세 이동(전투, 탐색 등). 이동 목표는 항상 홈 슬롯 반경(HomeTolerance) 안.
// 읽기 전용 소비자: 홈 슬롯(IHomeSlotProvider)·타겟(셀렉터)을 읽기만 한다.
// 이동은 AddMovementInput — MoveTo(재배치·추종·트래버설)가 진행 중이면 무조건 양보한다.
// =========================================================================
// 戦闘中の微細移動。ホームスロット半径内で、攻撃の合間にターゲットを見たまま小さく動く。
// 読み取り専用消費者。MoveTo進行中は無条件で譲る。
UCLASS(ClassGroup=(TacticalAI), meta=(BlueprintSpawnableComponent))
class TACTICALAI_API UCombatMicroMovementComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UCombatMicroMovementComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// 외부(어빌리티 등)의 일시 정지 스위치. 공격 시작 시 true, 종료 시 false.
	// GAS 도입 전까지의 최소 연결 지점 — 어빌리티가 생기면 발동/종료에서 이걸 토글한다.
	// 外部からの一時停止スイッチ。攻撃開始でtrue、終了でfalse。
	UFUNCTION(BlueprintCallable, Category = "MicroMovement")
	void SetSuppressed(bool bInSuppressed) { bSuppressed = bInSuppressed; }

protected:
	// ───── 미세 이동 튜닝 ─────

	// 이동과 이동 사이의 대기 시간 범위(초). 매번 이 안에서 랜덤 — 멤버 전원이 같은 박자로 움직이는 것 방지.
	// 移動間の待機時間範囲(秒)。毎回ランダム — 全員が同じ拍子で動くのを防ぐ。
	UPROPERTY(EditAnywhere, Category = "MicroMovement|Strafe", meta = (ClampMin = "0.1", Units = "s"))
	FVector2D StrafeIntervalRange = FVector2D(1.5f, 3.5f);

	// 이동 속도 배율 (캐릭터 최고 속도 대비). 전투 중 위치 조정이므로 낮게.
	// 移動速度倍率（キャラ最高速度比）。戦闘中の位置調整なので低め。
	UPROPERTY(EditAnywhere, Category = "MicroMovement|Strafe", meta = (ClampMin = "0.05", ClampMax = "1.0"))
	float StrafeSpeedScale = 0.25f;
	
	// 한 번의 이동 지속 시간 범위(초). 이 시간은 반드시 완주 — 이동을 중간에 자르지 않는다.
	// 최소 이동량 = 속도 × 하한. 짧은 잔걸음이 구조적으로 안 나온다.
	// 一回の移動持続時間(秒)。必ず完走 — 途中で切らない。最小移動量＝速度×下限。
	UPROPERTY(EditAnywhere, Category = "MicroMovement|Strafe", meta = (ClampMin = "0.5", Units = "s"))
	FVector2D StrafeDurationRange = FVector2D(2.f, 3.f);
	
	// ───── 홈 슬롯 ─────

	// 홈 슬롯 반경(cm).
	// 이동 시작 시점에 이 반경 안이면 자유 방향, 밖이면 홈 슬롯 방향으로 복귀 이동.
	// ホームスロット半径(cm)。壁ではなく方向規則の基準：内なら自由方向、外なら復帰方向。
	UPROPERTY(EditAnywhere, Category = "MicroMovement|Home", meta = (ClampMin = "50.0", Units = "cm"))
	float HomeTolerance = 300.f;

	// ───── 회전 ─────

	// 타겟 방향 회전 보간 속도. 이동 방향과 무관하게 몸은 항상 타겟 쪽을 향한다.
	// ターゲット方向への回転補間速度。移動方向に関係なく体は常にターゲットへ。
	UPROPERTY(EditAnywhere, Category = "MicroMovement", meta = (ClampMin = "0.5"))
	float FaceTargetInterpSpeed = 6.f;

private:
	// 이번 틱에 미세 이동을 해도 되는 상황인지 순서대로 검사. 하나라도 실패면 아무것도 안 한다.
	// 今ティックに微細移動して良い状況か順に検査。一つでも失敗なら何もしない。
	bool PassesActivationChecks(FVector& OutHomeSlot, const AActor*& OutTarget) const;

	// 이동 방향 결정. 반경 안 = 적 기준 ±허용각 랜덤, 반경 밖 = 홈 슬롯 방향(복귀).
	// 移動方向の決定。半径内＝敵基準ランダム、半径外＝ホームスロット方向（復帰）。
	bool TryPickStrafeDirection(const FVector& HomeSlot, const AActor& Target, FVector& OutDirection) const;
	
#if ENABLE_DRAW_DEBUG
	// 디버그 표시 (CVar: TacticalAI.DebugMicroMovement). 반경·이동 목표·상태.
	// デバッグ表示。半径・移動目標・状態。
	void DrawDebug(bool bActive, const FVector& HomeSlot) const;
#endif
	
	// 이동 시작/종료 시 회전 방식 전환 (이동 방향 자동 회전 ↔ 타겟 방향 고정).
	// 移動開始/終了時の回転方式切替。
	void BeginStrafeFacing();
	void EndStrafeFacing();

	// 현재 이동 목표를 향해 이동 중인가.
	bool bStrafing = false;

	// 외부 일시 정지 상태 (공격 중 등).
	bool bSuppressed = false;

	// 자유 이동 각도의 하한/상한(도). 적 방향 0도 기준 좌우 크기.
	// 自由移動角度の下限/上限(度)。全キャラ共通ルールのためエディタ非公開。
	static constexpr float FreeMoveAngleMinDeg = 50.f;
	static constexpr float FreeMoveAngleMaxDeg = 120.f;
	
	// 현재 이동 방향(월드, 이동 시작 시 확정) / 남은 이동 시간 / 다음 이동까지 대기 시간.
	FVector StrafeDirection = FVector::ZeroVector;
	float StrafeTimeRemaining = 0.f;
	float TimeUntilNextStrafe = 0.f;
	
	// 회전 방식 복원용 원본 값 (이동 시작 시 저장, 종료 시 복원).
	bool bSavedOrientRotationToMovement = true;
};