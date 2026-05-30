#pragma once
#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "AI/CrowdAvoidanceTypes.h"
#include "TacticalAvoidanceController.generated.h"

UINTERFACE(MinimalAPI)
class UTacticalAvoidanceController : public UInterface
{
	GENERATED_BODY()
};

/**
 * 빙의한 컨트롤러가 자기/자기 Pawn의 회피 역할을 세팅하는 단일 통로.
 * 외부는 GetController()를 이 인터페이스로 캐스팅해 명령하며,
 * 컨트롤러가 Player인지 AI인지 Enemy인지 알 필요가 없다.
 */
class TACTICALAI_API ITacticalAvoidanceController
{
	GENERATED_BODY()
public:
	virtual void SetAvoidanceRole(ECrowdAvoidanceRole CrowdRole) = 0;
};