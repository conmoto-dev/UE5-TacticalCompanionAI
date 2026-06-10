// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "TacticalAICharacter.h"
#include "PartyCharacter.generated.h"

/**
 * Party member character. Passive in formation: receives target coordinates from
 * FormationFollowComponent (via UpdateTargetSlotLocation) and moves there via AIController.
 * Yield / formation policy decisions are NOT made here — they live in the component.
 *
 * 隊形における受動的存在。座標はFormationFollowComponentから受け取り、
 * AIControllerで移動するだけ。Yield等の隊形方針判断はここでは行わない。
 */
UCLASS()
class TACTICALAI_API APartyCharacter : public ATacticalAICharacter
{
	GENERATED_BODY()

public:
	APartyCharacter();

	/**
	 * Receive a target coordinate from the formation system.
	 * bForceRefresh=true bypasses the UpdateThreshold cache (used for Yield re-pushing).
	 * 隊形システムからの目標座標受信。
	 * bForceRefresh=trueでキャッシュバイパス（Yield時の強制再発行用）。
	 */
	UFUNCTION(BlueprintCallable, Category="Formation")
	void UpdateTargetSlotLocation(const FVector& NewTarget, bool bForceRefresh = false);
	
protected:
	virtual void BeginPlay() override;

public:	
	virtual void Tick(float DeltaTime) override;
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;
	
	// 이동 명령 캐시 무효화. 다음 UpdateTargetSlotLocation이 dedup 없이 강제 재발행.
	// traversal·스턴 등 이동을 일시 가로챈 기능이 *끝날 때* 호출 → 슬롯 복귀 보장.
	// 移動を一時的に奪った機能（traversal等）が終了時に呼ぶ。着地後のスロット復帰を保証。
	void InvalidateMoveCache();
private:
	// Last target sent to AIController. Used for threshold-based MoveTo deduplication.
	// AIControllerに最後に渡した目標。重複MoveTo抑制用キャッシュ。
	UPROPERTY(VisibleAnywhere, Category="Formation")
	FVector CurrentTargetLocation;

	// Minimum distance change required to re-issue MoveTo. Prevents per-frame path replanning.
	// MoveTo再発行に必要な最小移動距離。毎フレームのパス再計算を抑制。
	UPROPERTY(EditAnywhere, Category="Formation")
	float UpdateThreshold = 50.f;

	// MoveTo acceptance radius. Character stops when within this distance.
	// MoveTo到達判定半径。
	UPROPERTY(EditAnywhere, Category="Formation")
	float AcceptanceRadius = 30.f;
	
	UPROPERTY(VisibleAnywhere, Category="Tactical AI")
	TObjectPtr<class UPlayerCrowdAgentComponent> PlayerAgentComp;
	
	
public:
	// 전투 포지셔닝 역할 조회. BattleComponent가 그룹 분류에 사용.
	// 戦闘ポジショニングの役割を返す。
	FGameplayTag GetCombatRole() const { return CombatRole; }
private:
	// 전투 시 포지셔닝 역할. Role.Combat 아래 태그만 선택 가능. 기본 Melee(생성자).
	// 戦闘時のポジショニング役割。Role.Combat配下のみ選択可。
	UPROPERTY(EditAnywhere, Category="Combat", meta=(Categories="Role.Combat"))
	FGameplayTag CombatRole;
};