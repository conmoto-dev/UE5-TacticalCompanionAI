#pragma once

#include "CoreMinimal.h"
#include "AI/Strategies/SlotGeneratorStrategy.h"
#include "SlotGeneratorStrategy_Encircle.generated.h"

/**
 * 포위 접근 배치. 자기 위치에서 타겟으로 접근하는 방향의 링 위 지점에 서되,
 * 이미 그 링을 점유한 동료와 최소 각도 간격을 지키도록 빈 각도로 자리 잡는다.
 *
 * 개별형(member-specific): 멤버 1명 기준 평가 → 슬롯이 곧 그 멤버 것.
 *  - 슬롯이 인원 수 N에 종속되지 않음 → 한 명의 타겟 변동이 남은 멤버 재배치로 전파 안 됨.
 *  - 타겟 facing 무관 → 적이 회전해도 슬롯 불변.
 *  - 포위는 강제가 아니라 결과 — 각자의 접근 방향 + 점유 회피에서 창발한다.
 * 추격은 여기서 안 한다 — 베이스 ShouldReposition(사거리 이탈)이 재커밋 트리거.
 *
 * 包囲アプローチ配置。自位置からターゲットへ向かう方向のリング上に立ち、
 * 既にリングを占有する味方とは最小角度間隔を保って空き角度へずれて入る。
 * スロットは人数Nに依存しない＝ターゲット変動が他メンバーへ伝播しない。
 * 包囲は強制ではなく、各自の接近方向＋占有回避から創発する結果。
 */
UCLASS(BlueprintType, DisplayName = "Encircle")
class TACTICALAI_API USlotGeneratorStrategy_Encircle : public USlotGeneratorStrategy
{
	GENERATED_BODY()

public:
	virtual FVector GenerateSlot(const FSlotGenContext& Context) const override;

	// 멤버 1명 기준으로 슬롯을 고르므로 멤버별 배정.
	// メンバー基準で生成するため個別割当。
	virtual ESlotAssignmentPolicy GetAssignmentPolicy() const override
	{
		return ESlotAssignmentPolicy::MemberSpecific;
	}

protected:
	// 서는 거리 = 자기 AttackRange × 이 비율.
	// 立ち位置＝自分のAttackRange×この比率。1.0未満の余裕が追撃ヒステリシスになる。
	UPROPERTY(EditAnywhere, Category = "Encircle", meta = (ClampMin = "0.1", ClampMax = "1.0"))
	float PreferredRangeRatio = 0.8f;
	
	// 링 위 동료 점유와 지킬 최소 각도 간격(도). 클수록 넓게 벌어져 포위가 빨리 펼쳐짐.
	// リング上の味方占有と保つ最小角度間隔（度）。大きいほど広く散開。
	UPROPERTY(EditAnywhere, Category = "Encircle", meta = (ClampMin = "0.0", ClampMax = "180.0"))
	float MinSeparationDeg = 45.f;

	// 점유 슬롯을 "이 타겟의 링 위"로 인정할 반경 오차(cm).
	// 링 반경 ± 이 값 안의 점유만 각도 경쟁에 참여 (다른 타겟·원거리 커밋은 자연 제외).
	// 占有スロットを「このリング上」と見なす半径許容誤差(cm)。他ターゲットの占有は自然に除外。
	UPROPERTY(EditAnywhere, Category = "Encircle", meta = (ClampMin = "0.0"))
	float RingOccupancyMargin = 150.f;

	// 접근 방향·해소 각도·경쟁 점유 디버그 표시.
	UPROPERTY(EditAnywhere, Category = "Encircle|Debug")
	bool bDrawDebug = false;
};