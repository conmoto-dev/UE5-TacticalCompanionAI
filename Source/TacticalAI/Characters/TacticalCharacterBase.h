#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "AbilitySystemInterface.h"
#include "AI/Targeting/Targetable.h"
#include "TacticalCharacterBase.generated.h"

class UAbilitySystemComponent;
class UGameplayAbility;
class UTacticalAbilitySystemComponent;
class UTacticalCombatAttributeSet;

// =========================================================================
// 동료·적이 공유하는 캐릭터 베이스 (카메라·입력 없음).
//
// 책임을 나누는 기준은 "플레이어 빙의 가능 여부".
// 카메라·입력은 빙의 가능한 동료(PartyCharacter)에 두고,
// 빙의와 무관한 공통 이동 설정·전투 기반·타깃 자격은 이 층에서 담당한다.
//
// 같은 동료가 플레이어와 AI 사이를 전환하더라도,
// ASC와 부여된 Ability는 캐릭터에 유지된다.
// 조작 주체는 Ability 요청 시점을 결정하고, 실제 실행은 GAS가 담당한다.
//
// 仲間・敵が共有するキャラクター基底（カメラ・入力なし）。
//
// 責務を分ける基準は「プレイヤー憑依の可否」。
// カメラ・入力は憑依可能な仲間（PartyCharacter）に置き、
// 憑依に依存しない共通移動設定・戦闘基盤・ターゲット資格はこの層が担当する。
//
// 同じ仲間がプレイヤーとAIの間で切り替わっても、
// ASCと付与済みAbilityはキャラクターに保持する。
// 操作主体がAbilityの要求タイミングを判断し、実行はGASが担当する。
// =========================================================================
UCLASS(Abstract)
class TACTICALAI_API ATacticalCharacterBase
	: public ACharacter, public ITargetable, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	ATacticalCharacterBase();

	// =========================================================================
	// Combat Target
	//
	// 「전투 타깃이 될 수 있다」는 아군·적의 공통 성질이므로 베이스에서 구현한다.
	// 무적 등 캐릭터별 예외는 서브클래스에서 재정의한다.
	//
	// 「戦闘ターゲットになれる」という性質は両陣営共通のため、基底で実装する。
	// 無敵などキャラクター固有の例外はサブクラスで上書きする。
	// =========================================================================
	virtual bool IsTargetable_Implementation() const override;
	virtual float GetEncircleRadius_Implementation() const override;

	// =========================================================================
	// Ability System
	//
	// 캐릭터는 ASC를 소유하고, 처음 보유할 Ability 목록을 제공한다.
	// ASC는 Ability 목록·활성 상태·태그를 관리하고 발동 요청을 처리한다.
	//
	// 공격 대상 선정과 요청 시점은 AI·입력 레이어가 판단하며,
	// 발동 후 공격 진행과 종료 시점은 각 Gameplay Ability가 결정한다.
	//
	// キャラクターはASCを所有し、初期Ability一覧を提供する。
	// ASCはAbility一覧・発動状態・タグを管理し、発動要求を処理する。
	//
	// ターゲット選択と要求タイミングはAI・入力レイヤーが判断し、
	// 発動後の攻撃進行と終了タイミングは各Gameplay Abilityが決定する。
	// =========================================================================

	// GAS 공통 인터페이스용 접근점.
	// 호출자가 Tactical 전용 클래스를 몰라도 이 캐릭터의 ASC를 조회할 수 있다.
	//
	// GAS共通Interface用のアクセスポイント。
	// 呼び出し側がTactical固有のクラスを知らなくてもASCを取得できる。
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	// 플레이어·AI 요청 등 Tactical 전용 함수를 사용하기 위한 접근점.
	// 위의 공통 Getter와 동일한 ASC를 반환하며, 별도 컴포넌트를 생성하지 않는다.
	//
	// プレイヤー・AI要求など、Tactical固有の関数を使用するためのアクセスポイント。
	// 共通Getterと同じASCを返し、別のComponentは生成しない。
	UFUNCTION(BlueprintPure, Category = "Ability System")
	UTacticalAbilitySystemComponent* GetTacticalAbilitySystemComponent() const;

protected:
	// Controller가 아직 없어도 캐릭터를 ASC의 Owner/Avatar로 초기화한다.
	// ASC 초기화 후 권한이 있는 경우 시작 Ability를 부여한다.
	//
	// Controllerが未設定でも、キャラクターをASCのOwner・Avatarとして初期化する。
	// ASC 初期化後、権限がある場合StartupAbilitiesを付与する。
	virtual void BeginPlay() override;

	// 동료 스왑으로 조작 주체가 바뀌면 ASC의 Controller 관련 참조를 갱신한다.
	// ASC의 Owner/Avatar는 캐릭터로 유지하며, Ability를 다시 부여하지 않는다.
	//
	// 仲間の切替で操作主体が変わった際、ASCのController関連参照を更新する。
	// Owner・Avatarはキャラクターのまま維持し、Abilityは再付与しない。
	virtual void NotifyControllerChanged() override;

	// 캐릭터별 Ability 목록·활성 Gameplay Effect·태그를 소유한다.
	// 플레이어와 AI가 같은 전투 실행 기반을 사용하도록 캐릭터에 둔다.
	//
	// キャラクターごとのAbility一覧・有効なGameplay Effect・タグを保持する。
	// プレイヤーとAIが同じ戦闘実行基盤を使えるよう、キャラクターに配置する。
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ability System")
	TObjectPtr<UTacticalAbilitySystemComponent> AbilitySystemComponent;

	// 전 캐릭터가 공통으로 사용하는 전투 Attribute 정의.
	// 클래스는 공유하지만 값은 캐릭터별 인스턴스에 보관한다.
	// 전투 중 값 변경은 ASC와 Gameplay Effect를 통해 처리한다.
	//
	// 全キャラクター共通の戦闘Attribute定義。
	// クラスは共通だが、値はキャラクターごとのインスタンスに保持する。
	// 戦闘中の値変更はASCとGameplay Effectを通して処理する。
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ability System")
	TObjectPtr<UTacticalCombatAttributeSet> CombatAttributeSet;

	// 캐릭터 BP의 Class Defaults에서 지정하는 시작 Ability 목록.
	// 캐릭터는 「무엇을 보유할지」를 정하고, 실제 부여 절차는 ASC에 맡긴다.
	// 부여는 사용 가능한 Ability를 등록하는 것이며, 즉시 발동시키지는 않는다.
	//
	// キャラクターBPのClass Defaultsで指定する初期Ability一覧。
	// キャラクターが「何を保有するか」を決め、付与処理はASCに委ねる。
	// 付与は使用可能なAbilityの登録であり、即時発動ではない。
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability System")
	TArray<TSubclassOf<UGameplayAbility>> StartupAbilities;

private:
	// 포위 진형의 기준 반경. 
	// 실제 충돌 크기와 진형 배치 간격을 독립적으로 조정하기 위해 캡슐 충돌 반경과 분리해서 보관한다.
	//
	// 包囲隊形の基準半径。大型ボスなどには大きな値を設定する。
	// 衝突サイズと隊形の配置間隔を独立して調整できるよう、カプセルのコリジョン半径とは分けて保持する。
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat Target",
		meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
	float EncircleRadius = 150.f;
};