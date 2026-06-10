#include "AI/CombatRoleTags.h"

namespace CombatRoleTags
{
	// 태그 문자열은 여기 한 곳에만 존재. 다른 코드는 전부 심볼로 참조.
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Melee,  "Role.Combat.Melee",  "근거리 — 타겟 중심 호 배치 / 近接：ターゲット中心のアーク配置");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ranged, "Role.Combat.Ranged", "원거리 — 적 전체 기준 안전 위치 / 遠距離：敵全体を考慮した安全位置");
}