#include "Enemies/Encounter/EnemyEncounter.h"
#include "Characters/EnemyCharacter.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/Pawn.h"
#include "AI/Targeting/Targetable.h"
#include "EngineUtils.h"
#include "DrawDebugHelpers.h"

DEFINE_LOG_CATEGORY_STATIC(LogEnemyEncounter, Log, All);

static TAutoConsoleVariable<bool> CVarDebugEnemyEncounter(
	TEXT("TacticalAI.DebugEnemyEncounter"), false,
	TEXT("Draw enemy encounter origin, member links and state."));

AEnemyEncounter::AEnemyEncounter()
{
	// Tick은 저주기 감지(내부 누적)와 디버그 표시에 사용한다.
	// Tickは低頻度感知（内部累積）とデバッグ表示に使用する。
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
}

void AEnemyEncounter::InitializeSettings(const FEnemyEncounterSettings& InSettings)
{
	Settings = InSettings;
}

void AEnemyEncounter::BeginPlay()
{
	Super::BeginPlay();
	
	// 전달받은 감지 주기 내에서 첫 감지 시점을 분산한다.
	// 渡された感知周期内で初回の感知タイミングを分散する。
	TimeUntilNextSense = FMath::FRandRange(0.f, Settings.SensingInterval);
}

void AEnemyEncounter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	
	TickSensing(DeltaTime);

	if (CVarDebugEnemyEncounter.GetValueOnGameThread())
	{
		DrawDebugEncounter();
	}
}

void AEnemyEncounter::RegisterMember(AEnemyCharacter* Member)
{
	if (!IsValid(Member) || Members.Contains(Member))
	{
		return;
	}

	// [1] 멤버 목록에 추가하고 적의 소속 인카운터를 설정한다.
	// メンバー一覧に追加し、敵の所属エンカウンターを設定する。
	Members.Add(Member);
	Member->SetEncounter(this);

	// [2] 멤버 파괴 시 목록 제거와 전멸 판정을 수행하도록 구독한다.
	// メンバー破壊時に一覧からの除外と全滅判定を行うため購読する。
	Member->OnDestroyed.AddDynamic(this, &AEnemyEncounter::HandleMemberDestroyed);

	UE_LOG(LogEnemyEncounter, Log, TEXT("エンカウンターにメンバーを登録しました。Encounter=%s, Member=%s, 現在数=%d"),
		*GetName(), *GetNameSafe(Member), Members.Num());
}

TArray<AEnemyCharacter*> AEnemyEncounter::GetAliveMembers() const
{
	TArray<AEnemyCharacter*> Result;
	Result.Reserve(Members.Num());
	for (const TObjectPtr<AEnemyCharacter>& Member : Members)
	{
		if (IsValid(Member))
		{
			Result.Add(Member);
		}
	}
	return Result;
}

bool AEnemyEncounter::IsDefeated() const
{
	return GetAliveMembers().Num() == 0;
}

void AEnemyEncounter::HandleMemberDestroyed(AActor* DestroyedActor)
{
	// [1] 레벨 종료 중의 연쇄 파괴에서는 전멸 통지를 쏘지 않는다.
	// レベル終了中の連鎖破壊では全滅通知を出さない。
	const UWorld* World = GetWorld();
	if (!World || World->bIsTearingDown)
	{
		return;
	}

	Members.Remove(Cast<AEnemyCharacter>(DestroyedActor));

	// [2] 마지막 멤버가 죽었을 때만 전멸을 알린다.
	// 最後のメンバー死亡時のみ全滅を通知。
	if (IsDefeated())
	{
		UE_LOG(LogEnemyEncounter, Log, TEXT("エンカウンターが全滅しました。Encounter=%s"), *GetName());
		OnEncounterDefeated.Broadcast(this);
	}
}

void AEnemyEncounter::DrawDebugEncounter() const
{
	const FVector Origin = GetActorLocation();
	DrawDebugSphere(GetWorld(), Origin, 40.f, 8, FColor::Red, false, -1.f);
	for (const AEnemyCharacter* Member : GetAliveMembers())
	{
		DrawDebugLine(GetWorld(), Origin, Member->GetActorLocation(), FColor::Orange, false, -1.f);
	}
	
	// TacticalAI.DebugEnemyEncounter 1
	// 状態をアンカー上にテキスト表示。
	static const TMap<EEnemyEncounterState, FColor> StateColors = {
		{ EEnemyEncounterState::Idle, FColor::Silver }, { EEnemyEncounterState::Alert, FColor::Yellow },
		{ EEnemyEncounterState::Engaged, FColor::Red }, { EEnemyEncounterState::Return, FColor::Cyan } };
	DrawDebugString(GetWorld(), Origin + FVector(0, 0, 120.f),
		UEnum::GetValueAsString(EncounterState), nullptr, StateColors[EncounterState], 0.f);
}

