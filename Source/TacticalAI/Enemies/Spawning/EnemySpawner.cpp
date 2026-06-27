#include "Enemies/Spawning/EnemySpawner.h"
#include "Components/SceneComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "NavigationSystem.h"

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

			FTransform ResolvedSpawnTransform = Slot.WorldTransform;

			AEnemyCharacter* SpawnedEnemy = SpawnEnemyAtSlot(
				Entry.EnemyClass,
				Slot,
				ResolvedSpawnTransform);

			if (SpawnedEnemy)
			{
				DrawDebugSlot(ResolvedSpawnTransform, Slot.SlotIndex);
			}

			++SlotIndex;
		}
	}
}

AEnemyCharacter* AEnemySpawner::SpawnEnemyAtSlot(
	const TSubclassOf<AEnemyCharacter> EnemyClass,
	const FEnemyFormationSlot& Slot,
	FTransform& OutResolvedSpawnTransform)
{
	OutResolvedSpawnTransform = Slot.WorldTransform;

	if (!EnemyClass || !GetWorld())
	{
		return nullptr;
	}

	const TArray<FTransform> SpawnCandidates =
		BuildSpawnResolveCandidates(Slot.WorldTransform);

	for (FTransform CandidateTransform : SpawnCandidates)
	{
		FVector ProjectedLocation = CandidateTransform.GetLocation();

		if (TryProjectSpawnLocationToNavigation(CandidateTransform.GetLocation(), ProjectedLocation))
		{
			CandidateTransform.SetLocation(ProjectedLocation);
		}

		AEnemyCharacter* SpawnedEnemy = TrySpawnEnemyAtTransform(
			EnemyClass,
			CandidateTransform,
			ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButDontSpawnIfColliding);

		if (SpawnedEnemy)
		{
			OutResolvedSpawnTransform = CandidateTransform;

			UE_LOG(
				LogEnemySpawner,
				Log,
				TEXT("敵をスポーンしました。Spawner=%s, Enemy=%s, SlotIndex=%d"),
				*GetName(),
				*GetNameSafe(SpawnedEnemy),
				Slot.SlotIndex);

			return SpawnedEnemy;
		}

		if (!bResolveSpawnCollision)
		{
			break;
		}
	}

	UE_LOG(
		LogEnemySpawner,
		Warning,
		TEXT("安全なスポーン位置を見つけられませんでした。Spawner=%s, EnemyClass=%s, SlotIndex=%d"),
		*GetName(),
		*GetNameSafe(EnemyClass.Get()),
		Slot.SlotIndex);

	return nullptr;
}

AEnemyCharacter* AEnemySpawner::TrySpawnEnemyAtTransform(
	const TSubclassOf<AEnemyCharacter> EnemyClass,
	const FTransform& SpawnTransform,
	const ESpawnActorCollisionHandlingMethod CollisionHandlingMethod)
{
	if (!EnemyClass || !GetWorld())
	{
		return nullptr;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = this;
	SpawnParams.SpawnCollisionHandlingOverride = CollisionHandlingMethod;

#if WITH_EDITOR
	if (!GetWorld()->IsGameWorld())
	{
		SpawnParams.ObjectFlags |= RF_Transient;
	}
#endif

	AEnemyCharacter* SpawnedEnemy =
		GetWorld()->SpawnActor<AEnemyCharacter>(
			EnemyClass.Get(),
			SpawnTransform,
			SpawnParams);

	if (SpawnedEnemy)
	{
		SpawnedEnemies.Add(SpawnedEnemy);
	}

	return SpawnedEnemy;
}

void AEnemySpawner::DrawDebugSlot(
	const FTransform& SlotTransform,
	const int32 SlotIndex) const
{
	if (!bDrawDebugSlots || !GetWorld())
	{
		return;
	}

	const FVector Location = SlotTransform.GetLocation();
	const FVector Forward = SlotTransform.GetUnitAxis(EAxis::X);

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

TArray<FTransform> AEnemySpawner::BuildSpawnResolveCandidates(
	const FTransform& DesiredTransform) const
{
	TArray<FTransform> Candidates;
	Candidates.Add(DesiredTransform);

	if (!bResolveSpawnCollision)
	{
		return Candidates;
	}

	const float SafeRadiusStep = FMath::Max(1.0f, SpawnResolveRadiusStep);
	const int32 SafeDirectionCount = FMath::Max(4, SpawnResolveDirectionCount);

	const FVector Origin = DesiredTransform.GetLocation();
	const FQuat Rotation = DesiredTransform.GetRotation();
	const FVector ForwardVector = DesiredTransform.GetUnitAxis(EAxis::X);
	const FVector RightVector = DesiredTransform.GetUnitAxis(EAxis::Y);

	for (float Radius = SafeRadiusStep; Radius <= SpawnResolveMaxRadius; Radius += SafeRadiusStep)
	{
		for (int32 DirectionIndex = 0; DirectionIndex < SafeDirectionCount; ++DirectionIndex)
		{
			// [1] 원래 슬롯 주변을 링 형태로 넓혀가며 후보 위치를 만든다.
			// [1] 元のスロット周辺をリング状に広げながら候補位置を作る。
			const float AngleRadians =
				2.0f * PI * static_cast<float>(DirectionIndex) / static_cast<float>(SafeDirectionCount);

			const FVector Offset =
				ForwardVector * FMath::Cos(AngleRadians) * Radius
				+ RightVector * FMath::Sin(AngleRadians) * Radius;

			FTransform CandidateTransform(Rotation, Origin + Offset, FVector::OneVector);
			Candidates.Add(CandidateTransform);
		}
	}

	return Candidates;
}

bool AEnemySpawner::TryProjectSpawnLocationToNavigation(
	const FVector& SourceLocation,
	FVector& OutProjectedLocation) const
{
	OutProjectedLocation = SourceLocation;

	if (!bProjectSpawnToNavMesh || !GetWorld())
	{
		return false;
	}

	UNavigationSystemV1* NavSystem =
		FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());

	if (!NavSystem)
	{
		return false;
	}

	FNavLocation NavLocation;

	if (!NavSystem->ProjectPointToNavigation(
		SourceLocation,
		NavLocation,
		NavProjectionExtent))
	{
		return false;
	}

	OutProjectedLocation = NavLocation.Location;
	return true;
}