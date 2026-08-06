#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "TargetScorePolicy.generated.h"

// =========================================================================
// Targeting Context
//
// 정책 평가에 쓰는 사실 스냅샷. 진영 중립 — 채우는 쪽(셀렉터 자식)이 측별로 다르다.
//
// ポリシー評価用の事実スナップショット。陣営中立 — 充填側(セレクタ子クラス)が陣営ごとに異なる。
// =========================================================================
USTRUCT(BlueprintType)
struct TACTICALAI_API FTargetingContext
{
	GENERATED_BODY()

	// 평가 주체(셀렉터 소유 캐릭터)의 현재 위치.
	// 評価主体の現在位置。
	UPROPERTY(BlueprintReadOnly, Category = "Targeting")
	FVector SelfLocation = FVector::ZeroVector;

	// 리더 위치. 적 측 컨텍스트에는 존재하지 않을 수 있다 (bHasLeader=false).
	// 리더 의존 정책은 이때 0점을 반환 — 입력 부재 = 기여 없음.
	// リーダー位置。敵側コンテキストでは存在しない場合がある。
	// リーダー依存ポリシーはその際0点を返す（入力不在＝寄与なし）。
	UPROPERTY(BlueprintReadOnly, Category = "Targeting")
	FVector LeaderLocation = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Targeting")
	bool bHasLeader = false;
	
	// 리더가 현재 잡은 타겟. 자신이 리더인 컨텍스트에서는 항상 비어 있음.
	// リーダーの現在ターゲット。自分がリーダーの場合は常に空（自己参照ループ遮断）。
	UPROPERTY(BlueprintReadOnly, Category = "Targeting")
	TObjectPtr<const AActor> LeaderTarget = nullptr;
	
	// 自分以外のパーティメンバーの現在ターゲット（リーダー含む）。
	UPROPERTY(BlueprintReadOnly, Category = "Targeting")
	TArray<TObjectPtr<const AActor>> AllyTargets;
};

// =========================================================================
// Target Score Policy (추상)
//
// "후보 하나를 0~1로 점수화"하는 계약.
// 캐릭터는 여러 정책을 가중치로 조합한다.
//
// ⚠ 무상태 필수: 정책은 캐릭터 BP에서 직접 편집하는 설정값만 가진다 
//   (공유 DataAsset로 재사용 가능 설계 계획 없음 — 정책 조합은 캐릭터 개성이라 재사용 축이 아님).
//   런타임 상태(직전 타겟·타이머 등)는 금지 — Instanced 객체는 BP 템플릿에서
//   사본이 만들어지므로, 상태 필드는 템플릿에 남은 값이 전 스폰에 복제된다.
//
// ステートレス必須。ポリシーはBPで直接編集する設定値のみ（共有DataAsset化はしない）。
// =========================================================================
UCLASS(Abstract, Blueprintable, EditInlineNew, DefaultToInstanced, CollapseCategories)
class TACTICALAI_API UTargetScorePolicy : public UObject
{
	GENERATED_BODY()

public:
	// 후보 점수. 반환 범위 계약은 0~1 (가중치 곱이 정책 간 비교 가능하도록).
	// BlueprintNativeEvent — C++ 리빌드 없이 BP 정책 추가 가능.
	// 候補の採点。返却範囲は0〜1の契約。BPでの新ポリシー追加が可能。
	UFUNCTION(BlueprintNativeEvent, Category = "Targeting")
	float ScoreTarget(const FTargetingContext& Context, const AActor* Candidate) const;
	virtual float ScoreTarget_Implementation(const FTargetingContext& Context, const AActor* Candidate) const
	{
		return 0.f;
	}
	
	// 셀렉터의 결정 귀속([3])이 읽는다. 값 자체는 protected — 편집은 에디터, 로직은 읽기만.
	float GetHoldDuration() const { return HoldDuration; }

protected:
	// 이 정책이 결정을 주도했을 때(승리 후보에 대한 가중 기여 최대) 타겟을 유지할 시간(초).
	// 기본값은 각 정책 클래스가 자기 결정의 성격에 맞게 생성자에서 선언.
	// 0 = 유지 없음. Instanced이므로 캐릭터 BP에서 개별 오버라이드 가능.
	// このポリシーが決定を主導した際のターゲット維持時間(秒)。既定値は各ポリシークラスが
	// 自分の決定の性格に合わせてコンストラクタで宣言。キャラBPで個別上書き可能。
	UPROPERTY(EditAnywhere, Category = "Targeting", meta = (ClampMin = "0.0", Units = "s"))
	float HoldDuration = 1.f;
};

// =========================================================================
// Weighted Target Policy
//
// 정책 + 가중치 한 쌍. 셀렉터가 배열로 들고 합산한다.
// ポリシー＋重みの組。セレクタが配列で保持し合算。
// =========================================================================
USTRUCT(BlueprintType)
struct TACTICALAI_API FWeightedTargetPolicy
{
	GENERATED_BODY()

	// 점수 계산 정책. 컴포넌트에 Instanced로 배치 → 캐릭터별 독립 설정.
	// 採点ポリシー。コンポーネントにInstanced配置 → キャラ別独立設定。
	UPROPERTY(EditAnywhere, Instanced, Category = "Targeting")
	TObjectPtr<UTargetScorePolicy> Policy = nullptr;

	// 이 정책의 가중치. 캐릭터 개성은 정책 조합 × 가중치로 표현.
	// このポリシーの重み。キャラの個性＝ポリシー構成×重み。
	UPROPERTY(EditAnywhere, Category = "Targeting", meta = (ClampMin = "0.0"))
	float Weight = 1.f;
};