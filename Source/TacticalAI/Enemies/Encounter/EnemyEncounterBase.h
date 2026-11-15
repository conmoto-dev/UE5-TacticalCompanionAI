#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Info.h"
#include "EnemyEncounterBase.generated.h"

class AEnemyCharacter;
class AEnemyEncounterBase;
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
	AEnemyEncounterBase*, Encounter, EEnemyEncounterState, OldState, EEnemyEncounterState, NewState);

// 전멸 통지. 파티 인지 등 외부 리스너가 구독한다.
// 全滅通知。パーティ知覚など外部リスナーが購読する。
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEnemyEncounterDefeated, AEnemyEncounterBase*, DefeatedEncounter);

// =========================================================================
// Enemy Encounter Base
//
// 파티가 인지하는 적 측 전투 단위의 공통 베이스.
// 멤버 명부·교전 상태·알려진 적·전멸 통지의 유일한 소유자.
// 파티 인지와 적 타겟팅은 이 타입만 참조하며, 하위 클래스를 모른다.
//
// 하위 클래스는 「멤버가 어디서 오는가」로 나뉜다.
//   AGroupEnemyEncounter  : 런타임에 스폰된 멤버. 생성 주체가 등록한다.
//   APlacedEnemyEncounter : 레벨에 배치된 멤버. 자신의 BeginPlay에서 등록한다.
// 베이스 자체에는 멤버를 얻는 경로가 없으므로 Abstract.
//
// AInfo인 이유: 렌더링 없는 정보 액터이며, 멤버가 모두 파괴된 뒤에도 남아
// 전멸 통지와 그 이후의 조회를 안전하게 처리하기 위함.
//
// パーティが知覚する敵側戦闘単位の共通基底。
// メンバー名簿・交戦状態・既知敵・全滅通知の唯一の所有者。
// パーティ知覚と敵ターゲッティングはこの型のみ参照し、サブクラスを知らない。
// =========================================================================
UCLASS(Abstract, Blueprintable)
class TACTICALAI_API AEnemyEncounterBase : public AInfo
{
	GENERATED_BODY()

public:
	AEnemyEncounterBase();

	virtual void Tick(float DeltaTime) override;

	// =========================================================================
	// Membership
	// 멤버 조회. 
	// メンバー照会。
	// =========================================================================

	/** 살아있는 멤버만 반환. 호출자는 항상 유효한 적만 받는다는 계약. */
	/** 生存メンバーのみ返す。呼び出し側は常に有効な敵だけ受け取る契約。 */
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
	
	// =========================================================================
	// 알려진 적. Engaged 동안만 유효. "교전 시작 = 상대 전원 인지" 규칙의 구현.
	// 점진 발견이 아니라 교전 진입 시 전량 파악. 스캔은 인카운터가 1회 (셀렉터별 중복 금지).
	// Engaged中のみ有効。「交戦開始＝相手全員を知覚」ルールの実装。スキャンはエンカウンターが1回。
	// =========================================================================
	UFUNCTION(BlueprintCallable, Category = "Enemy Encounter")
	TArray<AActor*> GetKnownHostiles() const;
	
protected:
	virtual void BeginPlay() override;

	// =========================================================================
	// 멤버 등록 공통 처리: 명부 추가 + back-ptr 설정 + 파괴 구독.
	// メンバー登録の共通処理：名簿追加＋back-ptr設定＋破壊購読。
	// =========================================================================
	void AddMember(AEnemyCharacter* Member);

private:
	// 멤버 파괴 = 사망으로 취급 (HP 시스템 도입 전의 사실).
	// HP가 생기면 사망 이벤트로 내부만 교체 — 외부 API는 유지.
	// メンバー破壊＝死亡扱い。導入後は内部だけ差し替え。
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
	
	// 멤버 명부. TObjectPtr UPROPERTY라 액터 파괴 시 자동 null —
	// メンバー名簿。Actor破壊時に自動でnullになる。
	UPROPERTY(VisibleAnywhere, Category = "Enemy Encounter")
	TArray<TObjectPtr<AEnemyCharacter>> Members;
	
	// =========================================================================
	// Encounter State
	// 거리는 모두 이 액터 위치(앵커) 또는 가장 가까운 멤버 기준.
	// 距離はすべてこのActor位置（アンカー）または最寄りメンバー基準。
	// =========================================================================

	// 경계 진입 거리. 플레이어가 이 안에 들어오면 Idle → Alert.
	// 警戒開始距離。プレイヤーがこの内側に入るとIdle→Alert。
	UPROPERTY(EditDefaultsOnly, Category = "Enemy Encounter|Sensing", meta = (ClampMin = "0.0", Units = "cm"))
	float AlertEnterDistance = 1700.f;

	// 경계 해제 거리. 진입보다 커야 함 (히스테리시스).
	// 警戒解除距離。開始より大きいこと（ヒステリシス）。
	UPROPERTY(EditDefaultsOnly, Category = "Enemy Encounter|Sensing", meta = (ClampMin = "0.0", Units = "cm"))
	float AlertExitDistance = 2200.f;

	// 교전 진입 거리. 플레이어가 이 안이면 Alert/Return → Engaged.
	// 交戦開始距離。この内側でAlert/Return→Engaged。
	UPROPERTY(EditDefaultsOnly, Category = "Enemy Encounter|Sensing", meta = (ClampMin = "0.0", Units = "cm"))
	float EngageDistance = 1000.f;

	// 추격 포기 거리 (앵커 기준). 플레이어가 앵커에서 이보다 멀면 Engaged → Return.
	// "내가 앵커에서 너무 벗어남" 판정은 적 이동 도입 시 추가.
	// 追跡諦め距離（アンカー基準）。「自分が離れすぎ」判定は移動導入時に追加。
	UPROPERTY(EditDefaultsOnly, Category = "Enemy Encounter|Sensing", meta = (ClampMin = "0.0", Units = "cm"))
	float ChaseGiveUpDistance = 2500.f;

	// 복귀 완료 반경. 생존 전원이 앵커의 이 안이면 Return → Idle.
	// 帰還完了半径。生存全員がこの内側でReturn→Idle。
	UPROPERTY(EditDefaultsOnly, Category = "Enemy Encounter|Sensing", meta = (ClampMin = "0.0", Units = "cm"))
	float ReturnHomeRadius = 700.f;
	
	// 감지 주기 (초).
	// 感知周期（秒）。
	UPROPERTY(EditDefaultsOnly, Category = "Enemy Encounter|Sensing", meta = (ClampMin = "0.05", Units = "s"))
	float SensingInterval = 0.25f;

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