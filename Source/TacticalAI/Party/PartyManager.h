// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PartyManager.generated.h"

class APartyCharacter;
class UFormationFollowComponent;
class UFormationDataAsset;

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
	
	/** Formation system component. Performs slot calculation and pushes targets to followers. */
	/** 隊形システム。スロット算出と仲間への目標座標プッシュを担当。 */
	UPROPERTY(VisibleAnywhere, Category="Formation")
	TObjectPtr<UFormationFollowComponent> FormationComponent;
};