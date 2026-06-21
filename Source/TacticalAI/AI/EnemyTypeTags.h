#pragma once

#include "NativeGameplayTags.h"

// =========================================================================
// 적 종류(아키타입) 태그 (Enemy.Type.*).
// 동료의 Role.Combat(포지셔닝 역할)과는 완전히 다른 축 — 이건 "무엇인가"(종류)다.
// 배치는 이 태그를 보지 않는다(교전거리 AttackRange로 정렬). 합성 규칙(A+B→C)만
// 이 태그를 읽어 "이 종류끼리 만나면 C전략" 같은 조건을 판정한다.
// 지금은 Melee/Ranged 둘로 거칠게 나누되 Enemy.Type 아래라, 추후 Goblin/Slime 등으로
// 얼마든지 세분 가능(확장축 유지). 코드가 심볼로 참조하므로 Native 등록(에디터 삭제 불가).
// =========================================================================
// 敵の種類タグ。仲間のRole.Combat（役割）とは別軸＝「何であるか」（種類）。
// 配置はこのタグを見ない（射程AttackRangeで整列）。合成規則のみが読む。
// 今はMelee/Rangedの粗い二分だがEnemy.Type配下なので、後でGoblin/Slime等に細分可能。
namespace EnemyTypeTags
{
	TACTICALAI_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Melee);
	TACTICALAI_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ranged);
}