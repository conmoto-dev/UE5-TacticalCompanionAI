#pragma once

#include "CoreMinimal.h"
#include "EnemyEncounterSettings.generated.h"

// ==================================================
// Enemy Encounter Settings
// 
// 스포너에서 편집하고 인카운터 생성 시 전달한다.
// Spawnerで編集し、エンカウンター生成時に渡す。
// ==================================================
USTRUCT()
struct TACTICALAI_API FEnemyEncounterSettings
{
	GENERATED_BODY()

	// 경계 진입 거리. 플레이어와 가장 가까운 멤버 사이의 거리가
	// 이 값 이하이면 Idle에서 Alert로 전환한다.
	// 警戒開始距離。プレイヤーと最寄りメンバーの距離が
	// この値以下の場合、IdleからAlertへ遷移する。
	UPROPERTY(EditAnywhere, Category = "Enemy Encounter|Sensing",
		meta = (ClampMin = "0.0", Units = "cm"))
	float AlertEnterDistance = 1700.f;

	// 경계 해제 거리. 플레이어와 가장 가까운 멤버 사이의 거리가
	// 이 값을 초과하면 Alert에서 Idle로 전환한다. 경계 진입 거리보다 크게 설정한다.
	// 警戒解除距離。プレイヤーと最寄りメンバーの距離が
	// この値を超えるとAlertからIdleへ遷移する。警戒開始距離より大きく設定する。
	UPROPERTY(EditAnywhere, Category = "Enemy Encounter|Sensing",
		meta = (ClampMin = "0.0", Units = "cm"))
	float AlertExitDistance = 2200.f;

	// 교전 진입 거리. 플레이어와 가장 가까운 멤버 사이의 거리가
	// 이 값 이하이면 Alert 또는 Return에서 Engaged로 전환한다.
	// 交戦開始距離。プレイヤーと最寄りメンバーの距離が
	// この値以下の場合、AlertまたはReturnからEngagedへ遷移する。
	UPROPERTY(EditAnywhere, Category = "Enemy Encounter|Sensing",
		meta = (ClampMin = "0.0", Units = "cm"))
	float EngageDistance = 1000.f;

	// 추격 포기 거리. 플레이어와 인카운터 기준 위치 사이의 거리가
	// 이 값을 초과하면 Engaged에서 Return으로 전환한다.
	// 追跡断念距離。プレイヤーとエンカウンターの基準位置の距離が
	// この値を超えるとEngagedからReturnへ遷移する。
	UPROPERTY(EditAnywhere, Category = "Enemy Encounter|Sensing",
		meta = (ClampMin = "0.0", Units = "cm"))
	float ChaseGiveUpDistance = 2500.f;

	// 복귀 완료 반경. 유효한 멤버 전원이 인카운터 기준 위치의
	// 이 반경 안에 있고 재교전 조건에 해당하지 않으면 Return에서 Idle로 전환한다.
	// 帰還完了半径。有効な全メンバーがエンカウンターの基準位置から
	// この半径内におり、再交戦条件を満たさない場合、ReturnからIdleへ遷移する。
	UPROPERTY(EditAnywhere, Category = "Enemy Encounter|Sensing",
		meta = (ClampMin = "0.0", Units = "cm"))
	float ReturnHomeRadius = 700.f;

	// 거리 감지와 교전 중 적대 대상 목록을 갱신하는 주기.
	// 距離感知と交戦中の敵対対象一覧を更新する周期。
	UPROPERTY(EditAnywhere, Category = "Enemy Encounter|Sensing",
		meta = (ClampMin = "0.05", Units = "s"))
	float SensingInterval = 0.25f;
};