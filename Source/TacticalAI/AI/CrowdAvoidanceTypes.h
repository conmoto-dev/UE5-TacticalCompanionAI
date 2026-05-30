#pragma once
#include "CoreMinimal.h"
#include "CrowdAvoidanceTypes.generated.h"

UENUM(BlueprintType)
enum class ECrowdAvoidanceRole : uint8
{
	Leader   UMETA(DisplayName = "Leader"),    // 플레이어 빙의. 아무도 안 피함.
	Normal   UMETA(DisplayName = "Normal"),    // 일반 동료.
	Yielding UMETA(DisplayName = "Yielding"),  // 양보 중.
};

// Detour 비트마스크. 위 enum 순번(0,1,2)과는 별개의 비트값(1,2,4).
namespace CrowdGroupBits
{
	constexpr int32 Leader   = 1 << 0; // 1
	constexpr int32 Normal   = 1 << 1; // 2
	constexpr int32 Yielding = 1 << 2; // 4
}