#pragma once

#include "CoreMinimal.h"
#include "AI/Targeting/TargetSelectorComponent.h"
#include "EnemyTargetSelectorComponent.generated.h"

// =========================================================================
// 적 측 타겟 셀렉터. 
// 리더 필드는 채우지 않음 → 리더 의존 정책은 0점으로 퇴화 (진영 분기 없음).
//
// 敵側ターゲットセレクタ。供給フック2つのみ実装 — 材料は所属EnemyGroup経由でPull。
// リーダー欄は空のまま → リーダー依存ポリシーは0点に退化（陣営分岐なし）。
// =========================================================================
UCLASS(ClassGroup=(TacticalAI), meta=(BlueprintSpawnableComponent))
class TACTICALAI_API UEnemyTargetSelectorComponent : public UTargetSelectorComponent
{
	GENERATED_BODY()

protected:
	virtual FTargetingContext BuildContext() const override;
	virtual TArray<AActor*> GatherCandidates() const override;
};