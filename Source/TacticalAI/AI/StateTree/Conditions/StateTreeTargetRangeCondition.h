#pragma once

#include "CoreMinimal.h"
#include "StateTreeConditionBase.h"
#include "StateTreeTargetRangeCondition.generated.h"

class AActor;

// =======================================================
// 타겟 거리 판정 방식
// 타겟이 없으면 두 방식 모두 false.
//
// ターゲット距離の判定方式。
// ターゲットがなければ両方式ともfalse。
// =======================================================
UENUM()
enum class ETargetRangeCheck : uint8
{
	// 캡슐 표면 사이의 수평 거리가 Distance 이하.
	// カプセル表面間の水平距離がDistance以下。
	Within,

	// 캡슐 표면 사이의 수평 거리가 Distance 초과.
	// カプセル表面間の水平距離がDistance超過。
	Beyond
};

// =======================================================
// 타겟 거리 조건의 실행별 데이터
//
// ターゲット距離条件の実行ごとのデータ。
// =======================================================
USTRUCT()
struct TACTICALAI_API FTargetRangeConditionInstanceData
{
	GENERATED_BODY()

	// 판정할 캐릭터. StateTree Context의 Actor와 연결한다.
	// 判定するキャラクター。StateTree ContextのActorと接続する。
	UPROPERTY(EditAnywhere, Category = Context)
	TObjectPtr<AActor> Actor = nullptr;

	// 판정 기준 거리(cm). 캐릭터와 타겟의 캡슐 표면 사이 수평 거리.
	// 평타 시작 판정에 쓸 때는 첫 타의 루트모션 돌진 거리를 고려해 정한다.
	// 判定基準距離(cm)。キャラクターとターゲットのカプセル表面間の水平距離。
	// 通常攻撃の開始判定に使う場合は、初段のルートモーション踏み込み距離を考慮して決める。
	UPROPERTY(EditAnywhere, Category = Parameter, meta = (ClampMin = "0.0", Units = "cm"))
	float Distance = 150.f;
};

// =======================================================
// 타겟 거리 조건
// 캐릭터와 현재 전투 타겟 사이의 캡슐 표면 수평 거리를 판정한다.
// 타겟은 TargetSelector의 확정 결과를 추상 베이스 타입으로만 조회한다.
// Context가 유효하지 않거나 타겟이 없으면 모든 방식에서 false.
//
// ターゲット距離条件。
// キャラクターと現在の戦闘ターゲット間のカプセル表面水平距離を判定する。
// ターゲットはTargetSelectorの確定結果を抽象基底型のみで照会する。
// Contextが無効、またはターゲットがない場合はすべての方式でfalse。
// =======================================================
USTRUCT(meta = (DisplayName = "Target Range", Category = "Tactical AI|Combat"))
struct TACTICALAI_API FStateTreeTargetRangeCondition : public FStateTreeConditionCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FTargetRangeConditionInstanceData;

	virtual const UStruct* GetInstanceDataType() const override
	{
		return FInstanceDataType::StaticStruct();
	}

	virtual bool TestCondition(FStateTreeExecutionContext& Context) const override;

	// 판정 방식. 상태마다 에셋에서 선택한다.
	// 判定方式。状態ごとにアセットで選択する。
	UPROPERTY(EditAnywhere, Category = Condition)
	ETargetRangeCheck Check = ETargetRangeCheck::Within;
};