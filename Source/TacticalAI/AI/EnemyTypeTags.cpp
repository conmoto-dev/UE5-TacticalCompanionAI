#include "AI/EnemyTypeTags.h"

namespace EnemyTypeTags
{
	// 태그 문자열은 여기 한 곳에만 존재. 다른 코드는 전부 심볼로 참조.
	// タグ文字列はここ一箇所のみ。他コードは全てシンボル参照。
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Melee,  "Enemy.Type.Melee",  "근접형 몹 — 합성 규칙 식별용 / 近接型：合成規則の識別用");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ranged, "Enemy.Type.Ranged", "원거리형 몹 — 합성 규칙 식별용 / 遠距離型：合成規則の識別用");
}