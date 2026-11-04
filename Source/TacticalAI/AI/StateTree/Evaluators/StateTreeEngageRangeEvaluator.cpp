#include "AI/StateTree/Evaluators/StateTreeEngageRangeEvaluator.h"

#include "AbilitySystemGlobals.h"
#include "GameFramework/Actor.h"
#include "StateTreeExecutionContext.h"
#include "AbilitySystem/TacticalAbilitySystemComponent.h"
#include "TacticalAI.h"

// =======================================================
// 트리 시작: 교전 거리 확정
//
// ツリー開始：交戦距離の確定。
// =======================================================
void FStateTreeEngageRangeEvaluator::TreeStart(FStateTreeExecutionContext& Context) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	// [1] 대체값으로 초기화한다. 이후 단계에서 실패해도 조건·Task는 이 값으로 동작한다.
	// [1] 代替値で初期化する。以降の段階で失敗しても、条件・Taskはこの値で動作する。
	InstanceData.EngageDistance = InstanceData.FallbackDistance;

	// [2] 시전자의 ASC를 조회한다. GA 구체 클래스는 알지 않는다.
	// [2] 術者のASCを照会する。GAの具体クラスは知らない。
	const AActor* Actor = InstanceData.Actor.Get();
	const UTacticalAbilitySystemComponent* AbilitySystem = IsValid(Actor)
		? Cast<UTacticalAbilitySystemComponent>(UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Actor))
		: nullptr;

	// [3] 교전 거리를 질의한다.
	// [3] 交戦距離を照会する。
	float Distance = 0.f;
	if (AbilitySystem && AbilitySystem->TryGetAbilityEngageDistance(InstanceData.AbilityTag, Distance))
	{
		InstanceData.EngageDistance = Distance;
		UE_LOG(LogTacticalAI, Verbose, TEXT("交戦距離を確定しました。Actor=%s, Ability=%s, Distance=%.1f"),
			*GetNameSafe(Actor), *InstanceData.AbilityTag.ToString(), Distance);
		return;
	}

	UE_LOG(LogTacticalAI, Warning, TEXT("交戦距離を算出できないため代替値を使用します。Actor=%s, Ability=%s, Fallback=%.1f"),
		*GetNameSafe(Actor), *InstanceData.AbilityTag.ToString(), InstanceData.FallbackDistance);
}