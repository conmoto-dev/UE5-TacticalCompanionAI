#include "Enemies/Spawning/EnemySpawner.h"
#include "Components/SceneComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"

#if WITH_EDITORONLY_DATA
#include "Components/ArrowComponent.h"
#endif

DEFINE_LOG_CATEGORY_STATIC(LogEnemySpawner, Log, All);

// =========================================================================
// AEnemySpawner
// =========================================================================
AEnemySpawner::AEnemySpawner()
{
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

#if WITH_EDITORONLY_DATA
	EditorArrow = CreateEditorOnlyDefaultSubobject<UArrowComponent>(TEXT("EditorArrow"));

	if (EditorArrow)
	{
		EditorArrow->SetupAttachment(SceneRoot);
		EditorArrow->SetHiddenInGame(true);
		EditorArrow->ArrowSize = 2.0f;
		EditorArrow->ArrowLength = 180.0f;
		EditorArrow->ArrowColor = FColor::Red;
	}
#endif
}

void AEnemySpawner::BeginPlay()
{
	Super::BeginPlay();

	if (bSpawnOnBeginPlay)
	{
		SpawnEnemies();
	}
}

void AEnemySpawner::SpawnEnemies()
{
	if (!GetWorld())
	{
		return;
	}

	if (bDestroyPreviousSpawnedEnemies)
	{
		ClearSpawnedEnemies();
	}

	switch (FormationMode)
	{
	case EEnemyFormationMode::ByEnemyClass:
		SpawnByEnemyClass();
		break;

	case EEnemyFormationMode::CompositeFormation:
		SpawnByCompositeFormation();
		break;

	default:
		break;
	}
}

void AEnemySpawner::ClearSpawnedEnemies()
{
	for (TObjectPtr<AEnemyCharacter>& SpawnedEnemy : SpawnedEnemies)
	{
		if (IsValid(SpawnedEnemy))
		{
			SpawnedEnemy->Destroy();
		}
	}

	SpawnedEnemies.Reset();
}

void AEnemySpawner::SpawnByEnemyClass()
{
	const int32 TotalSpawnCount = GetTotalSpawnCount(SpawnEntries);

	if (TotalSpawnCount <= 0)
	{
		UE_LOG(LogEnemySpawner, Warning, TEXT("ByEnemyClass has no valid spawn entries: %s"), *GetName());
		return;
	}

	// [1] 敵同士が重ならないように仮スロットを作る。
	const TArray<FEnemyFormationSlot> Slots = BuildFallbackSlots(TotalSpawnCount);

	// [2] 入力順に敵を仮スロットへ配置する。
	SpawnEntryListAtSlots(SpawnEntries, Slots, TEXT("ByEnemyClass"));
}

void AEnemySpawner::SpawnByCompositeFormation()
{
	if (!CompositeFormation.HasAnySpawnEntries())
	{
		UE_LOG(LogEnemySpawner, Warning, TEXT("CompositeFormation has no valid spawn entries: %s"), *GetName());
		return;
	}

	const FTransform SpawnerWorldTransform = GetActorTransform();

	for (const FEnemySubFormation& SubFormation : CompositeFormation.SubFormations)
	{
		const int32 SubFormationSpawnCount = SubFormation.GetTotalSpawnCount();

		// [1] 空のSubFormationは無視する。
		if (SubFormationSpawnCount <= 0)
		{
			continue;
		}

		if (!SubFormation.SubFormationStrategy)
		{
			UE_LOG(
				LogEnemySpawner,
				Warning,
				TEXT("スポーン対象があるSubFormationにSubFormation Strategyが設定されていません。Spawner=%s, SubFormation=%s"),
				*GetName(),
				*SubFormation.SubFormationName.ToString());

			continue;
		}

		// [2] Spawner基準のローカルTransformからSubFormationの基準点を作る。
		const FTransform SubFormationWorldTransform =
			SubFormation.MakeWorldTransform(SpawnerWorldTransform);

		// [3] SubFormation内の敵数に合わせてスロットを生成する。
		const TArray<FEnemyFormationSlot> Slots =
			SubFormation.SubFormationStrategy->BuildSlots(SubFormationWorldTransform, SubFormationSpawnCount);

		if (Slots.Num() <= 0)
		{
			UE_LOG(
				LogEnemySpawner,
				Warning,
				TEXT("SlotStrategy returned no slots. Spawner=%s, SubFormation=%s"),
				*GetName(),
				*SubFormation.SubFormationName.ToString());

			continue;
		}

		// [4] SubFormationに割り当てられた敵をスロット順に生成する。
		SpawnEntryListAtSlots(
			SubFormation.SpawnEntries,
			Slots,
			SubFormation.SubFormationName.ToString());
	}
}

