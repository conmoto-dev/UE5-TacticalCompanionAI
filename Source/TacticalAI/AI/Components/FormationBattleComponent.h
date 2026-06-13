// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AI/Components/TacticalFormationComponent.h"
#include "GameplayTagContainer.h"
#include "FormationBattleComponent.generated.h"


class USlotGeneratorStrategy;
class APartyCharacter;

// =========================================================================
// 역할군별 슬롯 생성 설정 (디자이너 편집).
// "어느 역할이 어떤 Strategy로, 얼마나 떨어져 서나"는 정책 → 데이터로 분리.
// 컴포넌트는 역할 목록을 하드코딩하지 않는다 — 이 배열을 순회할 뿐.
// (역할군 추가 = 엔트리 추가, 컴포넌트 코드 수정 없음)
// =========================================================================
// 役割ごとのスロット生成設定。役割→Strategyの対応はポリシーなのでデータ側へ。
// コンポーネントは役割一覧を知らない（役割追加＝エントリ追加のみ）。
USTRUCT()
struct FRoleSlotConfig
{
	GENERATED_BODY()

	// 이 설정이 적용될 전투 역할. / この設定が適用される戦闘役割。
	UPROPERTY(EditAnywhere, Category = "Battle", meta = (Categories = "Role.Combat"))
	FGameplayTag Role;

	// 이 역할군의 슬롯 생성 전략. / この役割グループのスロット生成戦略。
	UPROPERTY(EditAnywhere, Instanced, Category = "Battle")
	TObjectPtr<USlotGeneratorStrategy> SlotGenerator;

	// 기본 반경에 더할 역할별 오프셋. 원거리는 양수로 멀리 세움. / 基本半径への役割別オフセット。
	UPROPERTY(EditAnywhere, Category = "Battle")
	float RadiusOffset = 0.f;
	
	// 그룹 배치 순서. 낮을수록 먼저 자리를 잡음 (먼저 잡은 슬롯이 점유로 누적됨).
	// 이 순서는 "근접이 전선을 먼저 형성하고 원거리가 그 뒤 안전 위치를 고른다"는 게임 디자인
	// 配置順序は設計意図。近接が前線を作り、遠隔がその後ろに位置する。
	UPROPERTY(EditAnywhere)
	int32 PlacementPriority = 0;
};

// 역할군별 런타임 상태 (헝가리안 배정 결과 유지).
// weak ptr이므로 GC 추적 배관(USTRUCT+UPROPERTY) 불필요 — 캐릭터 소멸 시 자동 null.
// weak ptrなのでGC追跡不要。キャラ消滅時は自動でnull化。
struct FRoleGroupRuntime
{
	TArray<TWeakObjectPtr<APartyCharacter>> SlotAssignment;
};

/**
 * 전투 진형 컴포넌트.
 * anchor = 타겟 transform, 슬롯 = 절차적 생성(SlotGenerator Strategy).
 * 동료를 CombatRole 태그로 그룹 분리 → 그룹별로 Strategy·헝가리안을 독립 수행.
 * 비용행렬이 그룹 안에 갇히므로 역할 교차 배정(근거리가 원거리 슬롯에 등)이 구조적으로 불가.
 *
 * 戦闘隊形コンポーネント。役割タグでグループ分離し、Strategy・ハンガリアンを
 * グループ毎に独立実行。コスト行列が混ざらないため役割交差の割当は構造的に不可能。
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
	// 역할군별 슬롯 생성 설정. 태그 미지정/미등록 역할은 Melee 설정으로 폴백.
	// 役割別スロット生成設定。未設定の役割はMeleeへフォールバック。
	UPROPERTY(EditAnywhere, Category = "Battle")
	TArray<FRoleSlotConfig> RoleSlotConfigs;

	// 디자이너 기본 반경. 최종 반경 = 이 값 + 타겟의 EncircleRadius + 역할별 RadiusOffset.
	UPROPERTY(EditAnywhere, Category = "Battle", meta = (ClampMin = "0.0"))
	float DesignerBaseRadius = 200.f;

	// 교전 타겟. 약참조 — 타겟 소멸 시 댕글링 방지 (anchor TOptional의 근거).
	TWeakObjectPtr<AActor> CurrentTarget;
	
private:
	// [a] 기준 프레임. 타겟 유효하면 그 transform, 아니면 미반환(파이프라인 정지).
	TOptional<FTransform> GetFormationAnchor() const;

	// 최종 반경 산출 (디자이너 기본 + 타겟 크기 보정). Strategy는 이 출처를 모른다.
	float ComputeBaseRadius() const;

	// 역할 태그에 해당하는 설정 검색. 없으면 nullptr (호출부가 폴백 결정).
	const FRoleSlotConfig* FindConfigForRole(const FGameplayTag& Role) const;
	
protected:
	// =========================================================================
	// 역할군별 슬롯 배정 저장 (진입 시 헝가리안으로 그룹별 1회 결정, 전투 중 유지).
	// 매 틱 재배정하면 동료들이 자리 바꾸려 갈팡질팡 → 진입 시점에만 확정.
	// =========================================================================
	// 役割グループ毎の割当保存。戦闘中の再割当は右往左往の原因。進入時のみ確定。
	TMap<FGameplayTag, FRoleGroupRuntime> RuntimeGroups;

	// 재배정 필요 플래그. 전투 진입(Activate) 시 true. 모든 그룹에 공통 적용.
	bool bNeedsReassignment = true;

public:
	// 활성화 시 재배정 예약 (진입 트리거).
	virtual void Activate(bool bReset = false) override;
};