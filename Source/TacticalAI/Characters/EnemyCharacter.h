#pragma once

#include "CoreMinimal.h"

#include "Characters/TacticalCharacterBase.h"
#include "EnemyCharacter.generated.h"

class UEnemyFormationStrategy;
class AEnemyGroup;

// =========================================================================
// 적 캐릭터 베이스.
// 敵キャラクター基底。
// =========================================================================
UCLASS()
class TACTICALAI_API AEnemyCharacter : public ATacticalCharacterBase
{
	GENERATED_BODY()

public:
	AEnemyCharacter();

	// =========================================================================
	// 소속 그룹 back-ptr.
	// 소유가 아니므로 TWeakObjectPtr — 그룹이 먼저 사라져도 자동 무효화.
	// 설정은 AEnemyGroup::RegisterMember 경유만 (캐릭터가 스스로 정하지 않음).
	//
	// 所属グループへのback-ptr。所有ではないためTWeakObjectPtr。
	// 設定はRegisterMember経由のみ（キャラ側では決めない）。
	// =========================================================================
	void SetEnemyGroup(AEnemyGroup* InGroup);
	AEnemyGroup* GetEnemyGroup() const;
	
	// =========================================================================
	// 개별 공격 행동에서 참고할 선호 교전 사거리.
	//
	// Formation Layout 계층은 이 값을 직접 읽지 않는다.
	// Target Policy, Attack State, Return 판단 같은
	// 전투 실행 계층에서 참고할 수 있는 적 종류의 성향 값이다.
	//
	// 個別攻撃行動で参照する選好交戦距離。
	//
	// Formation Layout層はこの値を直接読まない。
	// Target Policy・Attack State・Return判断など、
	// 戦闘実行レイヤーが参照できる敵種の傾向値。
	// =========================================================================
	float GetAttackRange() const
	{
		return AttackRange;
	}

	// =========================================================================
	// 도주 발동 체력 비율.
	//
	// Formation Override는 이 값을 덮어쓰지 않는다.
	// 실제 도주 실행 여부는 행동 계층에서 판단한다.
	//
	// 退却開始の体力割合。
	//
	// Formation Overrideはこの値を上書きしない。
	// 実際に退却するかどうかは行動レイヤーで判断する。
	// =========================================================================
	float GetFleeHealthRatio() const
	{
		return FleeHealthRatio;
	}

private:
	
	TWeakObjectPtr<AEnemyGroup> OwningGroup;
	
	// =========================================================================
	// 적 종류의 전투 성향.
	//
	// 적 종류가 가지는 개별 전투 판단용 기본값이다.
	// Formation Override는 공간 배치 정책만 덮어쓰며,
	// 이 전투 성향 값은 덮어쓰지 않는다.
	//
	// 敵種の戦闘傾向。
	//
	// 敵種が持つ個別戦闘判断用の基本値。
	// Formation Overrideは空間配置方針だけを上書きし、
	// この戦闘傾向値は上書きしない。
	// =========================================================================

	// 선호 교전 사거리.
	// 개별 공격/이탈 판단에서 참고하는 적 종류의 전투 성향 값이다.
	//
	// 選好交戦距離。
	// 個別攻撃・離脱判断で参照する敵種の戦闘傾向値。
	UPROPERTY(
		EditDefaultsOnly,
		BlueprintReadOnly,
		Category = "敵AI|戦闘傾向",
		meta = (
			AllowPrivateAccess = "true",
			ClampMin = "0.0",
			DisplayName = "選好交戦距離"))
	float AttackRange = 200.0f;

	// 도주 발동 체력 비율.
	// 0이면 끝까지 싸우는 종류로 해석한다.
	//
	// 退却を開始する体力割合。
	// 0の場合は最後まで戦う敵種として扱う。
	UPROPERTY(
		EditDefaultsOnly,
		BlueprintReadOnly,
		Category = "敵AI|戦闘傾向",
		meta = (
			AllowPrivateAccess = "true",
			ClampMin = "0.0",
			ClampMax = "1.0",
			DisplayName = "退却開始体力割合"))
	float FleeHealthRatio = 0.0f;
};