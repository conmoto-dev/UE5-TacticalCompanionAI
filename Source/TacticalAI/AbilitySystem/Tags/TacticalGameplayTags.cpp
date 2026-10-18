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
		Event_Ability_Activate_InputBound,
		"Event.Ability.Activate.InputBound",
		"入力の押下・解放を使用する実行方式");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		Event_Ability_Activate_AutoComplete,
		"Event.Ability.Activate.AutoComplete",
		"Ability自身の完了条件で終了する実行方式");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		Event_Attack_Basic_Hit,
		"Event.Attack.Basic.Hit",
		"通常攻撃の実ヒットフレーム");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		Event_Attack_Basic_SectionEnd,
		"Event.Attack.Basic.SectionEnd",
		"通常攻撃1段分の終了");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		Data_Damage,
		"Data.Damage",
		"ダメージ量 (SetByCaller)");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		State_Attacking,
		"State.Attacking",
		"攻撃動作の実行中");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		State_Dead,
		"State.Dead",
		"死亡");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		Cooldown_Attack_Basic,
		"Cooldown.Attack.Basic",
		"通常攻撃のクールダウン");
}