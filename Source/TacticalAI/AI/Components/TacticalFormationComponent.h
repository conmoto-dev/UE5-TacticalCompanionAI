// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "TacticalFormationComponent.generated.h"

class APartyManager;
class APartyCharacter;

/**
 * 진형 컴포넌트 공통 부모 (추상).
 * Follow/Battle이 공유하는 기하 파이프라인(환경보정 등)을 점진적으로 여기 모은다.
 * 자식은 방침 훅(anchor·슬롯생성)만 구현. 현재는 껍데기.
 *
 * 隊形コンポーネントの抽象基底。共通幾何パイプラインを段階的に集約。
 */
UCLASS(Abstract)
class TACTICALAI_API UTacticalFormationComponent : public UActorComponent
{
	GENERATED_BODY()
	
protected:
	// ───── 공통 파티 접근 (Owner = PartyManager 전제) ─────
	// Follow/Battle 둘 다 Manager에 부착되어 동료 목록을 받는다.
	
	APartyManager* GetOwningPartyManager() const;
	
	TArray<APartyCharacter*> GetPartyFollowers() const;
	
	// =========================================================================
	// 공통 환경보정 파이프라인 (Follow/Battle 공유).
	// 기준점은 AnchorOrigin (Follow=리더발밑, Battle=타겟위치)으로 통일.
	// IgnoreActor는 선택적 (Follow=리더 제외, Battle=nullptr).
	// =========================================================================

	// 4단계: 지면Z → NavMesh투영 → 벽슬라이드 → fallback.
	FVector AdjustLocationForEnvironment(const FVector& IdealLocation, const FVector& AnchorOrigin, const AActor* IgnoreActor = nullptr) const;

	bool TryProjectToNavMesh(const FVector& Point, FVector& OutResult) const;
	bool TryFindGroundZ(const FVector& Point, float& OutZ, const AActor* IgnoreActor = nullptr) const;
	bool TryCalculateWallSlide(const FVector& From, const FVector& To, const AActor* IgnoreActor, FVector& OutSlidLocation) const;
	FVector CalculateFallbackLocation(const FVector& AnchorOrigin, const FVector& IdealLocation) const;
};