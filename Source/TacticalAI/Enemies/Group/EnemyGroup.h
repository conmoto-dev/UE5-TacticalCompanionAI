#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Info.h"
#include "EnemyGroup.generated.h"

class AEnemyCharacter;
class AEnemyGroup;
class APawn;

// 적 그룹의 전황 상태. 그룹이 유일한 소유자 — 멤버는 상태를 갖지 않고 읽기만 한다.
// 敵グループの戦況状態。所有者はグループのみ — メンバーは状態を持たず読むだけ。
UENUM(BlueprintType)
enum class EEnemyGroupState : uint8
{
	Idle,      // 평시
	Alert,     // 경계 (인지했으나 미교전)
	Engaged,   // 교전·추격
	Return,    // 포기·복귀
};

// 상태 전이 통지. 개별 행동(추후 StateTree)·적 진형 전환·디버그가 구독한다.
// 状態遷移通知。個体行動(将来StateTree)・敵隊形切替・デバッグが購読。
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnEnemyGroupStateChanged,
	AEnemyGroup*, Group, EEnemyGroupState, OldState, EEnemyGroupState, NewState);


// 그룹 전멸 통지. 파티 인지 등 외부 리스너가 구독한다.
// グループ全滅通知。パーティ知覚など外部リスナーが購読する。
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEnemyGroupDefeated, AEnemyGroup*, DefeatedGroup);

// =========================================================================
// Enemy Group
//
// 적 그룹의 런타임 개체. 멤버 명부와 그룹 수준 상태의 유일한 소유자.
// 그룹 경계는 스폰 데이터가 정하고(ADR-0006), 스포너가 스폰 시 생성·등록한다.
// AInfo인 이유: 렌더링 없는 정보 액터이면서, 추후 그룹 StateTree의
// 숙주(UStateTreeComponent 부착 대상)가 되기 위함.
//
// 敵グループのランタイム実体。メンバー名簿とグループ状態の唯一の所有者。
// グループ境界はスポーンデータが決め、Spawnerが生成・登録する。
// AInfoである理由：描画不要の情報Actorであり、将来グループStateTreeの
// ホストになるため。
// =========================================================================
UCLASS(Blueprintable)
class TACTICALAI_API AEnemyGroup : public AInfo
{
	GENERATED_BODY()

public:
	AEnemyGroup();

	virtual void Tick(float DeltaTime) override;

	// =========================================================================
	// Membership
	// 멤버 등록·조회. 등록은 스포너(또는 미래의 인카운터 스크립트)만 호출.
	// back-ptr 설정 경로를 여기 하나로 강제해, 캐릭터가 스스로 그룹을
	// 정하는 코드가 존재하지 않게 한다.
	// 登録経路をここに一本化し、キャラ側が勝手に所属を決めないようにする。
	// =========================================================================
	void RegisterMember(AEnemyCharacter* Member);

	/** 살아있는 멤버만 반환. 호출자는 항상 유효한 적만 받는다는 계약. */
	/** 生存メンバーのみ返す。呼び出し側は常に有効な敵だけ受け取る契約。 */
	UFUNCTION(BlueprintCallable, Category = "Enemy Group")
	TArray<AEnemyCharacter*> GetAliveMembers() const;

	UFUNCTION(BlueprintCallable, Category = "Enemy Group")
	bool IsDefeated() const;

	/** 전멸 통지. 브로드캐스트 후에도 그룹 액터는 스스로 파괴하지 않는다
	 *  (리스너가 콜백 안에서 안전하게 조회 가능하도록). */
	/** 全滅通知。通知後もグループActorは自壊しない（リスナーの安全な照会のため）。 */
	UPROPERTY(BlueprintAssignable, Category = "Enemy Group")
	FOnEnemyGroupDefeated OnGroupDefeated;

	// =========================================================================
	// 전황 상태 (Step A)
	// 전이 구동은 딱 2경로: ① 저주기 자기 감지(플레이어 기준 거리) ② 피격 이벤트.
	// 파티가 이 상태를 쓰는 일은 없다 — 파티→그룹은 읽기 전용 (ADR-0008).
	// 遷移駆動は2経路のみ：①低頻度の自己感知 ②被弾イベント。
	// =========================================================================
	UFUNCTION(BlueprintCallable, Category = "Enemy Group")
	EEnemyGroupState GetGroupState() const { return GroupState; }

	/** 피격 통지. 어느 상태에서든 즉시 교전 진입 (기습 경로).
	 *  호출자는 추후 전투 시스템 — 지금은 테스트용 수동 호출. */
	/** 被弾通知。どの状態からでも即時交戦へ（奇襲経路）。 */
	UFUNCTION(BlueprintCallable, Category = "Enemy Group")
	void NotifyAttackedBy(AActor* Attacker);

	UPROPERTY(BlueprintAssignable, Category = "Enemy Group")
	FOnEnemyGroupStateChanged OnGroupStateChanged;
	
