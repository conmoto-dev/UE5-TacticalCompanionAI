#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TargetDummy.generated.h"

class UCapsuleComponent;
class UStaticMeshComponent;

/**
 * 전투 진형 테스트용 허수아비 타겟.
 * AI·체력·공격 없음. 동료 진형이 둘러쌀 "대상"으로만 존재.
 * EncircleRadius로 동료가 둘러쌀 거리를 노출 (충돌 반경과 분리 — 연출 정책).
 *
 * テスト用の的。AI・HP・攻撃なし。隊形が囲む対象としてのみ存在。
 */
UCLASS()
class TACTICALAI_API ATargetDummy : public AActor
{
	GENERATED_BODY()

public:
	ATargetDummy();

	/** 동료가 이 타겟을 둘러쌀 때의 베이스 반경. 충돌과 별개의 연출 값. */
	/** 仲間がこの的を囲む際のベース半径。コリジョンとは別の演出値。 */
	UFUNCTION(BlueprintCallable, Category = "TargetDummy")
	float GetEncircleRadius() const { return EncircleRadius; }

protected:
	// 루트 충돌 (서 있기 + 클릭 선택용). 안 움직이니 Static.
	UPROPERTY(VisibleAnywhere, Category = "TargetDummy")
	TObjectPtr<UCapsuleComponent> Capsule;

	// 시각용 메시 (에디터에서 큐브/실린더 할당).
	UPROPERTY(VisibleAnywhere, Category = "TargetDummy")
	TObjectPtr<UStaticMeshComponent> Mesh;

	// 동료가 둘러쌀 거리. 디자이너 조정용 (큰 보스 = 큰 값).
	UPROPERTY(EditAnywhere, Category = "TargetDummy", meta = (ClampMin = "0.0"))
	float EncircleRadius = 150.f;
};