#include "AI/StateTree/Conditions/StateTreeTargetRangeCondition.h"

#include "GameFramework/Actor.h"
#include "StateTreeExecutionContext.h"
#include "AI/Targeting/TargetSelectorComponent.h"

// =======================================================
// 타겟 거리 조건 판정
// 진입 조건(상태 선택 시점)에서의 사용을 전제로 한다.
// 매 틱 전이 조건으로 쓰게 되면 셀렉터 조회를 Evaluator로 옮긴다.
//
// ターゲット距離条件の判定。
// 開始条件（状態選択時）での使用を前提とする。
// 毎ティックの遷移条件として使う場合は、セレクタ照会をEvaluatorへ移す。
// =======================================================
bool FStateTreeTargetRangeCondition::TestCondition(FStateTreeExecutionContext& Context) const
{
	const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	// [1] 판정 대상과 현재 타겟을 확인한다. 셀렉터는 추상 베이스로만 조회해 진영에 의존하지 않는다.
	// [1] 判定対象と現在のターゲットを確認する。セレクタは抽象基底のみで照会し、陣営に依存しない。
	const AActor* Actor = InstanceData.Actor.Get();
	if (!IsValid(Actor)) return false;

	const UTargetSelectorComponent* Selector = Actor->FindComponentByClass<UTargetSelectorComponent>();
	if (!Selector) return false;

	const AActor* Target = Selector->GetCurrentTarget();
	if (!IsValid(Target)) return false;

	// [2] 캡슐 표면 사이의 수평 거리로 판정한다. 캐릭터 크기 차이가 기준값에 섞이지 않도록 양쪽 반경을 뺀다.
	// [2] カプセル表面間の水平距離で判定する。体格差が基準値に混ざらないよう両者の半径を引く。
	const double CenterDistance2D =
		FVector::Dist2D(Actor->GetActorLocation(), Target->GetActorLocation());

	const double SurfaceDistance2D = CenterDistance2D
		- Actor->GetSimpleCollisionRadius()
		- Target->GetSimpleCollisionRadius();

	return Check == ETargetRangeCheck::Within
		? SurfaceDistance2D <= InstanceData.Distance
		: SurfaceDistance2D > InstanceData.Distance;
}