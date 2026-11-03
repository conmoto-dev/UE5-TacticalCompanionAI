#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "Interfaces/TacticalAvoidanceController.h"
#include "CompanionAIController.generated.h"

class UStateTreeAIComponent;

/**
 * 
 */
UCLASS()
class TACTICALAI_API ACompanionAIController : public AAIController,
											  public ITacticalAvoidanceController
{
	GENERATED_BODY()
	
public:
	// 기본 생성자 대신 FObjectInitializer를 파라미터로 받는 생성자로 변경
	ACompanionAIController(const FObjectInitializer& ObjectInitializer);
	
	virtual void SetAvoidanceRole(ECrowdAvoidanceRole CrowdRole) override;

protected:
	virtual void BeginPlay() override;
	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnUnPossess() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	
private:
	// =======================================================
	// StateTree 실행 관리
	// 최초 시작과 재빙의에서 같은 시작 조건을 사용한다.
	//
	// StateTreeの実行管理。
	// =======================================================

	void TryStartStateTree();

	// Controller가 소유하는 StateTree 실행 컴포넌트.
	// Controllerが所有するStateTree実行コンポーネント。
	UPROPERTY(VisibleAnywhere, Category = "AI|StateTree")
	TObjectPtr<UStateTreeAIComponent> StateTreeComponent;
};
