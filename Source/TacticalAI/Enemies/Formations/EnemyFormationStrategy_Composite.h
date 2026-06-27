#pragma once

#include "CoreMinimal.h"
#include "Enemies/Formations/EnemyFormationStrategy.h"
#include "EnemyFormationStrategy_Composite.generated.h"

class UEnemySubFormationStrategy;

// =========================================================================
// 복합 진형을 구성하는 하나의 입력 항목.
//
// 배열 인덱스가 입력 버킷 인덱스와 직접 대응한다.
// 예를 들어 첫 번째 항목은 Bucket 0,
// 두 번째 항목은 Bucket 1의 몬스터를 배치한다.
//
// 각 항목은 하위 진형의 모양과
// 전체 진형 루트 기준 상대 위치·회전을 정의한다.
//
// 複合フォーメーションを構成する1つの入力項目。
//
// 配列インデックスは入力バケットのインデックスと直接対応する。
// 各項目はサブフォーメーションの形状と、
// フォーメーションルート基準の相対位置・回転を定義する。
// =========================================================================
USTRUCT(BlueprintType)
struct TACTICALAI_API FEnemyCompositeFormationEntry
{
	GENERATED_BODY()

	// 에디터와 디버그에서 항목을 식별하기 위한 이름.
	// 진형 로직에서는 이 값을 조건으로 사용하지 않는다.
	//
	// Editorおよびデバッグ上で項目を識別するための名前。
	// フォーメーション処理の条件分岐には使用しない。
	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		Category = "敵フォーメーション|複合",
		meta = (DisplayName = "入力名"))
	FName EntryName = NAME_None;

	// 이 입력 버킷의 로컬 형상을 생성할 하위 진형 전략.
	//
	// この入力バケットのローカル形状を生成する
	// サブフォーメーション戦略。
	UPROPERTY(
		EditAnywhere,
		Instanced,
		BlueprintReadOnly,
		Category = "敵フォーメーション|複合",
		meta = (DisplayName = "サブフォーメーション戦略"))
	TObjectPtr<UEnemySubFormationStrategy> SubFormationStrategy = nullptr;

	// 전체 진형 루트에서 이 하위 진형 원점까지의 상대 위치.
	// 로컬 +X는 전방, 로컬 Y는 좌우 방향으로 해석한다.
	//
	// フォーメーションルートから
	// このサブフォーメーション原点までの相対位置。
	// ローカル+Xを前方、ローカルYを左右方向として扱う。
	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		Category = "敵フォーメーション|複合",
		meta = (DisplayName = "相対位置"))
	FVector RelativeLocation = FVector::ZeroVector;

	// 전체 진형 루트 기준의 상대 회전.
	// 하위 진형 자체의 슬롯 회전에 추가로 적용한다.
	//
	// フォーメーションルート基準の相対回転。
	// サブフォーメーション内のスロット回転に追加適用する。
	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		Category = "敵フォーメーション|複合",
		meta = (DisplayName = "相対回転"))
	FRotator RelativeRotation = FRotator::ZeroRotator;
};

// =========================================================================
// 여러 하위 진형을 조합해 하나의 적 진형 레이아웃을 만드는 전략.
//
// SubFormationEntries의 배열 순서가 입력 버킷 순서와 대응한다.
// 개별 입력 버킷은 비어 있어도 되지만,
// 모든 버킷이 비어 있는 경우는 기반 클래스에서 거부한다.
//
// 이 전략은 각 하위 진형의 상대 위치와 회전을 조율하지만,
// 타겟·진형 기준점·실제 몬스터 배정은 처리하지 않는다.
//
// 複数のサブフォーメーションを組み合わせ、
// 1つの敵フォーメーションレイアウトを生成する戦略。
//
// SubFormationEntriesの配列順序は入力バケット順序と対応する。
// 個別の入力バケットは空でもよいが、
// すべてのバケットが空の場合は基底クラスで拒否する。
//
// 各サブフォーメーションの相対位置と回転は調整するが、
// ターゲット・基準点・実際の敵割り当ては扱わない。
// =========================================================================
UCLASS(
	EditInlineNew,
	Blueprintable,
	BlueprintType,
	meta = (DisplayName = "敵フォーメーション - 複合"))
class TACTICALAI_API UEnemyFormationStrategy_Composite : public UEnemyFormationStrategy
{
	GENERATED_BODY()

protected:
	// =========================================================================
	// 복합 진형을 구성하는 입력 항목 배열.
	//
	// 배열 인덱스는 입력 버킷 인덱스와 직접 대응한다.
	// 입력 버킷 수는 이 배열 수보다 작거나 같아야 한다.
	//
	// 입력되지 않은 뒤쪽 항목은 이번 레이아웃 계산에서 사용하지 않는다.
	// 예를 들어 Entry가 [전열, 후열] 2개이고 입력이 [3]이면,
	// 전열만 생성하고 후열은 결과 Layout에 포함하지 않는다.
	//
	// 複合フォーメーションを構成する入力項目配列。
	//
	// 配列インデックスは入力バケットと直接対応する。
	// 入力バケット数は、この配列数以下でなければならない。
	//
	// 入力されていない後続項目は、今回のレイアウト計算では使用しない。
	// 例えばEntryが[前列, 後列]の2つで入力が[3]の場合、
	// 前列だけを生成し、後列は結果Layoutに含めない。
	// =========================================================================
	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		Category = "敵フォーメーション|複合",
		meta = (
			DisplayName = "サブフォーメーション構成",
			TitleProperty = "EntryName"))
	TArray<FEnemyCompositeFormationEntry> SubFormationEntries;

	// =========================================================================
	// 각 입력 버킷을 대응하는 하위 진형에 전달하고,
	// 결과를 전체 진형 루트 기준 좌표로 변환한다.
	//
	// 各入力バケットを対応するサブフォーメーションへ渡し、
	// 結果をフォーメーションルート基準の座標へ変換する。
	// =========================================================================
	virtual bool BuildLayoutInternal(
		const FEnemyFormationLayoutContext& Context,
		FEnemyFormationLayout& OutLayout,
		FString& OutError) const override;
};