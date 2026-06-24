#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"

#include "EnemySubFormationStrategy.generated.h"

// =========================================================================
// 적 하위 진형 생성 입력.
//
// 한 입력 그룹의 인원수만 전달한다.
// 몬스터 종류, 전투 역할, 슬롯 배정 방식은 이 구조체에 포함하지 않는다.
//
// 敵サブフォーメーション生成用の入力。
// 敵種・戦闘役割・スロット割り当て方式には依存せず、
// 対象グループの人数と再現用シードだけを保持する。
// =========================================================================
struct TACTICALAI_API FEnemySubFormationBuildContext
{
	// 이 하위 진형에 배치할 몬스터 수.
	// このサブフォーメーションに配置する敵の数。
	int32 MemberCount = 0;

	// 랜덤 배치를 재현 가능하게 만들기 위한 시드.
	// ランダム配置を再現可能にするためのシード。
	int32 RandomSeed = 0;
};

// =========================================================================
// 재사용 가능한 적 전용 하위 진형 전략.
//
// 한 그룹의 N명을 선, 호, 원, 분산 등의 로컬 배치로 변환한다.
// 출력은 상위 진형 원점이 아니라 이 하위 진형 자체 원점 기준이다.
//
// 1グループのN体を、直線・円弧・円形・分散などの
// ローカル配置へ変換する敵専用サブフォーメーション戦略。
//
// 出力は上位フォーメーション基準ではなく、
// このサブフォーメーション自身の原点を基準とする。
//
// 生成アルゴリズムはC++で実装し、Blueprintは
// 具体戦略のパラメータ調整と差し替えに使用する。
// =========================================================================
UCLASS(Abstract, EditInlineNew, BlueprintType)
class TACTICALAI_API UEnemySubFormationStrategy : public UObject
{
	GENERATED_BODY()

public:
	// =========================================================================
	// 공통 불변식을 검사한 뒤 로컬 슬롯을 생성한다.
	// 성공한 경우 출력 슬롯 수는 반드시 MemberCount와 같아야 한다.
	//
	// 共通の不変条件を検証した後、ローカルスロットを生成する。
	//
	// MemberCount == 0は正常入力として空配列を返す。
	// 成功時のスロット数はMemberCountと一致しなければならない。
	// =========================================================================
	bool TryBuildLocalSlots(
		const FEnemySubFormationBuildContext& Context,
		TArray<FTransform>& OutLocalSlotTransforms,
		FString* OutError = nullptr) const;

protected:
	// =========================================================================
	// 구체 하위 진형이 로컬 슬롯을 생성하는 C++ 확장 지점.
	//
	// 호출 시 MemberCount는 항상 1 이상이다.
	// 배정, NavMesh 보정, 월드 좌표 변환은 여기서 수행하지 않는다.
	//
	// 具体的なサブフォーメーションがローカルスロットを生成する C++専用の拡張点。
	// 割り当て・NavMesh補正・ワールド座標変換は行わない。
	// =========================================================================
	virtual bool BuildLocalSlotsInternal(
		const FEnemySubFormationBuildContext& Context,
		TArray<FTransform>& OutLocalSlotTransforms,
		FString& OutError) const
		PURE_VIRTUAL(
			UEnemySubFormationStrategy::BuildLocalSlotsInternal,
			return false;
		);
};