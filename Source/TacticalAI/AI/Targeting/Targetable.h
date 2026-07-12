#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "Targetable.generated.h"

UINTERFACE(MinimalAPI, Blueprintable)
class UTargetable : public UInterface
{
	GENERATED_BODY()
};

// =========================================================================
// Targetable Interface
//
// "전투 타겟이 될 수 있는 것"의 계약. 진영 무관 — 아군/적/기믹이 모두 구현.
// 자격(지금 잡을 수 있나)과 공간 정보(포위 반경)만 답한다.
// "누가 적인가"는 이 계약의 책임이 아니다 — 후보 공급은 인지 레이어가 결정.
//
// 「戦闘ターゲットになり得るもの」の契約。陣営非依存 — 味方/敵/ギミックが実装。
// 資格(今狙えるか)と空間情報(包囲半径)のみ。「誰が敵か」は知覚レイヤーの責務。
// =========================================================================
class TACTICALAI_API ITargetable
{
	GENERATED_BODY()

public:
	// 지금 타겟으로 잡을 수 있는가. 사망·무적 페이즈·기믹 비활성 등은
	// 타겟 자신이 답하고, 셀렉터는 이유를 모른 채 결과만 필터한다.
	// 今ターゲット可能か。死亡・無敵・ギミック非活性などは対象自身が答える。
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Combat Target")
	bool IsTargetable() const;

	// 이 타겟을 둘러쌀 때의 베이스 반경. 충돌과 별개의 연출 값 (구 TargetDummy 값).
	// この対象を囲む際のベース半径。コリジョンとは別の演出値。
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Combat Target")
	float GetEncircleRadius() const;
};