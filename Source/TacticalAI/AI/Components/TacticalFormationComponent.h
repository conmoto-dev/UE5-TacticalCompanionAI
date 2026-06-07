// Fill out your copyright notice in the Description page of Project Settings.
#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Algorithms/HungarianMatchingLibrary.h"
#include "TacticalFormationComponent.generated.h"

class APartyManager;
class APartyCharacter;

/**
 * 진형 컴포넌트 공통 부모 (추상).
 * Follow/Battle이 공유하는 기하 파이프라인(환경보정·슬롯배정)을 모은다.
 * 자식은 방침 훅(anchor·슬롯생성)만 구현.
 *
 * 隊形コンポーネントの抽象基底。Follow/Battleが共有する幾何パイプライン
 * （環境補正・スロット割当）を集約。子は方針フック（anchor・スロット生成）のみ実装。
 */
UCLASS(Abstract)
class TACTICALAI_API UTacticalFormationComponent : public UActorComponent
{
	GENERATED_BODY()

protected:
	// =========================================================================
	// 공통 파티 접근 (Owner = PartyManager 전제).
	// Follow/Battle 둘 다 Manager에 부착돼 같은 경로로 동료 목록을 받는다.
	// =========================================================================
	// 両コンポーネントが同じ経路で仲間リストを取得する。
	APartyManager* GetOwningPartyManager() const;
	TArray<APartyCharacter*> GetPartyFollowers() const;

	// =========================================================================
	// 공통 환경보정 파이프라인 (Follow/Battle 공유).
	// 이상 슬롯 좌표를 지형(NavMesh·벽·슬로프)에 맞게 보정.
	// 기준점은 AnchorOrigin으로 통일 (Follow=리더발밑, Battle=타겟위치).
	// IgnoreActor는 선택적 (Follow=리더 제외, Battle=nullptr).
	// =========================================================================
	// 依存はリーダーではなく「隊形の基準点(AnchorOrigin)」。
	// これによりFollow/Battleが同一実装を共有できる。
	FVector AdjustLocationForEnvironment(const FVector& IdealLocation, const FVector& AnchorOrigin, const AActor* IgnoreActor = nullptr) const;
	bool TryProjectToNavMesh(const FVector& Point, FVector& OutResult) const;
	bool TryFindGroundZ(const FVector& Point, float& OutZ, const AActor* IgnoreActor = nullptr) const;
	bool TryCalculateWallSlide(const FVector& From, const FVector& To, const AActor* IgnoreActor, FVector& OutSlidLocation) const;
	FVector CalculateFallbackLocation(const FVector& AnchorOrigin, const FVector& IdealLocation) const;

	// =========================================================================
	// 공통 슬롯 배정 (Formation 전용 래퍼).
	// 순수 헝가리안 알고리즘은 라이브러리에 있고, 여기선 거리 비용행렬만 구성해 호출.
	// 멤버 비의존 순수 함수 → Follow(캐시 좌표)/Battle(매 틱 좌표) 둘 다 사용 가능.
	// return[slotIdx] = 그 슬롯에 들어갈 동료.
	// =========================================================================
	// 純粋関数（メンバー非依存）。距離コスト行列を構築しライブラリのSolveAssignmentを呼ぶ。
	TArray<APartyCharacter*> SolveSlotAssignment(
		const TArray<APartyCharacter*>& Occupants,
		const TArray<FVector>& SlotLocations) const;
};