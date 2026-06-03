// SlotGeneratorStrategy.h
#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "SlotGeneratorStrategy.generated.h"

/**
 * 추상 슬롯 생성 전략. anchor 로컬 공간 기준 슬롯 오프셋을 생성한다.
 * 월드 변환은 호출부(공통 파이프라인)가 anchor로 처리 — Strategy는 월드/타겟을 모른다.
 * Yield Strategy와 동일 패턴: stateless, DataAsset Flyweight 공유 안전.
 *
 * 抽象スロット生成戦略。anchorローカル空間のオフセットのみ生成。
 * ワールド変換は呼び出し側がanchorで処理。Strategyはワールド・ターゲットを知らない。
 */
UCLASS(Abstract, EditInlineNew, Blueprintable, BlueprintType)
class TACTICALAI_API USlotGeneratorStrategy : public UObject
{
	GENERATED_BODY()

public:
	/**
	 * anchor 로컬 공간 기준 슬롯 오프셋 N개 생성.
	 * @param NumSlots    배치 인원
	 * @param BaseRadius  호출부가 산출한 최종 반경 (기본값 + 타겟 크기 보정 등).
	 *                    Strategy는 이 값의 출처를 모른다 (타겟 actor 비의존).
	 * @param OutLocalOffsets  [out] 로컬 오프셋 결과. 호출 측이 anchor로 월드 변환.
	 */
	virtual void GenerateSlots(int32 NumSlots, float BaseRadius, TArray<FVector>& OutLocalOffsets) const
		PURE_VIRTUAL(USlotGeneratorStrategy::GenerateSlots, );
};