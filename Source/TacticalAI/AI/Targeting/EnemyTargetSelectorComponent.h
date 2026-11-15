#pragma once

#include "CoreMinimal.h"
#include "AI/Targeting/TargetSelectorComponent.h"
#include "EnemyTargetSelectorComponent.generated.h"

// ==================================================
// Enemy Target Selector
//
// 소속 인카운터에서 타겟 후보와 다른 멤버의 현재 타겟을 조회한다.
// 후보 평가와 타겟 선택은 부모 클래스의 공통 처리를 사용한다.
//
// 所属エンカウンターからターゲット候補と他メンバーの現在のターゲットを取得する。
// 候補評価とターゲット選択には基底クラスの共通処理を使用する。
// ==================================================
UCLASS(ClassGroup=(TacticalAI), meta=(BlueprintSpawnableComponent))
class TACTICALAI_API UEnemyTargetSelectorComponent : public UTargetSelectorComponent
{
	GENERATED_BODY()
	
	UEnemyTargetSelectorComponent();

protected:
	virtual FTargetingContext BuildContext() const override;
	virtual TArray<AActor*> GatherCandidates() const override;
};