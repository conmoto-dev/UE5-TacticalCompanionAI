#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CombatMicroMovementComponent.generated.h"

class UTargetSelectorComponent;
class ACharacter;

// =======================================================
// 횡이동 한 번의 실행 상태
// 결과는 다음 횡이동이 시작될 때까지 유지한다.
//
// 一回の横移動の実行状態。
// 結果は次の横移動が開始されるまで保持する。
// =======================================================
UENUM(BlueprintType)
enum class ECombatStrafeState : uint8
{
	Idle,
	Running,
	Completed,
	Interrupted
};

// =======================================================
// 홈 슬롯 주변의 전투 미세 이동
// 외부 요청마다 방향과 지속시간을 정하여 한 번 이동한다.
// 홈 슬롯과 대상을 참조하며, MoveTo 진행 중에는 이동과 주시를 중단한다.
// 홈 슬롯 반경은 방향 선정 기준이며 이동 범위를 제한하지 않는다.
//
// ホームスロット周辺の戦闘微細移動。
// 外部要求ごとに方向と持続時間を決め、一回移動する。
// ホームスロットと対象を参照し、MoveTo中は移動と注視を中断する。
// ホームスロット半径は方向選択の基準であり、移動範囲を制限しない。
// =======================================================
UCLASS(ClassGroup=(TacticalAI), meta=(BlueprintSpawnableComponent))
class TACTICALAI_API UCombatMicroMovementComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UCombatMicroMovementComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// =======================================================
	// 횡이동 실행 요청·취소·결과 조회
	// 한 번에 하나의 요청만 실행한다.
	//
	// 横移動の実行要求・キャンセル・結果取得。
	// 同時に実行する要求は一つに限定する。
	// =======================================================

	// 횡이동을 한 번 시작한다. 실행 중이거나 시작 조건을 만족하지 않으면 false.
	// 一回の横移動を開始する。実行中、または開始条件を満たさない場合はfalse。
	UFUNCTION(BlueprintCallable, Category = "MicroMovement")
	bool StartStrafe();

	// 실행 중인 이동을 중단한다. 종료된 결과와 조건이 유효한 대상 주시는 유지한다.
	// 実行中の移動を中断する。終了済みの結果と、条件が有効な対象注視は維持する。
	UFUNCTION(BlueprintCallable, Category = "MicroMovement")
	void CancelStrafe();

	// 최근 횡이동의 실행 상태.
	// 直近の横移動の実行状態。
	UFUNCTION(BlueprintPure, Category = "MicroMovement")
	ECombatStrafeState GetStrafeState() const { return StrafeState; }

	// 외부 일시 정지. true이면 횡이동과 대상 주시를 즉시 중단한다.
	// 外部からの一時停止。trueなら横移動と対象注視を即座に中断する。
	UFUNCTION(BlueprintCallable, Category = "MicroMovement")
	void SetSuppressed(bool bInSuppressed);

protected:
	// ───── 미세 이동 튜닝 ─────

	// 이동 속도 배율 (캐릭터 최고 속도 대비). 전투 중 위치 조정이므로 낮게.
	// 移動速度倍率（キャラ最高速度比）。戦闘中の位置調整なので低め。
	UPROPERTY(EditAnywhere, Category = "MicroMovement|Strafe", meta = (ClampMin = "0.05", ClampMax = "1.0"))
	float StrafeSpeedScale = 0.25f;
	
	// 한 번의 이동 지속시간 범위. 외부 취소나 동작 조건 상실 시에는 도중에 중단한다.
	// 一回の移動持続時間の範囲。外部キャンセルや動作条件の喪失時は途中で中断する。
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
	
	// 이동 입력 제공을 끝내고 결과를 기록한다. 대상 주시는 별도로 관리한다.
	// 移動入力の供給を終了し、結果を記録する。対象注視は別途管理する。
	void FinishStrafe(bool bCompleted);
	
#if ENABLE_DRAW_DEBUG
	// 디버그 표시 (CVar: TacticalAI.DebugMicroMovement). 반경·이동 목표·상태.
	// デバッグ表示。半径・移動目標・状態。
	void DrawDebug(bool bActive, const FVector& HomeSlot) const;
#endif
	
	// 이동 시작/종료 시 회전 방식 전환 (이동 방향 자동 회전 ↔ 타겟 방향 고정).
	// 移動開始/終了時の回転方式切替。
	void BeginCombatFacing();
	void EndCombatFacing();
	
	// 개별 횡이동 실행 상태
	// 종료 결과는 다음 이동이 시작될 때까지 유지한다.
	// 個別の横移動実行状態。
	// 終了結果は次の移動が開始されるまで保持する。
	ECombatStrafeState StrafeState = ECombatStrafeState::Idle;

	bool bCombatFacing = false;
	
	// 외부 일시 정지 상태 (공격 중 등).
	bool bSuppressed = false;

	// 자유 이동 각도의 하한/상한(도). 적 방향 0도 기준 좌우 크기.
	// 自由移動角度の下限/上限(度)。全キャラ共通ルールのためエディタ非公開。
	static constexpr float FreeMoveAngleMinDeg = 50.f;
	static constexpr float FreeMoveAngleMaxDeg = 120.f;
	
	// 현재 이동 방향(월드 좌표, 이동 시작 시 확정)과 남은 이동 시간.
	// 現在の移動方向（ワールド座標、移動開始時に確定）と残り移動時間。
	FVector StrafeDirection = FVector::ZeroVector;
	float StrafeTimeRemaining = 0.f;
	
	// 회전 방식 복원용 원본 값 (이동 시작 시 저장, 종료 시 복원).
	bool bSavedOrientRotationToMovement = true;
};