TArray<FEnemyFormationSlot> AEnemySpawner::BuildFallbackSlots(const int32 SlotCount) const
{
	TArray<FEnemyFormationSlot> Slots;

	if (SlotCount <= 0)
	{
		return Slots;
	}

	Slots.Reserve(SlotCount);

	const FTransform BaseTransform = GetActorTransform();
	const FVector Origin = BaseTransform.GetLocation();
	const FQuat Rotation = BaseTransform.GetRotation();
	const FVector RightVector = BaseTransform.GetUnitAxis(EAxis::Y);

	const float FirstOffset =
		-0.5f * static_cast<float>(SlotCount - 1) * FallbackSpawnSpacing;

	for (int32 SlotIndex = 0; SlotIndex < SlotCount; ++SlotIndex)
	{
		// [1] Spawner基準のY軸方向に仮スロットを作る。
		const float OffsetY = FirstOffset + static_cast<float>(SlotIndex) * FallbackSpawnSpacing;
		const FVector SlotLocation = Origin + RightVector * OffsetY;

		// [2] スロットの向きはSpawnerの向きを使う。
		const FTransform SlotTransform(Rotation, SlotLocation, FVector::OneVector);
		Slots.Emplace(SlotTransform, SlotIndex);
	}

	return Slots;
}

void AEnemySpawner::SpawnEntryListAtSlots(
	const TArray<FEnemySpawnEntry>& Entries,
	const TArray<FEnemyFormationSlot>& Slots,
	const FString& ContextName)
{
	int32 SlotIndex = 0;

	for (const FEnemySpawnEntry& Entry : Entries)
	{
		if (Entry.HasInvalidClassWithCount())
		{
			UE_LOG(
				LogEnemySpawner,
				Warning,
				TEXT("SpawnEntry has Count but EnemyClass is null. Spawner=%s, Context=%s"),
				*GetName(),
				*ContextName);
		}

		const int32 SpawnCount = Entry.GetSpawnCount();

		for (int32 SpawnIndex = 0; SpawnIndex < SpawnCount; ++SpawnIndex)
		{
			if (!Slots.IsValidIndex(SlotIndex))
			{
				UE_LOG(
					LogEnemySpawner,
					Warning,
					TEXT("Not enough slots. Spawner=%s, Context=%s, SlotIndex=%d, SlotCount=%d"),
					*GetName(),
					*ContextName,
					SlotIndex,
					Slots.Num());

				return;
			}

			const FEnemyFormationSlot& Slot = Slots[SlotIndex];

			SpawnEnemyAtSlot(Entry.EnemyClass, Slot);
			DrawDebugSlot(Slot);

			++SlotIndex;
		}
	}
}

AEnemyCharacter* AEnemySpawner::SpawnEnemyAtSlot(
	const TSubclassOf<AEnemyCharacter> EnemyClass,
	const FEnemyFormationSlot& Slot)
{
	if (!EnemyClass)
	{
		return nullptr;
	}

	UWorld* World = GetWorld();

	if (!World)
	{
		return nullptr;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = this;
	SpawnParams.SpawnCollisionHandlingOverride =
		ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

#if WITH_EDITOR
	if (!World->IsGameWorld())
	{
		SpawnParams.ObjectFlags |= RF_Transient;
	}
#endif

	AEnemyCharacter* SpawnedEnemy =
		World->SpawnActor<AEnemyCharacter>(
			EnemyClass.Get(),
			Slot.WorldTransform,
			SpawnParams);

	if (!SpawnedEnemy)
	{
		UE_LOG(
			LogEnemySpawner,
			Warning,
			TEXT("Failed to spawn enemy. Spawner=%s, EnemyClass=%s"),
			*GetName(),
			*GetNameSafe(EnemyClass.Get()));

		return nullptr;
	}

	SpawnedEnemies.Add(SpawnedEnemy);

	UE_LOG(
		LogEnemySpawner,
		Log,
		TEXT("Spawned enemy. Spawner=%s, Enemy=%s, SlotIndex=%d"),
		*GetName(),
		*GetNameSafe(SpawnedEnemy),
		Slot.SlotIndex);

	return SpawnedEnemy;
}

void AEnemySpawner::DrawDebugSlot(const FEnemyFormationSlot& Slot) const
{
	if (!bDrawDebugSlots || !GetWorld())
	{
		return;
	}

	const FVector Location = Slot.WorldTransform.GetLocation();
	const FVector Forward = Slot.WorldTransform.GetUnitAxis(EAxis::X);

	DrawDebugSphere(
		GetWorld(),
		Location,
		35.0f,
		12,
		FColor::Cyan,
		false,
		DebugDrawDuration);

	DrawDebugDirectionalArrow(
		GetWorld(),
		Location,
		Location + Forward * 80.0f,
		20.0f,
		FColor::Green,
		false,
		DebugDrawDuration);
}

int32 AEnemySpawner::GetTotalSpawnCount(const TArray<FEnemySpawnEntry>& Entries) const
{
	int32 TotalCount = 0;

	for (const FEnemySpawnEntry& Entry : Entries)
	{
		TotalCount += Entry.GetSpawnCount();
	}

	return TotalCount;
}