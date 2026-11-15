#include "Party/PartyPerceptionComponent.h"
#include "Party/PartyManager.h"
#include "Characters/PartyCharacter.h"
#include "Enemies/Encounter/EnemyEncounter.h"
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

TArray<AEnemyEncounter*> UPartyPerceptionComponent::GetPerceivedEncounters() const
{
	TArray<AEnemyEncounter*> Result;
	Result.Reserve(PerceivedEncounters.Num());
	for (const TWeakObjectPtr<AEnemyEncounter>& Weak : PerceivedEncounters)
	{
		AEnemyEncounter* Encounter = Weak.Get();
		if (Encounter && !Encounter->IsDefeated())
		{
			Result.Add(Encounter);
		}
	}
	return Result;
}

void UPartyPerceptionComponent::MarkEncounterPerceived(AEnemyEncounter* Encounter)
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
		AEnemyEncounter* Encounter = PerceivedEncounters[i].Get();
		if (!Encounter || Encounter->IsDefeated() || !IsAnyMemberInRadius(Encounter, Origin, ExitRadiusSq))
		{
			if (Encounter)
			{
				UE_LOG(LogPartyPerception, Log, TEXT("敵エンカウンターの知覚を解除しました。Encounter=%s"), *GetNameSafe(Encounter));
			}
			PerceivedEncounters.RemoveAt(i);
		}
	}

	// [2] 미인지 인카운터의 멤버 중 하나라도 진입 반경 안에 있으면 같은 소속 적 전체를 인지한다.
	// 未知覚のエンカウンターのメンバー1体でも開始半径内にいれば知覚する。
	for (TActorIterator<AEnemyEncounter> It(GetWorld()); It; ++It)
	{
		AEnemyEncounter* Encounter = *It;
		if (Encounter->IsDefeated() || PerceivedEncounters.Contains(Encounter)) continue;

		if (IsAnyMemberInRadius(Encounter, Origin, EnterRadiusSq))
		{
			MarkEncounterPerceived(Encounter);
		}
	}

	DrawDebugPerception(Origin);
}

bool UPartyPerceptionComponent::IsAnyMemberInRadius(const AEnemyEncounter* Encounter,
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
	for (const AEnemyEncounter* Encounter : GetPerceivedEncounters())
	{
		DrawDebugLine(GetWorld(), Origin, Encounter->GetActorLocation(),
			FColor::Green, false, ScanInterval, 0, 2.f);
	}
}