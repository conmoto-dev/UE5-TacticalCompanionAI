// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AI/Components/TacticalFormationComponent.h"
#include "FormationBattleComponent.generated.h"


class USlotGeneratorStrategy;
class APartyCharacter;

/**
 * 전투 진형 컴포넌트 (골격).
 * anchor = 타겟 transform, 슬롯 = 절차적 생성(SlotGenerator Strategy).
 * 평시 FollowComponent와 (a)anchor (b)슬롯생성만 다르고, 나머지 기하 파이프라인은
 * 향후 공통 부모로 추출 예정 (지금은 Battle 단독 검증 우선 — Rule of Three).
 *
 * 戦闘隊形コンポーネント（骨格）。anchor=ターゲット、スロット=手続き生成。
 */
UCLASS(ClassGroup=(TacticalAI), meta=(BlueprintSpawnableComponent))
class TACTICALAI_API UFormationBattleComponent : public UTacticalFormationComponent
{
	GENERATED_BODY()

public:
	UFormationBattleComponent();
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	virtual void BeginPlay() override;

	UPROPERTY(EditAnywhere, Category = "Battle|Debug")
	TObjectPtr<AActor> DebugTargetActor;
	
	/** 외부(나중엔 [1] 타겟선정/Manager)에서 교전 타겟 지정. 지금은 에디터/테스트에서 직접 호출. */
	UFUNCTION(BlueprintCallable, Category = "Battle")
	void SetCombatTarget(AActor* InTarget) { CurrentTarget = InTarget; }

protected:
	// 절차적 슬롯 생성 전략. 디테일 패널에서 _Arc 등 선택 (Yield Strategy와 같은 Instanced 패턴).
	UPROPERTY(EditAnywhere, Instanced, Category = "Battle")
	TObjectPtr<USlotGeneratorStrategy> SlotGenerator;

	// 디자이너 기본 반경. 최종 반경 = 이 값 + 타겟의 EncircleRadius.
	UPROPERTY(EditAnywhere, Category = "Battle", meta = (ClampMin = "0.0"))
	float DesignerBaseRadius = 200.f;

	// 교전 타겟. 약참조 — 타겟 소멸 시 댕글링 방지 (anchor TOptional의 근거).
	TWeakObjectPtr<AActor> CurrentTarget;
	
private:
	// [a] 기준 프레임. 타겟 유효하면 그 transform, 아니면 미반환(파이프라인 정지).
	TOptional<FTransform> GetFormationAnchor() const;

	// 최종 반경 산출 (디자이너 기본 + 타겟 크기 보정). Strategy는 이 출처를 모른다.
	float ComputeBaseRadius() const;
	
protected:
	// =========================================================================
	// 슬롯 배정 저장 (진입 시 헝가리안으로 1회 결정, 전투 중 유지).
	// 매 틱 재배정하면 동료들이 자리 바꾸려 갈팡질팡 → 진입 시점에만 확정.
	// =========================================================================
	// 戦闘中の再割当は仲間が右往左往する原因。進入時に一度だけ確定する。
	UPROPERTY(Transient)
	TArray<TObjectPtr<APartyCharacter>> SlotAssignment;

	// 재배정 필요 플래그. 전투 진입(Activate) 시 true.
	bool bNeedsReassignment = true;

public:
	// 활성화 시 재배정 예약 (진입 트리거).
	virtual void Activate(bool bReset = false) override;
};