	// =========================================================================
	// 알려진 적. Engaged 동안만 유효. "교전 시작 = 상대 전원 인지" 규칙의 구현.
	// 점진 발견이 아니라 교전 진입 시 전량 파악. 스캔은 그룹이 1회 (셀렉터별 중복 금지).
	// Engaged中のみ有効。「交戦開始＝相手全員を知覚」ルールの実装。スキャンはグループが1回。
	// =========================================================================
	UFUNCTION(BlueprintCallable, Category = "Enemy Group")
	TArray<AActor*> GetKnownHostiles() const;
	
protected:
	virtual void BeginPlay() override;

private:
	// 멤버 파괴 = 사망으로 취급 (HP 시스템 도입 전의 사실).
	// HP가 생기면 사망 이벤트로 내부만 교체 — 외부 API는 유지.
	// メンバー破壊＝死亡扱い（HP導入前の暫定）。導入後は内部だけ差し替え。
	UFUNCTION()
	void HandleMemberDestroyed(AActor* DestroyedActor);

	void DrawDebugGroup() const;

	void TickSensing(float DeltaTime);
	void SetGroupState(EEnemyGroupState NewState);
	const APawn* GetSensedPlayerPawn() const;
	bool AreAllMembersNearAnchor() const;
	
	void RefreshKnownHostiles();
	
private:
	// =========================================================================
	// Enemy Group Member
	// =========================================================================
	
	// 멤버 명부. TObjectPtr UPROPERTY라 액터 파괴 시 자동 null —
	// 조회는 항상 IsValid 필터를 거친다.
	// メンバー名簿。Actor破壊時に自動でnullになるため、照会は常にIsValidを通す。
	UPROPERTY(VisibleAnywhere, Category = "Enemy Group")
	TArray<TObjectPtr<AEnemyCharacter>> Members;

	// 그룹 디버그 표시 (멤버 연결선). 켜면 Tick이 활성화된다.
	// グループのデバッグ表示（メンバー接続線）。有効時のみTickが動く。
	UPROPERTY(EditDefaultsOnly, Category = "Enemy Group|Debug")
	bool bDrawDebugGroup = false;
	
	// =========================================================================
	// Enemy Group State
	// =========================================================================

	// 경계 진입 거리. 플레이어가 이 안에 들어오면 Idle → Alert.
	// 警戒開始距離。プレイヤーがこの内側に入るとIdle→Alert。
	UPROPERTY(EditDefaultsOnly, Category = "Enemy Group|Sensing", meta = (ClampMin = "0.0", Units = "cm"))
	float AlertEnterDistance = 1700.f;

	// 경계 해제 거리. 진입보다 커야 함 (히스테리시스).
	// 警戒解除距離。開始より大きいこと（ヒステリシス）。
	UPROPERTY(EditDefaultsOnly, Category = "Enemy Group|Sensing", meta = (ClampMin = "0.0", Units = "cm"))
	float AlertExitDistance = 2200.f;

	// 교전 진입 거리. 플레이어가 이 안이면 Alert/Return → Engaged.
	// 交戦開始距離。この内側でAlert/Return→Engaged。
	UPROPERTY(EditDefaultsOnly, Category = "Enemy Group|Sensing", meta = (ClampMin = "0.0", Units = "cm"))
	float EngageDistance = 1000.f;

	// 추격 포기 거리 (스폰 앵커 기준). 플레이어가 홈에서 이보다 멀면 Engaged → Return.
	// "내가 홈에서 너무 벗어남" 판정은 적 이동 도입 시 추가.
	// 追跡諦め距離（スポーンアンカー基準）。「自分が離れすぎ」判定は移動導入時に追加。
	UPROPERTY(EditDefaultsOnly, Category = "Enemy Group|Sensing", meta = (ClampMin = "0.0", Units = "cm"))
	float ChaseGiveUpDistance = 2500.f;

	// 복귀 완료 반경. 생존 전원이 앵커의 이 안이면 Return → Idle.
	// 帰還完了半径。生存全員がこの内側でReturn→Idle。
	UPROPERTY(EditDefaultsOnly, Category = "Enemy Group|Sensing", meta = (ClampMin = "0.0", Units = "cm"))
	float ReturnHomeRadius = 700.f;
	
	// 感知周期（秒）。
	UPROPERTY(EditDefaultsOnly, Category = "Enemy Group|Sensing", meta = (ClampMin = "0.05", Units = "s"))
	float SensingInterval = 0.25f;

	// 현재 전황 상태. 변경은 SetGroupState 단일 경로만.
	// 現在の戦況状態。変更はSetGroupState経由のみ。
	EEnemyGroupState GroupState = EEnemyGroupState::Idle;

	// 다음 감지까지 남은 시간. 첫 감지를 랜덤 위상으로 시작해 그룹 간 동기화 방지.
	// 次感知までの残時間。初回をランダム位相にしグループ間の同期を防ぐ。
	float TimeUntilNextSense = 0.f;
	
	// 알려진 적 캐시. 약참조, 조회 시 유효성 필터. Engaged 이탈 시 비움.
	// 既知敵キャッシュ。非所有 — 弱参照。Engaged離脱時にクリア。
	TArray<TWeakObjectPtr<AActor>> KnownHostiles;
};