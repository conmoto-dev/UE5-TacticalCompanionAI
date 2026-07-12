#include "Party/PartyPerceptionComponent.h"
#include "Party/PartyManager.h"
#include "Characters/PartyCharacter.h"
#include "Enemies/Group/EnemyGroup.h"
#include "Characters/EnemyCharacter.h"
#include "EngineUtils.h"
#include "DrawDebugHelpers.h"

DEFINE_LOG_CATEGORY_STATIC(LogPartyPerception, Log, All);

// 파티 인지 디버그 표시. 콘솔: TacticalAI.DebugPartyPerception 1
// パーティ知覚のデバッグ表示。コンソール: TacticalAI.DebugPartyPerception 1
static TAutoConsoleVariable<bool> CVarDebugPartyPerception(
	TEXT("TacticalAI.DebugPartyPerception"), false,
	TEXT("Draw perception radii and lines to perceived groups."));

UPartyPerceptionComponent::UPartyPerceptionComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	// 저주기 스캔. 인지 판정에 매 프레임은 과잉 — 간격은 에디터 값으로.
	// 低頻度スキャン。知覚判定に毎フレームは過剰。
	PrimaryComponentTick.TickInterval = ScanInterval;
}

void UPartyPerceptionComponent::BeginPlay()
{
	Super::BeginPlay();
	// 에디터에서 바꾼 간격을 런타임 틱에 반영 (생성자 값은 CDO 시점).
	// エディタで変更した間隔をランタイムTickへ反映。
	PrimaryComponentTick.TickInterval = ScanInterval;
}

void UPartyPerceptionComponent::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	ScanForGroups();
}

TArray<AEnemyGroup*> UPartyPerceptionComponent::GetPerceivedGroups() const
{
	TArray<AEnemyGroup*> Result;
	Result.Reserve(PerceivedGroups.Num());
	for (const TWeakObjectPtr<AEnemyGroup>& Weak : PerceivedGroups)
	{
		AEnemyGroup* Group = Weak.Get();
		if (Group && !Group->IsDefeated())
		{
			Result.Add(Group);
		}
	}
	return Result;
}

void UPartyPerceptionComponent::MarkGroupPerceived(AEnemyGroup* Group)
{
	if (!IsValid(Group) || PerceivedGroups.Contains(Group))
	{
		return;
	}
	PerceivedGroups.Add(Group);
	UE_LOG(LogPartyPerception, Log, TEXT("敵グループを知覚しました。Group=%s"), *GetNameSafe(Group));
}

void UPartyPerceptionComponent::ScanForGroups()
{
	const APartyCharacter* Leader = GetLeader();
	if (!Leader) return;

	const FVector Origin = Leader->GetActorLocation();
	const float EnterRadiusSq = FMath::Square(PerceiveEnterRadius);
	// Exit < Enter로 잘못 설정돼도 최소한 Enter 거리는 보장 (설정 실수 방어).
	// Exit < Enter の設定ミスでも最低Enter距離は保証。
	const float ExitRadiusSq = FMath::Square(FMath::Max(PerceiveExitRadius, PerceiveEnterRadius));

	// [1] 이탈 판정: 인지 중인 그룹이 전원 Exit 반경 밖이면 해제. 무효·전멸도 함께 정리.
	// 離脱判定：知覚中グループが全員Exit半径外なら解除。無効・全滅もここで掃除。
	for (int32 i = PerceivedGroups.Num() - 1; i >= 0; --i)
	{
		AEnemyGroup* Group = PerceivedGroups[i].Get();
		if (!Group || Group->IsDefeated() || !IsAnyMemberInRadius(Group, Origin, ExitRadiusSq))
		{
			if (Group)
			{
				UE_LOG(LogPartyPerception, Log, TEXT("敵グループの知覚を解除しました。Group=%s"), *GetNameSafe(Group));
			}
			PerceivedGroups.RemoveAt(i);
		}
	}

	// [2] 진입 판정: 미인지 그룹 중 1체라도 Enter 반경 안이면 그룹 전체 인지.
	//     그룹 수는 스폰 지점 규모라 전체 순회로 충분 — LOD 도입 시 내부만 교체.
	// 進入判定：未知覚グループの1体でもEnter半径内なら全体を知覚。
	for (TActorIterator<AEnemyGroup> It(GetWorld()); It; ++It)
	{
		AEnemyGroup* Group = *It;
		if (Group->IsDefeated() || PerceivedGroups.Contains(Group)) continue;

		if (IsAnyMemberInRadius(Group, Origin, EnterRadiusSq))
		{
			MarkGroupPerceived(Group);
		}
	}

	DrawDebugPerception(Origin);
}

bool UPartyPerceptionComponent::IsAnyMemberInRadius(const AEnemyGroup* Group,
	const FVector& Origin, const float RadiusSq) const
{
	for (const AEnemyCharacter* Member : Group->GetAliveMembers())
	{
		if (FVector::DistSquared(Origin, Member->GetActorLocation()) <= RadiusSq)
		{
			return true;
		}
	}
	return false;
}

const APartyCharacter* UPartyPerceptionComponent::GetLeader() const
{
	const APartyManager* Manager = Cast<APartyManager>(GetOwner());
	return Manager ? Manager->GetLeader() : nullptr;
}

void UPartyPerceptionComponent::DrawDebugPerception(const FVector& Origin) const
{
	if (!CVarDebugPartyPerception.GetValueOnGameThread()) return;

	DrawDebugCircle(GetWorld(), Origin, PerceiveEnterRadius, 48, FColor::Green,
		false, ScanInterval, 0, 2.f, FVector::RightVector, FVector::ForwardVector, false);
	DrawDebugCircle(GetWorld(), Origin, PerceiveExitRadius, 48, FColor::Yellow,
		false, ScanInterval, 0, 2.f, FVector::RightVector, FVector::ForwardVector, false);
	for (const AEnemyGroup* Group : GetPerceivedGroups())
	{
		DrawDebugLine(GetWorld(), Origin, Group->GetActorLocation(),
			FColor::Green, false, ScanInterval, 0, 2.f);
	}
}