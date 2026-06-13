#pragma once

#include "CoreMinimal.h"
#include "AI/Strategies/SlotGeneratorStrategy.h"
#include "SlotGeneratorStrategy_RangedSafe.generated.h"

/**
 * 원거리 안전 위치 배치. 적 전체 분포·타겟·사거리를 근거로 "충분히 안전하고,
 * 전선을 유지하며, 타겟을 계속 칠 수 있고, 덜 움직이는" 위치를 고른다.
 * Arc와 달리 anchor가 아니라 PrimaryTarget을 사거리 평가의 중심으로 쓴다.
 *
 * 개별형(member-specific): 멤버 1명을 기준으로 후보를 평가하므로,
 * 슬롯이 곧 그 멤버의 것 → 그룹 헝가리안 불필요(컴포넌트가 skip).
 *
 * 遠距離の安全位置配置。敵全体の分布・ターゲット・射程から
 * 「十分安全で前線を保ち、ターゲットを撃ち続けられ、あまり動かない」位置を選ぶ。
 * Arcと違いanchorではなくPrimaryTargetを射程評価の中心とする。
 *
 * TODO: 현재 빈 깡통 — RequesterLocation을 그대로 반환(제자리). 파이프라인 검증용.
 *       후보 생성·점수화는 다음 단계에서.
 */
UCLASS(BlueprintType, DisplayName = "Slot Gen - Ranged Safe (원거리 안전)")
class TACTICALAI_API USlotGeneratorStrategy_RangedSafe : public USlotGeneratorStrategy
{
	GENERATED_BODY()

public:
	virtual FVector GenerateSlot(const FSlotGenContext& Context) const override;
	
	// メンバー基準で生成するため個別割当。
	virtual ESlotAssignmentPolicy GetAssignmentPolicy() const override
	{
		return ESlotAssignmentPolicy::MemberSpecific;
	}
};