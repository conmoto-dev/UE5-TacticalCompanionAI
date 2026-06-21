// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Characters/TacticalCharacterBase.h"
#include "PartyCharacter.generated.h"

class USpringArmComponent;
class UCameraComponent;
class UInputAction;
struct FInputActionValue;

/**
 * 동료(파티) 캐릭터. 진형에서 수동적 — 좌표를 FormationFollowComponent에서 받아
 * (UpdateTargetSlotLocation) AIController로 이동만. Yield 등 진형 방침 판단은 여기서 안 함.
 *
 * 리더 스왑으로 플레이어가 빙의하므로 카메라·입력을 직접 보유한다 — 적(AEnemyCharacter)과
 * 갈리는 지점이 바로 이것. 적은 빙의되지 않아 카메라/입력이 없다.
 *
 * 隊形における受動的存在。座標はFormationFollowComponentから受け取りAIControllerで移動。
 * リーダー交代で憑依されるためカメラ・入力を保有 — ここが敵との分岐点。
 */
UCLASS()
class TACTICALAI_API APartyCharacter : public ATacticalCharacterBase
{
	GENERATED_BODY()

public:
	APartyCharacter();

	/**
	 * 진형 시스템에서 목표 좌표 수신. bForceRefresh=true면 UpdateThreshold 캐시 우회(Yield 재발행용).
	 * 隊形システムからの目標座標受信。bForceRefresh=trueでキャッシュバイパス（Yield時の強制再発行）。
	 */
	UFUNCTION(BlueprintCallable, Category="Formation")
	void UpdateTargetSlotLocation(const FVector& NewTarget, bool bForceRefresh = false);

	// 이동 명령 캐시 무효화. 다음 UpdateTargetSlotLocation이 dedup 없이 강제 재발행.
	// traversal·스턴 등 이동을 일시 가로챈 기능이 *끝날 때* 호출 → 슬롯 복귀 보장.
	// 移動を一時的に奪った機能（traversal等）が終了時に呼ぶ。着地後のスロット復帰を保証。
	void InvalidateMoveCache();

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	// ───── 빙의 시 플레이어 입력 핸들러 (동료가 리더일 때만 의미) ─────
	// 憑依時のプレイヤー入力ハンドラ（仲間がリーダーの時のみ）。
	void Move(const FInputActionValue& Value);
	void Look(const FInputActionValue& Value);

public:
	// 컨트롤/UI 양쪽에서 들어오는 이동·시점·점프 입력 처리.
	// コントロール/UI両方から来る移動・視点・ジャンプ入力を処理。
	UFUNCTION(BlueprintCallable, Category="Input")
	void DoMove(float Right, float Forward);

	UFUNCTION(BlueprintCallable, Category="Input")
	void DoLook(float Yaw, float Pitch);

	UFUNCTION(BlueprintCallable, Category="Input")
	void DoJumpStart();

	UFUNCTION(BlueprintCallable, Category="Input")
	void DoJumpEnd();

	// 전투 포지셔닝 역할 조회. BattleComponent가 그룹 분류에 사용.
	// 戦闘ポジショニングの役割を返す。
	FGameplayTag GetCombatRole() const { return CombatRole; }

	// 이 캐릭터의 기준 교전 사거리 조회. 슬롯 생성 시 Context로 전달된다.
	// 戦闘の基準射程を返す。スロット生成時にContextへ渡る。
	float GetAttackRange() const { return AttackRange; }

	// 카메라 서브오브젝트 접근자.
	FORCEINLINE USpringArmComponent* GetCameraBoom() const { return CameraBoom; }
	FORCEINLINE UCameraComponent* GetFollowCamera() const { return FollowCamera; }

private:
	// ───── 빙의용 카메라 (동료 전용 — 적엔 없음) ─────
	// 카메라 붐. 캐릭터 뒤에서 충돌 시 끌어당김.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Camera", meta=(AllowPrivateAccess="true"))
	TObjectPtr<USpringArmComponent> CameraBoom;

	// 추종 카메라.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Camera", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UCameraComponent> FollowCamera;

	// ───── 입력 액션 (빙의 시 사용) ─────
	UPROPERTY(EditAnywhere, Category="Input")
	TObjectPtr<UInputAction> JumpAction;

	UPROPERTY(EditAnywhere, Category="Input")
	TObjectPtr<UInputAction> MoveAction;

	UPROPERTY(EditAnywhere, Category="Input")
	TObjectPtr<UInputAction> LookAction;

	UPROPERTY(EditAnywhere, Category="Input")
	TObjectPtr<UInputAction> MouseLookAction;

	// ───── 진형 이동 상태 ─────
	// AIController에 마지막 전달한 목표. threshold 기반 MoveTo 중복 억제 캐시.
	// AIControllerに最後に渡した目標。重複MoveTo抑制用キャッシュ。
	UPROPERTY(VisibleAnywhere, Category="Formation")
	FVector CurrentTargetLocation;

	// MoveTo 재발행에 필요한 최소 이동 거리. 매 프레임 경로 재계산 억제.
	// MoveTo再発行に必要な最小移動距離。毎フレームのパス再計算を抑制。
	UPROPERTY(EditAnywhere, Category="Formation")
	float UpdateThreshold = 50.f;

	// MoveTo 도달 판정 반경. 이 거리 안에 들어오면 정지.
	// MoveTo到達判定半径。
	UPROPERTY(EditAnywhere, Category="Formation")
	float AcceptanceRadius = 30.f;

	UPROPERTY(VisibleAnywhere, Category="Tactical AI")
	TObjectPtr<class UPlayerCrowdAgentComponent> PlayerAgentComp;

	// ───── 전투 정체성 ─────
	// 전투 시 포지셔닝 역할. Role.Combat 아래 태그만 선택 가능. 기본 Melee(생성자).
	// 戦闘時のポジショニング役割。Role.Combat配下のみ選択可。
	UPROPERTY(EditAnywhere, Category="Combat", meta=(Categories="Role.Combat"))
	FGameplayTag CombatRole;

	// 기준 교전 사거리. 원거리 안전 위치 계산의 기준 거리.
	// 디폴트 평타 기준값 — 스킬별 사거리는 추후 행동 레이어에서 덮어쓴다.
	// 基準射程。スキル別射程は後の行動レイヤーで上書き。
	UPROPERTY(EditAnywhere, Category="Combat", meta=(ClampMin="0.0"))
	float AttackRange = 100.f;
};