#pragma once

#include "CoreMinimal.h"
#include "StateTreeConditionBase.h"
#include "StateTreeHomeSlotCondition.generated.h"

class AActor;

// =======================================================
// 홈 슬롯 판정 방식
// 홈 슬롯이 없을 때의 결과를 방식마다 고정하기 위해,
// 반전 옵션 대신 유무 판정을 별도 값으로 둔다.
//
// ホームスロットの判定方式。
// ホームスロットがない場合の結果を方式ごとに固定するため、
// 反転オプションではなく有無の判定を別の値として持つ。
// =======================================================
UENUM()
enum class EHomeSlotCheck : uint8
{
	// 홈 슬롯이 있다.
	// ホームスロットがある。
	Exists,

	// 홈 슬롯이 없다.
	// ホームスロットがない。
	Missing,

	// 홈 슬롯까지의 수평 거리가 Distance 이하. 홈 슬롯이 없으면 false.
	// ホームスロットまでの水平距離がDistance以下。ホームスロットがなければfalse。
	Within,

	// 홈 슬롯까지의 수평 거리가 Distance 초과. 홈 슬롯이 없으면 false.
	// ホームスロットまでの水平距離がDistance超過。ホームスロットがなければfalse。
	Beyond
};

// =======================================================
// 홈 슬롯 조건의 실행별 데이터
// Context와 바인딩 가능한 값은 노드가 아니라 여기에 둔다.
//
// ホームスロット条件の実行ごとのデータ。
// Contextとバインド可能な値はノードではなくここに置く。
// =======================================================
USTRUCT()
struct TACTICALAI_API FHomeSlotConditionInstanceData
{
	GENERATED_BODY()

	// 판정할 캐릭터. StateTree Context의 Actor와 연결한다.
	// 判定するキャラクター。StateTree ContextのActorと接続する。
	UPROPERTY(EditAnywhere, Category = Context)
	TObjectPtr<AActor> Actor = nullptr;

	// 판정 방식. 상태마다 에셋에서 선택한다.
	// Distance 표시 여부를 결정하므로 같은 구조체에 둔다.
	// 判定方式。状態ごとにアセットで選択する。
	// Distanceの表示有無を決めるため同じ構造体に置く。
	UPROPERTY(EditAnywhere, Category = Parameter)
	EHomeSlotCheck Check = EHomeSlotCheck::Within;

	// 판정 기준 수평 거리(cm). Within / Beyond에서만 사용한다.
	// 여러 상태에서 같은 기준을 쓸 때는 StateTree Parameter에 바인딩한다.
	// 判定基準の水平距離(cm)。Within / Beyondでのみ使用する。
	// 複数の状態で同じ基準を使う場合はStateTree Parameterにバインドする。
	UPROPERTY(EditAnywhere, Category = Parameter,
		meta = (ClampMin = "0.0", Units = "cm",
			EditCondition = "Check == EHomeSlotCheck::Within || Check == EHomeSlotCheck::Beyond",
			EditConditionHides))
	float Distance = 100.f;
};

// =======================================================
// 홈 슬롯 조건
// 캐릭터와 홈 슬롯의 관계만 판정한다.
// 홈 슬롯의 의미(전투 모드·커밋 여부)는 IHomeSlotProvider 구현부가 결정한다.
// Context가 유효하지 않거나 계약을 구현하지 않은 Actor이면 모든 방식에서 false.
//
// ホームスロット条件。
// キャラクターとホームスロットの関係のみ判定する。
// ホームスロットの意味（戦闘モード・コミット有無）はIHomeSlotProvider実装側が決定する。
// Contextが無効、または契約未実装のActorの場合はすべての方式でfalse。
// =======================================================
USTRUCT(meta = (DisplayName = "Home Slot", Category = "Tactical AI|Combat"))
struct TACTICALAI_API FStateTreeHomeSlotCondition : public FStateTreeConditionCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FHomeSlotConditionInstanceData;

	virtual const UStruct* GetInstanceDataType() const override
	{
		return FInstanceDataType::StaticStruct();
	}

	virtual bool TestCondition(FStateTreeExecutionContext& Context) const override;
};