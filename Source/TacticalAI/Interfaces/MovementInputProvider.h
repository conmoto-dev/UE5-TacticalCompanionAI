#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "MovementInputProvider.generated.h"

UINTERFACE(MinimalAPI, meta = (CannotImplementInterfaceInBlueprint))
class UMovementInputProvider : public UInterface
{
	GENERATED_BODY()
};

// =======================================================
// 현재 이동 입력 방향을 제공하는 인터페이스.
// 現在の移動入力方向を提供するインターフェース。
// =======================================================
class TACTICALAI_API IMovementInputProvider
{
	GENERATED_BODY()

public:
	// 현재 이동 입력의 월드 좌표 기준 수평 방향을 반환한다.
	// 現在の移動入力をワールド座標の水平方向で返す。
	virtual FVector GetMovementInputDirection2D() const = 0;
};