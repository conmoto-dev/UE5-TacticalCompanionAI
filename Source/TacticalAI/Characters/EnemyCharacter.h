#pragma once

#include "CoreMinimal.h"
#include "Characters/TacticalCharacterBase.h"
#include "EnemyCharacter.generated.h"

class UEnemyFormationProfile;

// =========================================================================
// 적 캐릭터 베이스.
//
// 적의 "종류"는 이 BP 클래스 단위로 취급한다.
// 외형, 개별 전투 성향, 기본 Formation Profile은
// 같은 적 종류의 기본값으로 보관한다.
//
// 기본 Formation은 Encounter/Spawn Override가 없을 때만 사용하는
// fallback 공간 배치 정책이다.
// Encounter/Spawn 쪽에서 Formation Override가 지정되면,
// 이 기본 Formation은 실행하지 않는다.
//
// Formation 계산, Target 조정, Slot 배정, 이동 명령은
// 이 Actor가 직접 수행하지 않는다.
// 이 Actor는 적 종류의 기본 정책 데이터를 제공하고,
// 런타임 전술 결정은 이후 Enemy Tactical Coordinator가 담당한다.
//
// 敵キャラクター基底。
//
// 敵の「種類」はこのBPクラス単位で扱う。
// 外見・個別の戦闘傾向・デフォルトFormation Profileは、
// 同じ敵種の基本値として保持する。
//
// デフォルトFormationは、Encounter/Spawn Overrideが無い場合だけ使う
// fallback用の空間配置方針。
// Encounter/Spawn側でFormation Overrideが指定された場合、
// このデフォルトFormationは実行されない。
//
// Formation計算・Target調整・Slot割り当て・移動命令は
// このActorでは直接行わない。
// このActorは敵種の基本方針データを提供し、
// ランタイム戦術決定はEnemy Tactical Coordinatorが担当する。
// =========================================================================
UCLASS()
class TACTICALAI_API AEnemyCharacter : public ATacticalCharacterBase
{
	GENERATED_BODY()

public:
	AEnemyCharacter();

	// =========================================================================
	// Override가 없을 때 사용할 적 종류 기본 Formation Profile.
	//
	// 이 값은 몬스터 종류의 기본 공간 배치 정책일 뿐이며,
	// Encounter/Spawn Override가 지정되면 사용하지 않는다.
	//
	// Overrideが無い場合に使用する敵種デフォルトFormation Profile。
	//
	// これは敵種の基本空間配置方針であり、
	// Encounter/Spawn Overrideが指定された場合は使用しない。
	// =========================================================================
	UEnemyFormationProfile* GetDefaultFormationProfile() const
	{
		return DefaultFormationProfile.Get();
	}

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
	// =========================================================================
	// 적 종류 기본 Formation.
	//
	// 같은 BP 클래스의 적들이 Override 없이 그룹화될 때 사용할 기본 Profile.
	// 레벨의 특정 Spawn/Encounter에서만 다른 배치를 쓰고 싶다면,
	// 이 값을 바꾸지 말고 Spawn/Encounter의 Formation Source를 사용한다.
	//
	// 敵種デフォルトFormation。
	//
	// 同じBPクラスの敵がOverride無しでグループ化される場合に使う
	// 基本Profile。
	// レベル上の特定Spawn/Encounterだけ別配置にしたい場合は、
	// この値ではなくSpawn/Encounter側のFormation Sourceを使用する。
	// =========================================================================
	UPROPERTY(
		EditDefaultsOnly,
		BlueprintReadOnly,
		Category = "敵フォーメーション",
		meta = (
			AllowPrivateAccess = "true",
			DisplayName = "デフォルトFormation Profile"))
	TObjectPtr<UEnemyFormationProfile> DefaultFormationProfile = nullptr;

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