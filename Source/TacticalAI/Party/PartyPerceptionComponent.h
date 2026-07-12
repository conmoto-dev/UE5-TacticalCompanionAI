#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "PartyPerceptionComponent.generated.h"

class AEnemyGroup;
class APartyManager;
class APartyCharacter;

// =========================================================================
// Party Perception Component
//
// 파티의 적 그룹 인지를 담당. 감지 기준은 리더(=플레이어) 위치.
// 적 1체라도 반경에 들어오면 소속 그룹 전체를 인지로 승격한다 (그룹 단위 인지).
// "인지"까지만 담당 — 교전 여부는 별도 레이어(현재는 모드 결정)의 책임.
//
// パーティの敵グループ知覚を担当。基準はリーダー（＝プレイヤー）位置。
// 敵1体でも半径内に入れば所属グループ全体を知覚に昇格（グループ単位知覚）。
// 「知覚」まで — 交戦判断は別レイヤーの責務。
// =========================================================================
UCLASS(ClassGroup=(Custom))
class TACTICALAI_API UPartyPerceptionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UPartyPerceptionComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

	// =========================================================================
	// 조회 API. 호출자는 항상 유효·비전멸 그룹만 받는다는 계약.
	// 照会API。呼び出し側は常に有効・未全滅のグループのみ受け取る契約。
	// =========================================================================
	TArray<AEnemyGroup*> GetPerceivedGroups() const;

	// 반경 감지를 거치지 않는 즉시 인지 (기습 피격 등 교전 이벤트용 문).
	// 호출자는 추후 전투 스텝에서 생긴다.
	// 半径検知を経ない即時知覚（奇襲被弾など交戦イベント用の入口）。
	void MarkGroupPerceived(AEnemyGroup* Group);

protected:
	virtual void BeginPlay() override;

private:
	void ScanForGroups();
	bool IsAnyMemberInRadius(const AEnemyGroup* Group, const FVector& Origin, float RadiusSq) const;
	const APartyCharacter* GetLeader() const;
	void DrawDebugPerception(const FVector& Origin) const;

private:
	// 인지 진입 반경. 이 안에 적이 들어오면 그 그룹을 인지.
	// 知覚開始半径。この内側に敵が入るとそのグループを知覚。
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

	// 인지 중인 그룹. 소유 아님 — 약참조, 조회 시 유효성 필터.
	// 知覚中のグループ。所有ではない — 弱参照、照会時に有効性フィルタ。
	TArray<TWeakObjectPtr<AEnemyGroup>> PerceivedGroups;
};