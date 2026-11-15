#include "Party/PartyPerceptionComponent.h"
#include "Party/PartyManager.h"
#include "Characters/PartyCharacter.h"
#include "Enemies/Encounter/EnemyEncounterBase.h"
#include "Characters/EnemyCharacter.h"
#include "EngineUtils.h"
#include "DrawDebugHelpers.h"

DEFINE_LOG_CATEGORY_STATIC(LogPartyPerception, Log, All);

// 파티 인지 디버그 표시. 콘솔: TacticalAI.DebugPartyPerception 1
// パーティ知覚のデバッグ表示。コンソール: TacticalAI.DebugPartyPerception 1
static TAutoConsoleVariable<bool> CVarDebugPartyPerception(
	TEXT("TacticalAI.DebugPartyPerception"), false,
	TEXT("Draw perception radii and lines to perceived encounters."));

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
	ScanForEncounters();
}

TArray<AEnemyEncounterBase*> UPartyPerceptionComponent::GetPerceivedEncounters() const
{
	TArray<AEnemyEncounterBase*> Result;
	Result.Reserve(PerceivedEncounters.Num());
	for (const TWeakObjectPtr<AEnemyEncounterBase>& Weak : PerceivedEncounters)
	{
		AEnemyEncounterBase* Encounter = Weak.Get();
		if (Encounter && !Encounter->IsDefeated())
		{
			Result.Add(Encounter);
		}
	}
	return Result;
}

void UPartyPerceptionComponent::MarkEncounterPerceived(AEnemyEncounterBase* Encounter)
{
	if (!IsValid(Encounter) || PerceivedEncounters.Contains(Encounter))
	{
		return;
	}
	PerceivedEncounters.Add(Encounter);
	UE_LOG(LogPartyPerception, Log, TEXT("敵エンカウンターを知覚しました。Encounter=%s"), *GetNameSafe(Encounter));
}

void UPartyPerceptionComponent::ScanForEncounters()
{
	const APartyCharacter* Leader = GetLeader();
	if (!Leader) return;

	const FVector Origin = Leader->GetActorLocation();
	const float EnterRadiusSq = FMath::Square(PerceiveEnterRadius);
	// Exit < Enter로 잘못 설정돼도 최소한 Enter 거리는 보장 (설정 실수 방어).
	// Exit < Enter の設定ミスでも最低Enter距離は保証。
	const float ExitRadiusSq = FMath::Square(FMath::Max(PerceiveExitRadius, PerceiveEnterRadius));

	// [1] 이탈 판정: 인지 중인 인카운터가 전원 Exit 반경 밖이면 해제. 무효·전멸도 함께 정리.
	// 離脱判定：知覚中エンカウンターが全員Exit半径外なら解除。無効・全滅もここで掃除。
	for (int32 i = PerceivedEncounters.Num() - 1; i >= 0; --i)
	{
		AEnemyEncounterBase* Encounter = PerceivedEncounters[i].Get();
		if (!Encounter || Encounter->IsDefeated() || !IsAnyMemberInRadius(Encounter, Origin, ExitRadiusSq))
		{
			if (Encounter)
			{
				UE_LOG(LogPartyPerception, Log, TEXT("敵エンカウンターの知覚を解除しました。Encounter=%s"), *GetNameSafe(Encounter));
			}
			PerceivedEncounters.RemoveAt(i);
		}
	}

	// [2] 진입 판정: 미인지 인카운터 중 1체라도 Enter 반경 안이면 인카운터 전체 인지
	//     인카운터 수는 스폰 지점·배치 규모라 전체 순회로 충분 — LOD 도입 시 내부만 교체.
	// 進入判定：未知覚エンカウンターの1体でもEnter半径内なら全体を知覚。
	// 基底型で走査するため、系統に関係なく同じ判定。
	for (TActorIterator<AEnemyEncounterBase> It(GetWorld()); It; ++It)
	{
		AEnemyEncounterBase* Encounter = *It;
		if (Encounter->IsDefeated() || PerceivedEncounters.Contains(Encounter)) continue;

		if (IsAnyMemberInRadius(Encounter, Origin, EnterRadiusSq))
		{
			MarkEncounterPerceived(Encounter);
		}
	}

	DrawDebugPerception(Origin);
}

bool UPartyPerceptionComponent::IsAnyMemberInRadius(const AEnemyEncounterBase* Encounter,
	const FVector& Origin, const float RadiusSq) const
{
	for (const AEnemyCharacter* Member : Encounter->GetAliveMembers())
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
	for (const AEnemyEncounterBase* Encounter : GetPerceivedEncounters())
	{
		DrawDebugLine(GetWorld(), Origin, Encounter->GetActorLocation(),
			FColor::Green, false, ScanInterval, 0, 2.f);
	}
}