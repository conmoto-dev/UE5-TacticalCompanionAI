#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Enemies/Formations/EnemyFormationStrategy.h"
#include "EnemyFormationProfile.generated.h"

// =========================================================================
// 재사용 가능한 적 진형 설정 자산.
//
// 하나의 완성된 공간 배치 정책을 이름 있는 자산으로 보관한다.
// 몬스터의 기본 진형과 Encounter의 Override 진형은
// 모두 이 Profile 타입을 참조한다.
//
// Profile 내부의 Strategy는 설정 데이터로만 사용하며,
// 멤버 목록·현재 타겟·커밋된 슬롯 같은 런타임 상태를 저장하지 않는다.
//
// 再利用可能な敵フォーメーション設定アセット。
//
// 1つの完成した空間配置方針を、名前付きアセットとして保持する。
// 敵のデフォルトフォーメーションとEncounterのOverrideは、
// どちらもこのProfile型を参照する。
//
// 内部Strategyは設定データとしてのみ使用し、
// メンバー一覧・現在ターゲット・確定スロットなどの
// ランタイム状態は保持しない。
// =========================================================================
UCLASS(BlueprintType, meta = (DisplayName = "敵フォーメーションプロファイル"))
class TACTICALAI_API UEnemyFormationProfile : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	// =========================================================================
	// Profile에 설정된 Strategy로 로컬 레이아웃을 생성한다.
	//
	// Profile은 Strategy 선택과 소유 경계만 제공하며,
	// 실제 입력·출력 검증과 좌표 계산은 Strategy에 위임한다.
	//
	// Profileに設定されたStrategyを使用して、
	// ローカルレイアウトを生成する。
	//
	// ProfileはStrategyの選択と所有境界だけを提供し、
	// 実際の入出力検証と座標計算はStrategyへ委譲する。
	// =========================================================================
	bool TryBuildLayout(
		const FEnemyFormationLayoutContext& Context,
		FEnemyFormationLayout& OutLayout,
		FString* OutError = nullptr) const;

protected:
	// =========================================================================
	// 이 Profile이 사용할 공간 레이아웃 전략.
	//
	// Profile마다 독립된 인라인 객체로 저장되므로,
	// 같은 Composite 클래스라도 서로 다른 하위 진형과 간격을 설정할 수 있다.
	//
	// 런타임에서 이 객체의 값을 변경하지 않는다.
	// 여러 Coordinator가 같은 Profile을 참조할 수 있기 때문이다.
	//
	// このProfileが使用する空間レイアウト戦略。
	//
	// Profileごとに独立したインラインオブジェクトとして保存されるため、
	// 同じCompositeクラスでも異なるサブフォーメーションや
	// 間隔を設定できる。
	//
	// 複数のCoordinatorから共有される可能性があるため、
	// ランタイム中にこのオブジェクトの値を変更してはならない。
	// =========================================================================
	UPROPERTY(
		EditDefaultsOnly,
		Instanced,
		BlueprintReadOnly,
		Category = "敵フォーメーション",
		meta = (DisplayName = "レイアウト戦略"))
		TObjectPtr<UEnemyFormationStrategy> LayoutStrategy = nullptr;
};