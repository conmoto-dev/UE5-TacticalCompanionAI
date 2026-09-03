#pragma once

#include "NativeGameplayTags.h"

// =======================================================
// Tactical AI Gameplay Tags
//
// C++ 전투 코드와 Gameplay Ability·Gameplay Effect 에셋이
// 공통으로 사용하는 고정 식별자를 선언한다.
//
// 계층 규칙:
//   Ability.*   행위의 분류 (발동 차단·상호 배제의 대조 축)
//   Event.*     AnimNotify·Gameplay Event로 Ability에 전달하는 전투 타이밍
//   State.*     ASC가 보유하는 런타임 상태
//   Cooldown.*  쿨다운 (쿨다운 GE가 부여, ASC가 발동 검사에서 대조)
//   
// C++戦闘コードとGameplay Ability・Gameplay Effectアセットが
// 共通で使用する固定識別子を宣言する。
// =======================================================
namespace TacticalGameplayTags
{
	// ── Ability ──
	
	// 평타 Gameplay Ability를 식별하는 태그.
	// 플레이어 입력과 AI 모두 이 태그로 평타 활성화를 요청한다.
	// 通常攻撃Gameplay Abilityを識別するタグ。
	// プレイヤー入力とAIの両方がこのタグで発動を要求する。
	TACTICALAI_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Attack_Basic);

	// ── Event ──

	// 평타 몽타주의 실제 타격 프레임. 이 이벤트를 받은 Ability가 데미지 GE를 적용한다.
	// 通常攻撃モンタージュの実ヒットフレーム。このイベントを受けたAbilityがダメージGEを適用する。
	TACTICALAI_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_Attack_Basic_Hit);

	// 평타 1타 분량의 종료 지점. 입력 유지 여부에 따라 다음 타수 재생 또는 종료를 결정한다.
	// 通常攻撃1段分の終了地点。入力維持状態に応じて次段再生または終了を決定する。
	TACTICALAI_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_Attack_Basic_SectionEnd);
	
	// ── SetByCaller ──
	
	// Ability에서 피해 Gameplay Effect로 전달하는 피해량 키 — 평타 GA가 콤보 배율 적용 후 채운다.
	// 캐릭터 상태가 아니라 SetByCaller 수치의 식별자로 사용한다
	// AbilityからダメージGameplay Effectへ渡すダメージ量のキー。通常攻撃GAがコンボ倍率適用後に埋める。
	// キャラクター状態ではなくSetByCaller値の識別子として使用する。
	TACTICALAI_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Data_Damage);
	
	// ── State ──

	// 공격 동작 수행 중. 평타 GA가 실행 동안 부여 — 이동 계열(미세 이동 등)의 정지 조건이자
	// 공격 중 재발동 차단의 대조 축.
	// 攻撃動作の実行中。移動系の停止条件かつ攻撃中の再発動遮断の対照軸。
	TACTICALAI_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Attacking);

	// 사망. 모든 어빌리티 발동 차단 + ITargetable 자격 상실의 근거.
	// 死亡。全アビリティ発動遮断＋ITargetable資格喪失の根拠。
	TACTICALAI_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Dead);

	// ── Cooldown ──

	// 평타 쿨다운. 쿨다운 GE가 지속시간 동안 부여.
	// 通常攻撃のクールダウン。クールダウンGEが持続時間の間付与。
	TACTICALAI_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Cooldown_Attack_Basic);
	
}