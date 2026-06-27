#pragma once

#include "CoreMinimal.h"
#include "Characters/EnemyCharacter.h"
#include "EnemyFormationTypes.generated.h"

class AEnemyCharacter;
class UEnemySubFormationStrategy;

// =========================================================================
// Enemy Formation Mode
//
// 적의 초기 배치 모드.
// 敵の初期配置モード。
// =========================================================================
UENUM(BlueprintType)
enum class EEnemyFormationMode : uint8
{
	ByEnemyClass UMETA(DisplayName = "By Enemy Class"),
	CompositeFormation UMETA(DisplayName = "Composite Formation")
};

// =========================================================================
// Enemy Formation Slot
//
// 적 1마리가 배치될 월드 공간 슬롯.
// 敵1体が配置されるワールド空間上のスロット。
// =========================================================================
USTRUCT(BlueprintType)
struct TACTICALAI_API FEnemyFormationSlot
{
	GENERATED_BODY()

public:
	FEnemyFormationSlot() = default;

	FEnemyFormationSlot(const FTransform& InWorldTransform, const int32 InSlotIndex)
		: WorldTransform(InWorldTransform)
		, SlotIndex(InSlotIndex)
	{
	}

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Enemy AI|Formation")
	FTransform WorldTransform = FTransform::Identity;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Enemy AI|Formation")
	int32 SlotIndex = INDEX_NONE;
};

// =========================================================================
// Enemy Spawn Entry
//
// 스폰할 적 클래스와 수량.
// 生成する敵クラスと数。
// =========================================================================
USTRUCT(BlueprintType)
struct TACTICALAI_API FEnemySpawnEntry
{
	GENERATED_BODY()

public:
	int32 GetSpawnCount() const
	{
		return EnemyClass ? FMath::Max(0, Count) : 0;
	}

	bool HasInvalidClassWithCount() const
	{
		return EnemyClass == nullptr && Count > 0;
	}

	// 스폰할 적 클래스.
	// 生成する敵クラス。
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy AI|Spawn",
		meta = (DisplayName = "Enemy Class"))
	TSubclassOf<AEnemyCharacter> EnemyClass = nullptr;

	// 스폰 수량.
	// 生成数。
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy AI|Spawn",
		meta = (ClampMin = "0", DisplayName = "Count"))
	int32 Count = 1;
};

// =========================================================================
// Enemy SubFormation
//
// Spawner 기준으로 배치되는 하위 Formation.
// 적 목록, 로컬 기준 위치, 슬롯 생성 전략을 가진다.
//
// Spawner基準で配置される下位Formation。
// 敵リスト、ローカル基準位置、スロット生成戦略を持つ。
// =========================================================================
USTRUCT(BlueprintType)
struct TACTICALAI_API FEnemySubFormation
{
	GENERATED_BODY()

public:
	int32 GetTotalSpawnCount() const
	{
		int32 TotalCount = 0;

		for (const FEnemySpawnEntry& Entry : SpawnEntries)
		{
			TotalCount += Entry.GetSpawnCount();
		}

		return TotalCount;
	}

	FTransform MakeWorldTransform(const FTransform& SpawnerWorldTransform) const
	{
		const FTransform LocalFormationTransform(
			LocalRotation.Quaternion(),
			LocalOffset,
			FVector::OneVector);

		return LocalFormationTransform * SpawnerWorldTransform;
	}

	// 에디터에서 식별하기 위한 이름.
	// エディタ上で識別するための名前。
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy AI|SubFormation",
		meta = (DisplayName = "SubFormation Name"))
	FName SubFormationName = TEXT("SubFormation");

	// Spawner 기준 로컬 위치. 앞쪽은 X+, 오른쪽은 Y+.
	// Spawner基準のローカル位置。前方はX+、右方向はY+。
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy AI|SubFormation",
		meta = (DisplayName = "Local Offset"))
	FVector LocalOffset = FVector::ZeroVector;

	// SubFormation 기준 로컬 회전.
	// SubFormation基準のローカル回転。
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy AI|SubFormation",
		meta = (DisplayName = "Local Rotation"))
	FRotator LocalRotation = FRotator::ZeroRotator;

	// 이 SubFormation에 배치할 적 목록.
	// このSubFormationに割り当てる敵リスト。
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy AI|SubFormation",
		meta = (DisplayName = "Spawn Entries"))
	TArray<FEnemySpawnEntry> SpawnEntries;

	// 이 SubFormation의 슬롯 생성 방식.
	// このSubFormationのスロット生成方式。
	UPROPERTY(EditAnywhere, Instanced, BlueprintReadOnly, Category = "Enemy AI|SubFormation",
		meta = (DisplayName = "SubFormation"))
	TObjectPtr<UEnemySubFormationStrategy> SubFormationStrategy = nullptr;
};

// =========================================================================
// Enemy Composite Formation
//
// Spawner 안에서 직접 편집하는 SubFormation 조합.
// Spawner内で直接編集するSubFormationの組み合わせ。
// =========================================================================
USTRUCT(BlueprintType)
struct TACTICALAI_API FEnemyCompositeFormation
{
	GENERATED_BODY()

public:
	bool HasAnySpawnEntries() const
	{
		for (const FEnemySubFormation& SubFormation : SubFormations)
		{
			if (SubFormation.GetTotalSpawnCount() > 0)
			{
				return true;
			}
		}

		return false;
	}

	// 이 Spawner에서 조합할 SubFormation 목록.
	// このSpawnerで組み合わせるSubFormation一覧。
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy AI|Composite Formation",
		meta = (DisplayName = "SubFormations"))
	TArray<FEnemySubFormation> SubFormations;
};