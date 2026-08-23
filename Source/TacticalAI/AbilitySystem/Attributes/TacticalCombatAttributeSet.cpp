#include "AbilitySystem/Attributes/TacticalCombatAttributeSet.h"
#include "GameplayEffectExtension.h"

// =======================================================
// 캐릭터별 최종 초기값은 이후 초기화 Gameplay Effect가 담당한다.
// 여기의 값은 초기화 Effect 적용 전에도 유효한 상태를 보장하는 기본값이다.
//
// キャラクター別の最終初期値は初期化Gameplay Effectが担当する。
// ここではEffect適用前にも有効な状態を保証する。
// =======================================================
UTacticalCombatAttributeSet::UTacticalCombatAttributeSet()
{
	InitMaxHealth(100.f);
	InitHealth(GetMaxHealth());
	InitBasicAttackPower(1.f);
	InitIncomingDamage(0.f);
}

// =======================================================
// Gameplay Effect 적용 결과 보정
//
// AttributeSet은 값의 유효 범위만 보장한다.
// 피해량 계산과 사망 처리는 각각 Gameplay Effect와 별도 생명주기 계층이 담당한다.
//
// AttributeSetは値の有効範囲のみ保証する。
// ダメージ計算と死亡処理はGameplay Effectと別のライフサイクル層が担当する。
// =======================================================
void UTacticalCombatAttributeSet::PostGameplayEffectExecute(
	const FGameplayEffectModCallbackData& Data)
{
	Super::PostGameplayEffectExecute(Data);

	// [1] 전달받은 피해량을 현재 체력에 반영하고 Meta Attribute를 초기화한다.
	// [1] 受け取ったダメージ量を現在体力へ反映し、Meta Attributeを初期化する。
	if (Data.EvaluatedData.Attribute == GetIncomingDamageAttribute())
	{
		const float Damage = FMath::Max(GetIncomingDamage(), 0.f);

		SetIncomingDamage(0.f);

		if (Damage > 0.f)
		{
			SetHealth(FMath::Clamp(
				GetHealth() - Damage,
				0.f,
				GetMaxHealth()));
		}

		return;
	}
	
	// [2] 체력 변경 결과를 0~MaxHealth 범위로 제한한다.
	// [2] 体力の変更結果を0からMaxHealthの範囲に制限する。
	if (Data.EvaluatedData.Attribute == GetHealthAttribute())
	{
		SetHealth(FMath::Clamp(
			GetHealth(),
			0.f,
			GetMaxHealth()));

		return;
	}

	// [3] 최대 체력은 1 이상을 보장하고, 현재 체력도 새 최대값에 맞춘다.
	// [3] 最大体力を1以上に保ち、現在体力も新しい最大値に合わせる。
	if (Data.EvaluatedData.Attribute == GetMaxHealthAttribute())
	{
		SetMaxHealth(FMath::Max(GetMaxHealth(), 1.f));
		SetHealth(FMath::Clamp(
			GetHealth(),
			0.f,
			GetMaxHealth()));
	}
}