#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "HomeSlotProvider.generated.h"

UINTERFACE(MinimalAPI, meta = (CannotImplementInterfaceInBlueprint))
class UHomeSlotProvider : public UInterface
{
	GENERATED_BODY()
};

/**
 * "배치 시스템이 지금 이 캐릭터에게 할당한 기준 위치(홈 슬롯)"를 답하는 계약.
 * 위치만 답한다 — 허용 반경을 얼마로 볼지, 어떻게 따라갈지는 소비하는 쪽이 정한다.
 *
 * ⚠ 모드마다 홈 슬롯의 의미가 다르다:
 *   전투 커밋 슬롯 = "이 근처에서 싸운다"의 기준 위치 (반경 허용 전제).
 *   추종 슬롯 = 끝까지 따라가야 하는 목적지.
 *   소비하는 쪽은 자기 문맥의 의미로만 사용할 것.
 *
 * 현재 구현: 파티 캐릭터(전투 모드의 커밋 슬롯만). 추종 슬롯·적 전투 배치·순찰 지점은
 * 그 값을 소비하는 시스템이 생길 때 각 구현부에 분기 추가 — 이 인터페이스는 무수정.
 *
 * 「配置システムが今このキャラに割り当てた基準位置（ホームスロット）」を答える契約。
 * 位置のみ答える。モードごとに意味が異なる点に注意 — 消費側は自分の文脈でのみ使用。
 */
class TACTICALAI_API IHomeSlotProvider
{
	GENERATED_BODY()

public:
	/**
	 * @param OutHomeSlot  유효 시 홈 슬롯의 월드 좌표.
	 * @return             현재 문맥에 홈 슬롯이 존재하는가 (전투 미진입·미커밋 등이면 false).
	 */
	virtual bool TryGetHomeSlot(FVector& OutHomeSlot) const = 0;
};