#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "PartyPerceptionComponent.generated.h"

class AEnemyEncounter;
class APartyManager;
class APartyCharacter;

// ==================================================
// Party Perception Component
//
// 리더 위치를 기준으로 적 인카운터를 감지한다.
// 하나의 멤버라도 진입 반경 안에 있으면 소속 인카운터를 인지한다.
// 인지 목록을 관리하며, 파티의 교전 여부는 결정하지 않는다.
//
// リーダー位置を基準に敵エンカウンターを検知する。
// メンバー1体でも開始半径内にいれば所属エンカウンターを知覚する。
// 知覚一覧を管理し、パーティの交戦判断は行わない。
// ==================================================
UCLASS(ClassGroup=(Custom))
class TACTICALAI_API UPartyPerceptionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UPartyPerceptionComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

	// =========================================================================
	// 조회 API. 호출자는 항상 유효·비전멸 인카운터만 받는다는 계약.
	// 照会API。呼び出し側は常に有効・未全滅のエンカウンターのみ受け取る契約。
	// =========================================================================
	TArray<AEnemyEncounter*> GetPerceivedEncounters() const;

	// 반경 검사 없이 지정한 인카운터를 인지 목록에 추가한다.
	// 半径判定を行わず、指定したエンカウンターを知覚一覧に追加する。
	void MarkEncounterPerceived(AEnemyEncounter* Encounter);

protected:
	virtual void BeginPlay() override;

private:
	void ScanForEncounters();
	bool IsAnyMemberInRadius(const AEnemyEncounter* Encounter, const FVector& Origin, float RadiusSq) const;
	const APartyCharacter* GetLeader() const;
	void DrawDebugPerception(const FVector& Origin) const;

private:
	// 인지 진입 반경. 이 안에 적이 들어오면 그 인카운터를 인지.
	// 知覚開始半径。この内側に敵が入るとそのエンカウンターを知覚。
	UPROPERTY(EditAnywhere, Category = "Perception", meta = (ClampMin = "0.0"))
	float PerceiveEnterRadius = 1200.f;

	// 인지 이탈 반경. 전원이 이 밖으로 나가면 인지 해제. Enter보다 커야 함 (히스테리시스).
	// 知覚解除半径。全員がこの外に出ると解除。Enterより大きいこと（ヒステリシス）。
	UPROPERTY(EditAnywhere, Category = "Perception", meta = (ClampMin = "0.0"))
	float PerceiveExitRadius = 1500.f;

	// 감지 스캔 간격 (초). 인지는 프레임 정밀도가 불필요.
	// 検知スキャン間隔（秒）。知覚にフレーム精度は不要。
	UPROPERTY(EditAnywhere, Category = "Perception", meta = (ClampMin = "0.05"))
	float ScanInterval = 0.2f;

	// 인지 중인 인카운터. 소유 아님 — 약참조, 조회 시 유효성 필터.
	// 知覚中のエンカウンター。所有ではない — 弱参照、照会時に有効性フィルタ。
	TArray<TWeakObjectPtr<AEnemyEncounter>> PerceivedEncounters;
};