void AEnemyEncounter::TickSensing(float DeltaTime)
{
	TimeUntilNextSense -= DeltaTime;
	if (TimeUntilNextSense > 0.f || IsDefeated())
	{
		return;
	}
	TimeUntilNextSense = Settings.SensingInterval;

	// [1] 감지 대상 = 플레이어 폰 (엔진 API — 파티 타입 의존 없음, ADR-0008 의존 방향 유지).
	//     전투 시작·이탈이 플레이어 기준이라는 파티 측 규칙과 대칭.
	// 感知対象＝プレイヤーPawn（エンジンAPIのみ — パーティ型への依存なし）。
	const APawn* Player = GetSensedPlayerPawn();
	if (!Player)
	{
		return;
	}

	const FVector Anchor = GetActorLocation();
	const float DistToNearestMember = [&]()
	{
		float Best = TNumericLimits<float>::Max();
		for (const AEnemyCharacter* Member : GetAliveMembers())
		{
			Best = FMath::Min(Best, FVector::Dist(Player->GetActorLocation(), Member->GetActorLocation()));
		}
		return Best;
	}();
	const float DistToAnchor = FVector::Dist(Player->GetActorLocation(), Anchor);

	// [2] Engaged 동안 알려진 적 갱신.
	// Engaged中は既知敵を更新（感知周期を再利用）。
	if (EncounterState == EEnemyEncounterState::Engaged)
	{
		RefreshKnownHostiles();
	}
	
	// [3] 상태별 전이 판정. Exit < Enter 설정 실수는 Enter 거리로 방어.
	// 状態別の遷移判定。Exit<Enterの設定ミスはEnter距離で防御。
	const float SafeAlertExit = FMath::Max(Settings.AlertExitDistance, Settings.AlertEnterDistance);

	switch (EncounterState)
	{
	case EEnemyEncounterState::Idle:
		if (DistToNearestMember <= Settings.AlertEnterDistance) SetEncounterState(EEnemyEncounterState::Alert);
		break;

	case EEnemyEncounterState::Alert:
		if (DistToNearestMember <= Settings.EngageDistance)     SetEncounterState(EEnemyEncounterState::Engaged);
		else if (DistToNearestMember > SafeAlertExit)  SetEncounterState(EEnemyEncounterState::Idle);
		break;

	case EEnemyEncounterState::Engaged:
		// 追跡諦めのみアンカー基準。
		if (DistToAnchor > Settings.ChaseGiveUpDistance)        SetEncounterState(EEnemyEncounterState::Return);
		break;

	case EEnemyEncounterState::Return:
		if (DistToNearestMember <= Settings.EngageDistance)     SetEncounterState(EEnemyEncounterState::Engaged);
		else if (AreAllMembersNearAnchor())            SetEncounterState(EEnemyEncounterState::Idle);
		break;
	}
}

void AEnemyEncounter::NotifyAttackedBy(AActor* Attacker)
{
	// 인카운터 일원이 피격당하면 어느 상태에서든 즉시 교전.
	// どの状態からでも即時交戦へ。
	if (IsDefeated())
	{
		return;
	}
	UE_LOG(LogEnemyEncounter, Log, TEXT("被弾通知を受けました。Encounter=%s, Attacker=%s"),
		*GetName(), *GetNameSafe(Attacker));
	SetEncounterState(EEnemyEncounterState::Engaged);
}

void AEnemyEncounter::SetEncounterState(const EEnemyEncounterState NewState)
{
	if (EncounterState == NewState)
	{
		return;
	}

	const EEnemyEncounterState OldState = EncounterState;
	EncounterState = NewState;

	// Engaged 이탈 = 교전 지식 소멸. 진입 시엔 즉시 1회 갱신.
	// Engaged離脱＝交戦知識の消滅。進入時は即時1回更新。
	if (NewState == EEnemyEncounterState::Engaged)
	{
		RefreshKnownHostiles();
	}
	else
	{
		KnownHostiles.Reset();
	}
	
	UE_LOG(LogEnemyEncounter, Log, TEXT("エンカウンター状態が遷移しました。Encounter=%s, %s → %s"),
		*GetName(), *UEnum::GetValueAsString(OldState), *UEnum::GetValueAsString(NewState));

	OnEncounterStateChanged.Broadcast(this, OldState, NewState);
}

const APawn* AEnemyEncounter::GetSensedPlayerPawn() const
{
	return UGameplayStatics::GetPlayerPawn(this, 0);
}

bool AEnemyEncounter::AreAllMembersNearAnchor() const
{
	const FVector Anchor = GetActorLocation();
	for (const AEnemyCharacter* Member : GetAliveMembers())
	{
		if (FVector::Dist(Anchor, Member->GetActorLocation()) > Settings.ReturnHomeRadius)
		{
			return false;
		}
	}
	return true;
}

void AEnemyEncounter::RefreshKnownHostiles()
{
	KnownHostiles.Reset();

	// ⚠ 2진영 가정: "ITargetable 구현 폰 중 우리 편(적 캐릭터)이 아니면 적대".
	// ⚠ 2陣営前提：第3陣営導入時はこのフィルタのみ差し替え。
	for (TActorIterator<APawn> It(GetWorld()); It; ++It)
	{
		APawn* Pawn = *It;
		if (Pawn->Implements<UTargetable>() && !Pawn->IsA<AEnemyCharacter>())
		{
			KnownHostiles.Add(Pawn);
		}
	}
}

TArray<AActor*> AEnemyEncounter::GetKnownHostiles() const
{
	// Engaged 게이트 + 유효성 필터. 자격(IsTargetable) 필터는 셀렉터 베이스의 몫.
	// Engagedゲート＋有効性フィルタ。ターゲット可能資格フィルタ(IsTargetable)はセレクタ基底の責務。
	TArray<AActor*> Result;
	if (EncounterState != EEnemyEncounterState::Engaged)
	{
		return Result;
	}
	Result.Reserve(KnownHostiles.Num());
	for (const TWeakObjectPtr<AActor>& Weak : KnownHostiles)
	{
		if (AActor* Hostile = Weak.Get())
		{
			Result.Add(Hostile);
		}
	}
	return Result;
}