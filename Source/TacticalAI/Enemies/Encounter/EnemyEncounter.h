#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Info.h"
#include "Enemies/Encounter/EnemyEncounterSettings.h"
#include "EnemyEncounter.generated.h"

class AEnemyCharacter;
class AEnemyEncounter;
class APawn;

// 적 측 교전 상태.
// 敵側の交戦状態。
UENUM(BlueprintType)
enum class EEnemyEncounterState : uint8
{
	Idle,      // 평시
	Alert,     // 경계 (인지했으나 미교전)
	Engaged,   // 교전·추격
	Return,    // 포기·복귀
};

// 상태 전이 통지. 개별 행동(StateTree)·디버그가 구독한다.
// 状態遷移通知。個体行動(StateTree)・デバッグが購読。
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnEnemyEncounterStateChanged,
	AEnemyEncounter*, Encounter, EEnemyEncounterState, OldState, EEnemyEncounterState, NewState);

// 전멸 통지. 파티 인지 등 외부 리스너가 구독한다.
// 全滅通知。パーティ知覚など外部リスナーが購読する。
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEnemyEncounterDefeated, AEnemyEncounter*, DefeatedEncounter);

// ==================================================
// Enemy Encounter
//
// 스포너가 생성한 적들의 교전 단위.
// 멤버 목록, 교전 상태, 알려진 적, 전멸 통지를 관리한다.
// 단일 적과 그룹 적은 동일한 등록 및 교전 처리를 사용한다.
// 파티 인지와 적 타겟팅은 이 인카운터를 조회한다.
//
// Spawnerが生成した敵の交戦単位。
// メンバー一覧、交戦状態、既知の敵、全滅通知を管理する。
// 単体の敵とグループの敵は同じ登録処理と交戦処理を使用する。
// パーティ知覚と敵ターゲッティングはこのエンカウンターを参照する。
// ==================================================
UCLASS(NotBlueprintable, NotPlaceable)
class TACTICALAI_API AEnemyEncounter : public AInfo
{
	GENERATED_BODY()

public:
	AEnemyEncounter();

	virtual void Tick(float DeltaTime) override;

	// ==================================================
	// EnemyEncounterSettings Initialization
	// ==================================================
	void InitializeSettings(const FEnemyEncounterSettings& InSettings);
	
	// ==================================================
	// Membership
	//
	// 생성된 적을 등록하고 유효한 멤버를 조회한다.
	// 生成された敵を登録し、有効なメンバーを取得する。
	// ==================================================

	// 멤버 목록 추가, 소속 인카운터 설정, 파괴 이벤트 구독.
	// メンバー一覧への追加、所属エンカウンターの設定、破壊イベントの購読。
	void RegisterMember(AEnemyCharacter* Member);
	
	// 유효한 멤버 액터를 반환한다. 
	// 有効なメンバーActorを返す。
	UFUNCTION(BlueprintCallable, Category = "Enemy Encounter")
	TArray<AEnemyCharacter*> GetAliveMembers() const;

	UFUNCTION(BlueprintCallable, Category = "Enemy Encounter")
	bool IsDefeated() const;

	/** 전멸 통지. 브로드캐스트 후에도 인카운터 액터는 스스로 파괴하지 않는다
	 *  (리스너가 콜백 안에서 안전하게 조회 가능하도록). */
	/** 全滅通知。 */
	UPROPERTY(BlueprintAssignable, Category = "Enemy Encounter")
	FOnEnemyEncounterDefeated OnEncounterDefeated;

	// =========================================================================
	// 교전 상태
	// 전이 구동은 딱 2경로: ① 저주기 자기 감지(플레이어 기준 거리) ② 피격 이벤트.
	// 파티가 이 상태를 쓰는 일은 없다 — 파티→인카운터는 읽기 전용 (ADR-0008).
	// 交戦状態。遷移駆動は2経路のみ：①低頻度の自己感知 ②被弾イベント。
	// =========================================================================
	UFUNCTION(BlueprintCallable, Category = "Enemy Encounter")
	EEnemyEncounterState GetEncounterState() const { return EncounterState; }

	//피격 통지. 어느 상태에서든 즉시 교전 진입 (기습 경로).
	//被弾通知。どの状態からでも即時交戦へ（奇襲経路）。
	UFUNCTION(BlueprintCallable, Category = "Enemy Encounter")
	void NotifyAttackedBy(AActor* Attacker);

	UPROPERTY(BlueprintAssignable, Category = "Enemy Encounter")
	FOnEnemyEncounterStateChanged OnEncounterStateChanged;
	
	// ==================================================
	// Known Hostiles
	//
	// 교전 진입 시와 교전 중 감지 주기마다 인카운터가 후보를 갱신한다.
	// 개별 타겟 셀렉터는 이 목록을 조회한다. 교전 상태를 벗어나면 비운다.
	//
	// 交戦開始時と交戦中の感知周期ごとに候補を更新する。
	// 各ターゲットセレクタはこの一覧を参照する。交戦状態を離れるとクリアする。
	// ==================================================
	UFUNCTION(BlueprintCallable, Category = "Enemy Encounter")
	TArray<AActor*> GetKnownHostiles() const;
	
protected:
	virtual void BeginPlay() override;

private:
	// 파괴된 멤버를 목록에서 제거하고 남은 멤버가 없으면 전멸을 통지한다.
	// 破壊されたメンバーを一覧から除外し、残りのメンバーがいなければ全滅を通知する。
	UFUNCTION()
	void HandleMemberDestroyed(AActor* DestroyedActor);

	void DrawDebugEncounter() const;

	void TickSensing(float DeltaTime);
	void SetEncounterState(EEnemyEncounterState NewState);
	const APawn* GetSensedPlayerPawn() const;
	bool AreAllMembersNearAnchor() const;
	
	void RefreshKnownHostiles();
	
private:
	// =========================================================================
	// Encounter Member
	// =========================================================================
	
	// 등록된 멤버 참조. 조회 시 유효성을 검사하고 파괴 통지 시 목록에서 제거한다.
	// 登録済みメンバーへの参照。取得時に有効性を確認し、破壊通知時に一覧から除外する。
	UPROPERTY(VisibleAnywhere, Category = "Enemy Encounter")
	TArray<TObjectPtr<AEnemyCharacter>> Members;
	
	// ==================================================
	// Encounter Settings
	// ==================================================

	// 생성 시 전달받은 교전 설정. 편집은 스포너에서 수행한다.
	// 生成時に渡された交戦設定。編集はSpawnerで行う。
	UPROPERTY(VisibleAnywhere, Category = "Enemy Encounter")
	FEnemyEncounterSettings Settings;

	// ==================================================
	// Encounter State
	//
	// 현재 교전 상태와 감지 시간, 알려진 적을 관리한다.
	// 現在の交戦状態、感知時間、既知の敵を管理する。
	// ==================================================

	// 현재 교전 상태. 
	// 現在の交戦状態。
	EEnemyEncounterState EncounterState = EEnemyEncounterState::Idle;

	// 다음 감지까지 남은 시간. 첫 감지를 랜덤 위상으로 시작해 인카운터 간 동기화 방지.
	// 次感知までの残時間。初回をランダム位相にしエンカウンター間の同期を防ぐ。
	float TimeUntilNextSense = 0.f;
	
	// 알려진 적 캐시. 약참조, 조회 시 유효성 필터. Engaged 이탈 시 비움.
	// 既知敵キャッシュ。非所有 — 弱参照。Engaged離脱時にクリア。
	TArray<TWeakObjectPtr<AActor>> KnownHostiles;
};