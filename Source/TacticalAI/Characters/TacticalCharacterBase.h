#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "AbilitySystemInterface.h"
#include "AI/Targeting/Targetable.h"
#include "TacticalCharacterBase.generated.h"

class UAbilitySystemComponent;
class UTacticalCombatAttributeSet;

// =========================================================================
// 동료·적이 공유하는 캐릭터 베이스 (카메라·입력 없음).
// 경계 기준은 "플레이어 빙의 가능 여부" — 카메라/입력은 빙의되는 동료(APartyCharacter)
// 에만 두고, 빙의와 무관한 공통 이동 특성(회전·속도 기본값)만 이 층에 모은다.
// =========================================================================
// 仲間と敵が共有するキャラ基底（カメラ・入力なし）。
// 境界は「プレイヤー憑依の可否」— カメラ/入力は憑依される仲間側にのみ置き、
// ここには憑依に依存しない共通の移動特性のみ集約する。
UCLASS(Abstract)
class TACTICALAI_API ATacticalCharacterBase : public ACharacter, public ITargetable, public IAbilitySystemInterface
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
	
	// =========================================================================
	// Ability System
	//
	// ASC는 어빌리티 목록·활성 상태·태그를 소유하고 발동 조건을 검사한다.
	// 공격 대상 선정과 Ability 발동 요청 시점 판단은 AI/입력 레이어의 책임이며,
	// 각 Ability는 발동 후 실행 흐름과 종료 시점을 관리한다.
	//
	// ASCはアビリティ一覧・発動状態・タグを所有し、発動条件を検査する。
	// ターゲット選択とAbilityの発動要求タイミングはAI・入力レイヤーの責務とし、
	// 各Abilityは発動後の実行フローと終了タイミングを管理する。
	// =========================================================================
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

protected:
	// 배치 직후 아직 Controller가 없더라도 ASC의 Owner/Avatar를 초기화한다.
	// 配置直後にControllerが無い場合でもASCのOwner・Avatarを初期化する。
	virtual void BeginPlay() override;
	
	// 각 캐릭터의 어빌리티 실행 상태를 소유하는 GAS 컴포넌트.
	// 어빌리티 목록·활성 GE·태그를 소유.
	// このキャラクターのアビリティ実行状態を所有するGASコンポーネント。
	// アビリティ・活性GE・タグを所有。
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ability System")
	TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;
	
	// 캐릭터 공통 전투 관련 Attribute를 보관하는 GAS AttributeSet.
	// 값 변경은 ASC와 Gameplay Effect를 통해 처리하며 외부에서 직접 수정하지 않는다.
	//
	// 戦闘関連のAttributeを保持するGAS AttributeSet。
	// 値の変更はASCとGameplay Effectを通して行い、外部から直接変更しない。
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ability System")
	TObjectPtr<UTacticalCombatAttributeSet> CombatAttributeSet;
	
private:
	// 포위 진형의 베이스 반경. 큰 보스 = 큰 값. 충돌 반경과 분리된 연출 값.
	// 包囲隊形のベース半径。大型ボス＝大きい値。コリジョンとは別の演出値。
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat Target",
		meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
	float EncircleRadius = 150.f;
};