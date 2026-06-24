#pragma once

#include "CoreMinimal.h"

#include "Enemies/Formations/EnemySubFormationStrategy.h"

#include "EnemySubFormationStrategy_Line.generated.h"

// =========================================================================
// 적을 로컬 Y축 방향으로 한 줄 배치하는 하위 진형 전략.
//
// 진형의 중심은 항상 로컬 원점에 맞춘다.
// 로컬 +X는 전방, 로컬 Y는 좌우 방향으로 해석한다.
//
// 개별 몬스터의 역할이나 실제 슬롯 배정 순서는 해석하지 않는다.
// 슬롯 회전, 월드 좌표 변환, NavMesh 보정도 상위 계층의 책임이다.
//
// 敵をローカルY軸方向へ横一列に配置する
// サブフォーメーション戦略。
//
// 配置全体の中心は常にローカル原点に合わせる。
// ローカル+Xを前方、ローカルYを左右方向として扱う。
//
// 敵の役割や実際のスロット割り当て順は解釈しない。
// スロットの向き・ワールド座標変換・NavMesh補正も
// 上位レイヤーの責務とする。
// =========================================================================
UCLASS(
	EditInlineNew,
	Blueprintable,
	BlueprintType,
	DisplayName = "敵サブフォーメーション - 横一列")
class TACTICALAI_API UEnemySubFormationStrategy_Line
	: public UEnemySubFormationStrategy
{
	GENERATED_BODY()

protected:
	// =========================================================================
	// 한 슬롯과 인접 슬롯 사이의 거리.
	// 값이 클수록 한 줄이 넓게 펼쳐진다.
	//
	// 隣接するスロット同士の間隔。
	// 値が大きいほど横一列の配置が広がる。
	// =========================================================================
	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		Category = "敵フォーメーション|横一列",
		meta = (ClampMin = "1.0", UIMin = "1.0"))
	float SlotSpacing = 150.0f;

	// =========================================================================
	// 입력 인원수만큼 원점 중심의 횡 1열 슬롯을 생성한다.
	//
	// 入力人数と同数の横一列スロットを、
	// ローカル原点を中心として生成する。
	// =========================================================================
	virtual bool BuildLocalSlotsInternal(
		const FEnemySubFormationBuildContext& Context,
		TArray<FTransform>& OutLocalSlotTransforms,
		FString& OutError) const override;
};