#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "StateTreeEvaluatorBase.h"
#include "StateTreeEngageRangeEvaluator.generated.h"

class AActor;

// =======================================================
// 교전 거리 Evaluator의 실행별 데이터
// Output은 같은 트리의 조건·Task가 바인딩해서 읽는다.
//
// 交戦距離Evaluatorの実行ごとのデータ。
// Outputは同じツリーの条件・Taskがバインドして読む。
// =======================================================
USTRUCT()
struct TACTICALAI_API FEngageRangeEvaluatorInstanceData
{
	GENERATED_BODY()

	// 질의 대상 캐릭터. StateTree Context의 Actor와 연결한다.
	// 照会対象のキャラクター。StateTree ContextのActorと接続する。
	UPROPERTY(EditAnywhere, Category = Context)
	TObjectPtr<AActor> Actor = nullptr;

	// 교전 거리를 조회할 Ability의 Asset Tag.
	// 交戦距離を照会するAbilityのAsset Tag。
	UPROPERTY(EditAnywhere, Category = Parameter, meta = (Categories = "Ability"))
	FGameplayTag AbilityTag;

	// Ability가 교전 거리를 산출하지 못할 때 사용할 거리(cm). 캡슐 표면 사이 수평 거리.
	// Abilityが交戦距離を算出できない場合に使う距離(cm)。カプセル表面間の水平距離。
	UPROPERTY(EditAnywhere, Category = Parameter, meta = (ClampMin = "0.0", Units = "cm"))
	float FallbackDistance = 150.f;

	// 이 캐릭터의 교전 거리(cm). 공격 개시·접근 판정이 바인딩한다.
	// このキャラクターの交戦距離(cm)。攻撃開始・接近の判定がバインドする。
	UPROPERTY(EditAnywhere, Category = Output)
	float EngageDistance = 0.f;
};

// =======================================================
// 교전 거리 Evaluator
// 트리 시작 시 지정한 Ability의 교전 거리를 ASC에 한 번 질의하여 실행 인스턴스마다 공급한다.
//
// 交戦距離Evaluator。
// ツリー開始時に指定Abilityの交戦距離をASCへ一度照会し、実行インスタンスごとに供給する。
// =======================================================
USTRUCT(meta = (DisplayName = "Engage Range", Category = "Tactical AI|Combat"))
struct TACTICALAI_API FStateTreeEngageRangeEvaluator : public FStateTreeEvaluatorCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FEngageRangeEvaluatorInstanceData;

	virtual const UStruct* GetInstanceDataType() const override
	{
		return FInstanceDataType::StaticStruct();
	}

	virtual void TreeStart(FStateTreeExecutionContext& Context) const override;
};