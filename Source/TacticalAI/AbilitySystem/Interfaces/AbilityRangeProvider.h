#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "AbilityRangeProvider.generated.h"

struct FGameplayAbilityActorInfo;

UINTERFACE(MinimalAPI, meta = (CannotImplementInterfaceInBlueprint))
class UAbilityRangeProvider : public UInterface
{
	GENERATED_BODY()
};

// =======================================================
// Ability 교전 거리 계약
//
// AI가 이 Ability를 시작해도 되는 대상과의 거리를 Ability 자신이 답한다.
// 계산 방식(루트모션 이동량·투사체 비거리·고정값 등)은 구현하는 Ability가 결정하고,
// AI는 Ability의 구체 클래스를 모른 채 ASC를 거쳐 이 값만 사용한다.
// 사거리 개념이 없는 Ability는 이 계약을 구현하지 않는다.
//
// 상태 규칙: CDO에서 호출될 수 있으므로 상태를 변경하지 않는다(캐시 금지).
//
// Ability交戦距離の契約。
// AIがこのAbilityを開始してよい対象との距離を、Ability自身が答える。
// 計算方式（ルートモーション移動量・投射物の飛距離・固定値など）は実装するAbilityが決定し、
// AIはAbilityの具体クラスを知らずに、ASC経由でこの値のみ使用する。
// 射程の概念がないAbilityはこの契約を実装しない。
//
// 状態規則：CDOから呼ばれ得るため状態を変更しない（キャッシュ禁止）。
// =======================================================
class TACTICALAI_API IAbilityRangeProvider
{
	GENERATED_BODY()

public:
	/**
	 * @param ActorInfo    질의한 ASC의 ActorInfo. 메시 스케일 등 시전자 기준 값의 조회에 사용한다.
	 *                     問い合わせ元ASCのActorInfo。メッシュスケールなど術者基準の値の取得に使う。
	 * @param OutDistance  유효 시 교전 거리(cm). 캡슐 표면 사이 수평 거리.
	 *                     有効時の交戦距離(cm)。カプセル表面間の水平距離。
	 * @return             현재 설정으로 교전 거리를 산출할 수 있는가.
	 *                     現在の設定で交戦距離を算出できるか。
	 */
	virtual bool TryGetEngageDistance(
		const FGameplayAbilityActorInfo& ActorInfo,
		float& OutDistance) const = 0;
};