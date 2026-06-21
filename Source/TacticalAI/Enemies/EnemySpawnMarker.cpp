// Fill out your copyright notice in the Description page of Project Settings.

#include "Enemies/EnemySpawnMarker.h"
#include "Enemies/EnemyGroup.h"
#include "Characters/EnemyCharacter.h"
#include "Components/SceneComponent.h"

AEnemySpawnMarker::AEnemySpawnMarker()
{
	PrimaryActorTick.bCanEverTick = false;

	// 루트 — 에디터에서 마커 위치를 잡고 옮기기 위한 최소 컴포넌트.
	// 시각 표시(빌보드/메시)가 필요하면 BP에서 추가. 마커는 위치만 의미 있다.
	// ルート — エディタで位置を掴むための最小コンポーネント。視覚表示はBPで追加。
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void AEnemySpawnMarker::BeginPlay()
{
	Super::BeginPlay();

	// [지금] 무조건 스폰. [나중] 이 한 줄을 빼고, 거리 최적화 Subsystem이 근처일 때만
	//        SpawnGroup()을 호출하도록 트리거만 교체한다. SpawnGroup 자체는 무변경.
	// [今]無条件スポーン。[後]この行を外し、Subsystemが近接時のみSpawnGroup()を呼ぶ。
	SpawnGroup();
}

void AEnemySpawnMarker::SpawnGroup()
{
	UWorld* World = GetWorld();
	if (!World) return;

	// [1] Entries 평탄화 — 각 종류를 Count만큼 펼쳐 단일 클래스 배열로.
	//     빈 CharacterClass는 건너뛴다(디자이너 미입력 방어).
	// Entriesを平坦化 — 各種類をCount分展開。空クラスはスキップ。
	TArray<TSubclassOf<AEnemyCharacter>> Flattened;
	for (const FSpawnEntry& Entry : Entries)
	{
		if (!Entry.CharacterClass) continue;
		for (int32 i = 0; i < Entry.Count; ++i)
		{
			Flattened.Add(Entry.CharacterClass);
		}
	}
	if (Flattened.Num() == 0) return;

	// [2] 그룹 1개 스폰 (마커 위치). GroupClass 미지정이면 기본 AEnemyGroup.
	// グループを1つスポーン。
	TSubclassOf<AEnemyGroup> ClassToUse = AEnemyGroup::StaticClass();
	if (GroupClass)
	{
		ClassToUse = GroupClass;
	}

	FActorSpawnParameters Params;
	Params.Owner = this;
	AEnemyGroup* Group = World->SpawnActor<AEnemyGroup>(
		ClassToUse, GetActorLocation(), GetActorRotation(), Params);
	if (!Group) return;

	// [3] 멤버 스폰은 그룹에 위임 — 그룹이 멤버를 소유해야 진형이 붙는다(조각4).
	// メンバースポーンはグループに委譲 — 所有がグループにあることで隊形が付く。
	Group->InitializeGroup(Flattened);
}