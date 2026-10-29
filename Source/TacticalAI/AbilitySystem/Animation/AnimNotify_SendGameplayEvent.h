#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "GameplayTagContainer.h"
#include "AnimNotify_SendGameplayEvent.generated.h"

// =======================================================
// Send Gameplay Event Anim Notify
//
// 애니메이션의 지정 시점에 Owner 액터의 ASC로 Gameplay Event를 송신한다.
// 이벤트에 따른 처리는 Gameplay Ability가 담당한다.
//
// 타임라인에 배치한 각 Notify의 인스턴스는 애니메이션 에셋 내부에 저장된다.
// 같은 에셋을 재생하는 캐릭터들이 해당 인스턴스를 공유하므로,
// 캐릭터별 런타임 상태를 멤버 변수에 저장하지 않는다.
//
// アニメーションの指定タイミングで、MeshCompの所有アクターへGameplay Eventを送信する。
// イベントに応じた処理はGameplay Abilityが担当する。
//
// タイムラインに配置した各Notifyのインスタンスは、アニメーションアセット内に保存される。
// 同じアセットを再生するキャラクター間でそのインスタンスを共有するため、
// キャラクターごとのランタイム状態はメンバー変数に保持しない。
// =======================================================
UCLASS(meta = (DisplayName = "Send Gameplay Event"))
class TACTICALAI_API UAnimNotify_SendGameplayEvent : public UAnimNotify
{
	GENERATED_BODY()

public:
	virtual void Notify(
		USkeletalMeshComponent* MeshComp,
		UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) override;

	// 애니메이션 타임라인의 Notify 이름에 송신할 이벤트 태그를 표시한다.
	// アニメーションタイムラインのNotify名に、送信するイベントタグを表示する。
	virtual FString GetNotifyName_Implementation() const override;

protected:
	// Notify 실행 시 송신할 이벤트 태그. 에디터에서는 Event 계층의 태그만 선택할 수 있다.
	// 태그가 설정되지 않으면 이벤트를 송신하지 않는다.
	// Notify実行時に送信するイベントタグ。エディタではEvent階層のタグのみ選択できる。
	// タグが未設定の場合、イベントは送信しない。
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gameplay Event", meta = (Categories = "Event"))
	FGameplayTag EventTag;
};