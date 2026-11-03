#include "AI/StateTree/Conditions/StateTreeHomeSlotCondition.h"

#include "GameFramework/Actor.h"
#include "StateTreeExecutionContext.h"
#include "AI/Interfaces/HomeSlotProvider.h"

// =======================================================
// 홈 슬롯 조건 판정
// 전이 조건으로 매 틱 평가될 수 있으므로
// 인터페이스 조회와 수평 거리 계산만 수행한다.
//
// ホームスロット条件の判定。
// 遷移条件として毎ティック評価され得るため、
// インターフェース照会と水平距離計算のみ行う。
// =======================================================
bool FStateTreeHomeSlotCondition::TestCondition(FStateTreeExecutionContext& Context) const
{
	const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	// [1] 판정 대상과 홈 슬롯 공급 계약을 확인한다.
	// [1] 判定対象とホームスロット供給契約を確認する。
	const AActor* Actor = InstanceData.Actor.Get();
	if (!IsValid(Actor)) return false;

	const IHomeSlotProvider* HomeProvider = Cast<IHomeSlotProvider>(Actor);
	if (!HomeProvider) return false;

	// [2] 유무만 묻는 방식은 조회 결과로 바로 판정한다.
	// [2] 有無のみを問う方式は照会結果で直接判定する。
	FVector HomeSlot = FVector::ZeroVector;
	const bool bHasHomeSlot = HomeProvider->TryGetHomeSlot(HomeSlot);

	switch (InstanceData.Check)
	{
	case EHomeSlotCheck::Exists:
		return bHasHomeSlot;

	case EHomeSlotCheck::Missing:
		return !bHasHomeSlot;

	default:
		break;
	}

	// [3] 거리 방식. 홈 슬롯은 NavMesh 투영 좌표, Actor는 캡슐 중심이므로 높이 차이를 제외한다.
	// [3] 距離方式。ホームスロットはNavMesh投影座標、Actorはカプセル中心のため高低差を除外する。
	if (!bHasHomeSlot) return false;

	const double DistanceSquared2D = FVector::DistSquared2D(Actor->GetActorLocation(), HomeSlot);
	const double ThresholdSquared = FMath::Square(static_cast<double>(InstanceData.Distance));

	return InstanceData.Check == EHomeSlotCheck::Within
		? DistanceSquared2D <= ThresholdSquared
		: DistanceSquared2D > ThresholdSquared;
}