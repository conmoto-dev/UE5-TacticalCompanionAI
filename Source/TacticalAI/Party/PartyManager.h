// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PartyManager.generated.h"

class APartyCharacter;
class UFormationDataAsset;
class UFormationFollowComponent;
class UFormationBattleComponent;
class UPartyPerceptionComponent;
class UTargetSelectorComponent;

UENUM(BlueprintType)
enum class EPartyFormationMode : uint8
{
	Follow  UMETA(DisplayName = "Follow"),
	Battle  UMETA(DisplayName = "Battle"),
};

/**
 * Hub for party state. Owns members, leader index, and the formation system.
 * Components/Characters query the manager but never modify each other directly.
 * パーティ状態のハブ。メンバー、リーダー、隊形システムを保有。
 * コンポーネントやキャラクターは互いに直接参照せず必ずManager経由で通信。
 */
UCLASS()
class TACTICALAI_API APartyManager : public AActor
{
	GENERATED_BODY()
	
public:	
	APartyManager();
	
	virtual void Tick(float DeltaTime) override;
	
	/** Returns the current leader character. */
	/** 現在のリーダーキャラクターを返す。 */
	APartyCharacter* GetLeader() const;

	/** Returns all members except the current leader. */
	/** リーダー以外の全メンバーを返す。 */
	TArray<APartyCharacter*> GetFollowers() const;
	
	/** Swap the current leader to a new index. Controller swap comes later. */
	/** リーダー切替。コントローラー実切替は今後のステップで実装。 */
	UFUNCTION(BlueprintCallable, Category="Party")
	void SwapLeader(int32 NewLeaderIndex);

protected:
	virtual void BeginPlay() override;

	/** All party members. Manually assigned in editor. */
	/** パーティ全員。エディタで手動割り当て。 */
	UPROPERTY(EditAnywhere, Category="Party")
	TArray<TObjectPtr<APartyCharacter>> Members;

	/** Index into Members pointing to the current leader. */
	/** Members配列内の現在リーダーのインデックス。 */
	UPROPERTY(EditAnywhere, Category="Party")
	int32 CurrentLeaderIndex = 0;
	
	/** Set Formation by current party state.(Idle, Battle, etc..)*/
public:
	/** 진형 모드 전환 (실행만 담당 — 전환 "결정"은 호출자/추후 StateTree). 멱등. */
	UFUNCTION(BlueprintCallable, Category="Formation")
	void SetFormationMode(EPartyFormationMode NewMode);

	/** 인지한 적 전체 조회 (유효한 것만). 모드 결정·포메이션·추후 타겟팅이 공유하는 단일 소스. */
	/** 知覚した敵全体を返す。モード判断・隊形・将来のターゲティングが共有する単一ソース。 */
	TArray<AActor*> GetPerceivedEnemies() const;
	
	// public 게터 추가 (추후 타겟 시스템이 그룹 단위 조회할 창구):
	UPartyPerceptionComponent* GetPerception() const { return PerceptionComponent; }
	
protected:
	/** Formation system component. Performs slot calculation and pushes targets to followers. */
	/** 隊形システム。スロット算出と仲間への目標座標プッシュを担当。 */
	UPROPERTY(VisibleAnywhere, Category="Formation")
	TObjectPtr<UFormationFollowComponent> FollowComponent;

	UPROPERTY(VisibleAnywhere, Category="Formation")
	TObjectPtr<UFormationBattleComponent> BattleComponent;
	
	/** Detect Battle State by Leader - Nearest Enemy Distance. */

	/** 적 그룹 인지 시스템. GetPerceivedEnemies의 실제 공급자. */
	/** 敵グループ知覚システム。GetPerceivedEnemiesの実供給元。 */
	UPROPERTY(VisibleAnywhere, Category="Perception")
	TObjectPtr<UPartyPerceptionComponent> PerceptionComponent;

	// 전투 진입 거리 (이보다 가까우면 Battle).
	UPROPERTY(EditAnywhere, Category="Formation|Switching", meta=(ClampMin="0.0"))
	float EnterBattleDistance = 700.f;

	// 전투 이탈 거리 (이보다 멀면 Follow). Enter보다 커야 함 (히스테리시스).
	UPROPERTY(EditAnywhere, Category="Formation|Switching", meta=(ClampMin="0.0"))
	float ExitBattleDistance = 1000.f;

private:
	// "결정" — 거리 재서 모드 판단 후 SetFormationMode 호출.
	// StateTree 이전 시 이 함수만 교체 (SetFormationMode는 그대로 생존).
	void TickModeDecision();
	
	EPartyFormationMode CurrentFormationMode = EPartyFormationMode::Follow;
};