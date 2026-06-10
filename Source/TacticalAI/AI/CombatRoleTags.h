#pragma once

#include "NativeGameplayTags.h"

// =========================================================================
// 전투 포지셔닝 역할 태그 (Role.Combat.*).
// BattleComponent가 이 태그로 동료를 그룹 분류한다.
// Native 등록이라 에디터에서 삭제 불가 — 코드가 참조하는 태그는 항상 Native로.
// 주의: 이건 "포지셔닝" 역할 축이다. 타겟팅·스킬용 역할이 생기면 별개 축으로.
// =========================================================================
// 戦闘ポジショニング役割タグ。コードが参照するタグはNative登録（エディタ削除不可）。
// ターゲティング・スキル用の役割とは別軸。
namespace CombatRoleTags
{
	TACTICALAI_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Melee);
	TACTICALAI_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ranged);
}