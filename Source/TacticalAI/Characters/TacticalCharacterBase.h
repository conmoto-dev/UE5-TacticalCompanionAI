#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "AI/Targeting/Targetable.h"
#include "TacticalCharacterBase.generated.h"

// =========================================================================
// 동료·적이 공유하는 캐릭터 베이스 (카메라·입력 없음).
// 경계 기준은 "플레이어 빙의 가능 여부" — 카메라/입력은 빙의되는 동료(APartyCharacter)
// 에만 두고, 빙의와 무관한 공통 이동 특성(회전·속도 기본값)만 이 층에 모은다.
// 향후 HP·피격·사망 등 동료/적 공통 자산이 생기면 여기에 올린다(현재는 자리만 확보).
// =========================================================================
// 仲間と敵が共有するキャラ基底（カメラ・入力なし）。
// 境界は「プレイヤー憑依の可否」— カメラ/入力は憑依される仲間側にのみ置き、
// ここには憑依に依存しない共通の移動特性のみ集約する。
UCLASS(Abstract)
class TACTICALAI_API ATacticalCharacterBase : public ACharacter, public ITargetable
{
	GENERATED_BODY()

public:
	ATacticalCharacterBase();
	
	// =========================================================================
	// ITargetable 구현 (양 진영 공통).
	// "전투 타겟이 될 수 있다"는 아군·적 캐릭터의 공통 성질이라 베이스가 담당.
	// 무적 프레임 등 진영별 예외는 서브클래스가 오버라이드.
	// ITargetable実装(両陣営共通)。無敵フレーム等の例外はサブクラスで上書き。
	// =========================================================================
	virtual bool IsTargetable_Implementation() const override;
	virtual float GetEncircleRadius_Implementation() const override;

private:
	// 포위 진형의 베이스 반경. 큰 보스 = 큰 값. 충돌 반경과 분리된 연출 값.
	// 包囲隊形のベース半径。大型ボス＝大きい値。コリジョンとは別の演出値。
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat Target",
		meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
	float EncircleRadius = 150.f;
};