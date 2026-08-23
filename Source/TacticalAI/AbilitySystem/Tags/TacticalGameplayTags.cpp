#include "AbilitySystem/Tags/TacticalGameplayTags.h"

// =======================================================
// Tactical AI Gameplay Tags
//
// 선언된 Native Gameplay Tag 식별자와 실제 계층형 태그 문자열을 연결한다.
// 宣言済みのNative Gameplay Tag識別子と実際の階層タグ文字列を関連付ける。
// =======================================================
namespace TacticalGameplayTags
{
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		Ability_Attack_Basic,
		"Ability.Attack.Basic",
		"Gameplay Ability 通常攻撃");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		Data_Damage,
		"Data.Damage",
		"ダメージ量 (SetByCaller)");
	
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Attacking, 
		"State.Attacking",
		"공격 동작 수행 중 / 攻撃動作の実行中");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Dead, 
		"State.Dead",
		"사망 / 死亡");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Cooldown_Attack_Basic, 
		"Cooldown.Attack.Basic",
		"통상 공격 쿨다운 / 通常攻撃のクールダウン");
}