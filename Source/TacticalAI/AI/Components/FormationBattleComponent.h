// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AI/Components/TacticalFormationComponent.h"
#include "GameplayTagContainer.h"
#include "FormationBattleComponent.generated.h"


class USlotGeneratorStrategy;
class APartyCharacter;
struct FSlotGenContext;

// =========================================================================
// 역할군별 슬롯 생성 설정 (디자이너 편집).
// "어느 역할이 어떤 Strategy로, 얼마나 떨어져 서나"는 정책 → 데이터로 분리.
// 컴포넌트는 역할 목록을 하드코딩하지 않는다 — 이 배열을 순회할 뿐.
// =========================================================================
// 役割ごとのスロット生成設定。役割→Strategyの対応はポリシーなのでデータ側へ。
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
	// 配置順序は設計意図。近接が前線を作り、遠隔がその後ろに位置する。
	UPROPERTY(EditAnywhere)
	int32 PlacementPriority = 0;
};

// 역할군별 런타임 상태 (집합형 헝가리안 배정 결과 유지).
// weak ptr이므로 GC 추적 배관 불필요 — 캐릭터 소멸 시 자동 null.
// weak ptrなのでGC追跡不要。キャラ消滅時は自動でnull化。
struct FRoleGroupRuntime
{
	TArray<TWeakObjectPtr<APartyCharacter>> SlotAssignment;
};

// =========================================================================
// 유닛 1명의 재배치 커밋 상태 (개별형/MemberSpecific 전용).
// "어디로 가기로 했나"를 들고, 검증·홀드의 기준점이 된다. 매 틱 재생성 대신
// 이 커밋에 묶여 있다가, 트리거가 떴을 때만 갱신된다(= 커밋 잠금 → 이동 중 정지 방지).
// =========================================================================
// 1ユニットの再配置コミット状態。検証対象はlive位置でなくCommittedSlot（移動中ノイズ遮断）。
struct FCommitSnapshot
{
	// 가기로 확정한 슬롯(환경보정까지 끝난 월드 좌표). 검증·홀드·점유의 기준.
	FVector CommittedSlot = FVector::ZeroVector;

	// 마지막 커밋 시각(초). 재배치 직후의 이동 거부감(reluctance) 산출 기준.
	// コミット時刻。再配置直後の「移動したくなさ」算出基準。
	float CommitTime = 0.f;

	// 한 번이라도 커밋했는가. false면 첫 평가에서 강제 초기 배치.
	bool bHasCommitted = false;
};

/**
 * 전투 진형 컴포넌트.
 * anchor = 타겟 transform. 동료를 CombatRole 태그로 그룹 분리 → 그룹별 Strategy.
 *
 * 배정 정책 2갈래:
 *  - GroupHungarian(Arc/근접): 슬롯을 집합 생성 → 진입 시 헝가리안 1회 → 타겟에 붙어 안정.
 *  - MemberSpecific(RangedSafe/원거리): 유닛별 커밋. 매 틱 재결정이 아니라 트리거가 떴을
 *    때만 재배치하고 그 사이엔 슬롯을 잠근다 → 이동 중 정지·프레임 회전 트위치 제거.
 *
 * 戦闘隊形コンポーネント。集合型は進入時1回割当、個別型はトリガー時のみ再配置しコミットを固定。
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
	
	// 교전 타겟. 약참조 — 타겟 소멸 시 댕글링 방지 (anchor TOptional의 근거).
	TWeakObjectPtr<AActor> CurrentTarget;

private:
	// [a] 기준 프레임. 타겟 유효하면 그 transform, 아니면 미반환(파이프라인 정지).
	TOptional<FTransform> GetFormationAnchor() const;

	// 최종 반경 산출 (디자이너 기본 + 타겟 크기 보정). Strategy는 이 출처를 모른다.
	float ComputeBaseRadius() const;

	// 역할 태그에 해당하는 설정 검색. 없으면 nullptr (호출부가 폴백 결정).
	const FRoleSlotConfig* FindConfigForRole(const FGameplayTag& Role) const;

	// =========================================================================
	// 재배치 결정 게이트.
	// "지금 자리를 버리고 옮길 이유가 있나"만 판단. 어디로 갈지는 Strategy 몫.
	// ⚠ 플레이어 위치/방향은 여기 트리거에 절대 넣지 않는다 — 넣으면 오빗 트위치 부활.
	//   플레이어는 목적지 편향(Strategy)에만 들어간다.
	// =========================================================================
	// 再配置決定ゲート（個別型専用）。プレイヤー位置はトリガーに入れない（オービット復活）。

	// 재배치 실행: 현재 월드로 슬롯 생성 → 환경보정 → 커밋 갱신 → locomotion에 전달.
	void CommitReposition(APartyCharacter* Member, const FSlotGenContext& Context,
		const FRoleSlotConfig& Config, FCommitSnapshot& OutSnapshot);

	// 개별형 점유 입력 수집: 이미 배치된 그룹들의 슬롯 + 나를 제외한 다른 유닛들의 커밋 슬롯.
	// 매 틱 재생성이 아니라 "남들이 실제로 선 자리(커밋)"를 읽으므로 동시성 충돌이 없다(Bug 1 수정).
	// 「他ユニットの実コミット位置」を読むため同時生成の衝突が起きない（Bug 1修正）。
	TArray<FVector> GatherOccupancyForMemberSpecific(const APartyCharacter* Self, const TArray<FVector>& GroupOccupied) const;

protected:
	// =========================================================================
	// 집합형(Arc) 그룹별 슬롯 배정 저장 (진입 시 헝가리안으로 1회 결정, 전투 중 유지).
	// 個別型はこれを使わず、CommitSnapshotsで管理する。
	// =========================================================================
	TMap<FGameplayTag, FRoleGroupRuntime> RuntimeGroups;

	// 개별형(RangedSafe) 캐릭터별 커밋 상태. weak ptr 키 — 소멸 시 .Get()==null로 거른다.
	// 個別型のキャラ別コミット状態。
	TMap<TWeakObjectPtr<APartyCharacter>, FCommitSnapshot> CommitSnapshots;

	// 재배정 필요 플래그. 전투 진입(Activate) 시 true. 집합형 헝가리안에 적용.
	bool bNeedsReassignment = true;

public:
	// 활성화 시 재배정 예약 + 개별형 커밋 초기화 (진입 시 전원 초기 배치).
	virtual void Activate(bool bReset = false) override;
};