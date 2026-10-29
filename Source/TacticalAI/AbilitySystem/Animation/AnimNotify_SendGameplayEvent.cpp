#include "AbilitySystem/Animation/AnimNotify_SendGameplayEvent.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemGlobals.h"
#include "Components/SkeletalMeshComponent.h"

// =======================================================
// Gameplay Event 송신
// Owner 액터에 ASC가 있을 때만 송신한다.
//
// Gameplay Eventの送信。
// OwnerアクターにASCがある場合のみ送信する。
// =======================================================
void UAnimNotify_SendGameplayEvent::Notify(
	USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	// [1] 태그가 설정되지 않은 Notify는 송신하지 않는다.
	// [1] タグ未設定のNotifyは送信しない。
	if (!EventTag.IsValid()) return;

	AActor* OwnerActor = MeshComp ? MeshComp->GetOwner() : nullptr;
	if (!OwnerActor) return;

	// [2] ASC가 없는 액터는 건너뛴다.
	// [2] ASCを持たないアクターはスキップする。
	if (!UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(OwnerActor)) return;

	// [3] 이벤트 태그와 송신 주체를 Payload에 담아 ASC로 전달한다.
	// [3] イベントタグと送信元をPayloadに設定し、ASCへ伝達する。
	FGameplayEventData Payload;
	Payload.EventTag = EventTag;
	Payload.Instigator = OwnerActor;

	UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(OwnerActor, EventTag, Payload);
}

// =======================================================
// Notify 표시 이름
// 태그가 설정되어 있으면 태그 이름을 표시하고, 미설정이면 기본 이름을 사용한다.
//
// Notifyの表示名
// タグが設定されている場合はタグ名を表示し、未設定の場合は既定の名前を使用する。
// =======================================================
FString UAnimNotify_SendGameplayEvent::GetNotifyName_Implementation() const
{
	return EventTag.IsValid() ? EventTag.ToString() : Super::GetNotifyName_Implementation();
}