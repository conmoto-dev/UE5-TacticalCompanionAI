// Fill out your copyright notice in the Description page of Project Settings.

#include "Enemies/EnemyGroup.h"
#include "Characters/EnemyCharacter.h"
#include "TacticalAI.h"

AEnemyGroup::AEnemyGroup()
{
	PrimaryActorTick.bCanEverTick = false;
}

void AEnemyGroup::InitializeGroup(const TArray<TSubclassOf<AEnemyCharacter>>& SpawnClasses)
{
	UWorld* World = GetWorld();
	if (!World) return;

	const int32 Total = SpawnClasses.Num();
	if (Total == 0) return;

	Members.Reserve(Total);

	// 그룹 위치 기준으로 멤버를 원 둘레에 등각 배치(겹침 방지용 임시).
	// 진형(조각4)이 슬롯으로 덮어쓰므로 시작 각도·정렬은 의미 없다 — 단순 분산.
	// グループ位置を基準に円周へ等角配置（仮）。隊形が上書きするため起点角は無意味。
	const FVector Origin = GetActorLocation();

	for (int32 i = 0; i < Total; ++i)
	{
		const TSubclassOf<AEnemyCharacter> Class = SpawnClasses[i];
		if (!Class)
		{
			// 빈 엔트리는 건너뛴다 — 디자이너가 BP를 안 채웠을 수 있음.
			UE_LOG(LogTacticalAI, Warning, TEXT("%s: null spawn class at index %d, skipped."), *GetNameSafe(this), i);
			continue;
		}

		// 등각 분산 위치. 1마리면 중심, 여럿이면 원 둘레.
		FVector SpawnLoc = Origin;
		if (Total > 1)
		{
			const float AngleRad = (static_cast<float>(i) / static_cast<float>(Total)) * 2.f * PI;
			SpawnLoc = Origin + FVector(FMath::Cos(AngleRad), FMath::Sin(AngleRad), 0.f) * SpawnRingRadius;
		}

		FActorSpawnParameters Params;
		Params.Owner = this;
		// 캡슐이 지면·다른 멤버와 겹쳐도 일단 스폰(임시 배치라 보정은 진형이 함).
		// カプセルが重なってもまずスポーン（仮配置、補正は隊形側）。
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

		AEnemyCharacter* Spawned = World->SpawnActor<AEnemyCharacter>(Class, SpawnLoc, GetActorRotation(), Params);
		if (Spawned)
		{
			Members.Add(Spawned);
		}
	}
}