#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Info.h"
#include "EnemyGroup.generated.h"

class AEnemyCharacter;
class AEnemyGroup;

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

protected:
	virtual void BeginPlay() override;

private:
	// 멤버 파괴 = 사망으로 취급 (HP 시스템 도입 전의 사실).
	// HP가 생기면 사망 이벤트로 내부만 교체 — 외부 API는 유지.
	// メンバー破壊＝死亡扱い（HP導入前の暫定）。導入後は内部だけ差し替え。
	UFUNCTION()
	void HandleMemberDestroyed(AActor* DestroyedActor);

	void DrawDebugGroup() const;

private:
	// 멤버 명부. TObjectPtr UPROPERTY라 액터 파괴 시 자동 null —
	// 조회는 항상 IsValid 필터를 거친다.
	// メンバー名簿。Actor破壊時に自動でnullになるため、照会は常にIsValidを通す。
	UPROPERTY(VisibleAnywhere, Category = "Enemy Group")
	TArray<TObjectPtr<AEnemyCharacter>> Members;

	// 그룹 디버그 표시 (멤버 연결선). 켜면 Tick이 활성화된다.
	// グループのデバッグ表示（メンバー接続線）。有効時のみTickが動く。
	UPROPERTY(EditDefaultsOnly, Category = "Enemy Group|Debug")
	bool bDrawDebugGroup = false;
};