#include "Enemies/Formations/EnemySubFormationStrategy_Line.h"

bool UEnemySubFormationStrategy_Line::BuildLocalSlotsInternal(
	const FEnemySubFormationBuildContext& Context,
	TArray<FTransform>& OutLocalSlotTransforms,
	FString& OutError) const
{
	// [1] 런타임 데이터도 검증한다.
	// [1] Editor上のClampだけに依存せず、実行時の値も検証する。
	if (!FMath::IsFinite(SlotSpacing) || SlotSpacing <= 0.0f)
	{
		OutError = FString::Printf(
			TEXT(
				"%s: スロット間隔は0より大きい有限値である必要があります""（現在値: %.3f）。"),
			*GetClass()->GetName(),
			SlotSpacing);

		return false;
	}

	// [2] 홀수와 짝수 모두 진형 중심이 원점에 오도록 중앙 인덱스를 구한다.
	// [2] 奇数・偶数のどちらでも配置中心が原点になるよう、中央インデックスを算出する。
	const float CenterIndex = static_cast<float>(Context.MemberCount - 1) * 0.5f;

	// [3] 로컬 -Y에서 +Y 순서로 횡 1열 슬롯을 생성한다.
	// [3] ローカル-Yから+Yの順に横一列のスロットを生成する。
	for (int32 SlotIndex = 0; SlotIndex < Context.MemberCount; ++SlotIndex)
	{
		const float LocalY = (static_cast<float>(SlotIndex) - CenterIndex) * SlotSpacing;

		const FVector LocalLocation(0.0, LocalY, 0.0);

		OutLocalSlotTransforms.Add(
			FTransform(
				FQuat::Identity,
				LocalLocation,
				FVector::OneVector));
	}

	return true;
}