#pragma once

#include "CoreMinimal.h"
#include "AI/Targeting/TargetSelectorComponent.h"
#include "EnemyTargetSelectorComponent.generated.h"

// =========================================================================
// 적 측 타겟 셀렉터. 
// 리더 필드는 채우지 않음 → 리더 의존 정책은 0점으로 퇴화 (진영 분기 없음).
// 재료는 소속 인카운터 경유로 Pull — 소속 갈래(스폰 그룹·레벨 배치)는 모른다.
//
// 敵側ターゲットセレクタ。供給フック2つのみ実装 — 材料は所属エンカウンター経由でPull。
// リーダー欄は空のまま → リーダー依存ポリシーは0点に退化（陣営分岐なし）。
// =========================================================================
UCLASS(ClassGroup=(TacticalAI), meta=(BlueprintSpawnableComponent))
class TACTICALAI_API UEnemyTargetSelectorComponent : public UTargetSelectorComponent
{
	GENERATED_BODY()
	
	UEnemyTargetSelectorComponent();

protected:
	virtual FTargetingContext BuildContext() const override;
	virtual TArray<AActor*> GatherCandidates() const override;
};