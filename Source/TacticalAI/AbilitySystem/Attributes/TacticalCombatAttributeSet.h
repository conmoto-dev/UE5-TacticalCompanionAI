// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "AbilitySystemComponent.h"
#include "TacticalCombatAttributeSet.generated.h"

/**
 * Attribute 접근에 필요한 표준 GAS 함수들을 생성한다.
 * 어트리뷰트 1개당 표준 접근자 4종(Get/Set/Init + Attribute 핸들) 선언을 묶는 GAS 공식 매크로.
 * Attributeへのアクセスに必要な標準GAS関数を生成する。
 */
#define ATTRIBUTE_ACCESSORS(ClassName, PropertyName) \
	GAMEPLAYATTRIBUTE_PROPERTY_GETTER(ClassName, PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_GETTER(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_SETTER(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_INITTER(PropertyName)

// =========================================================================
// 전 캐릭터 공통 전투 스탯. 클래스는 하나, 값은 ASC 인스턴스별(캐릭터별).
// 어트리뷰트 추가 기준: 전투 중 GE로 변하는 것만.
// (AttackRange 등 전투 중 실시간을 변하지 않는 값은 캐릭터 소유)
// =========================================================================
// 全キャラ共通の戦闘ステータス。クラスは一つ、値はASCインスタンス毎。
// キャラクターの戦闘に必要な最小限のAttributeを管理する。
UCLASS()
class TACTICALAI_API UTacticalCombatAttributeSet : public UAttributeSet
{
	GENERATED_BODY()
	
public:
	UTacticalCombatAttributeSet();
	
	/**
	 * Gameplay Effect가 Attribute를 변경한 직후 호출된다.
	 * Gameplay EffectによるAttribute変更直後に呼び出される。
	 */
	virtual void PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data) override;
	
	
	// ───── Attribute ─────
	
	// 現在体力。死亡「判定」は消費側が変更通知で行う。
	UPROPERTY(BlueprintReadOnly, Category = "Attributes")
	FGameplayAttributeData Health;
	ATTRIBUTE_ACCESSORS(UTacticalCombatAttributeSet, Health)
	
	// 最大体力。
	UPROPERTY(BlueprintReadOnly, Category = "Attributes")
	FGameplayAttributeData MaxHealth;
	ATTRIBUTE_ACCESSORS(UTacticalCombatAttributeSet, MaxHealth)

	// 평타 피해 계산에 사용하는 공격력. 평타 Ability가 타수별 배율을 적용한다.
	// 通常攻撃のダメージ計算に使用する攻撃力。通常攻撃Abilityが段毎の倍率を適用する。
	UPROPERTY(BlueprintReadOnly, Category = "Attributes")
	FGameplayAttributeData BasicAttackPower;
	ATTRIBUTE_ACCESSORS(UTacticalCombatAttributeSet, BasicAttackPower)
	
	// ───── Meta Attribute ─────

	// 피해 Gameplay Effect가 전달한 최종 피해량.
	// PostGameplayEffectExecute에서 Health에 반영한 뒤 즉시 0으로 초기화한다.
	// ダメージGameplay Effectから渡された最終ダメージ量。
	UPROPERTY(BlueprintReadOnly, Category = "Meta Attributes")
	FGameplayAttributeData IncomingDamage;
	ATTRIBUTE_ACCESSORS(UTacticalCombatAttributeSet, IncomingDamage)
};

#undef ATTRIBUTE_ACCESSORS