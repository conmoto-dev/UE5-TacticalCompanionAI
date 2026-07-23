#include "Enemies/Group/EnemyGroup.h"
#include "Characters/EnemyCharacter.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/Pawn.h"
#include "DrawDebugHelpers.h"

DEFINE_LOG_CATEGORY_STATIC(LogEnemyGroup, Log, All);

static TAutoConsoleVariable<bool> CVarDebugEnemyGroup(
	TEXT("TacticalAI.DebugEnemyGroup"), false,
	TEXT("Draw enemy group origin and member links."));

AEnemyGroup::AEnemyGroup()
{
	// Tick은 디버그 표시 전용. BeginPlay에서 스위치에 따라 켠다.
	// Tickはデバッグ表示専用。BeginPlayでスイッチに応じて有効化。
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
}

void AEnemyGroup::BeginPlay()
{
	Super::BeginPlay();
	
	TimeUntilNextSense = FMath::FRandRange(0.f, SensingInterval);
}

void AEnemyGroup::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	
	TickSensing(DeltaTime);

	if (CVarDebugEnemyGroup.GetValueOnGameThread())
	{
		DrawDebugGroup();
	}
}

void AEnemyGroup::RegisterMember(AEnemyCharacter* Member)
{
	if (!IsValid(Member) || Members.Contains(Member))
	{
		return;
	}

	// [1] 명부에 추가하고 back-ptr를 꽂는다 (설정 경로는 여기뿐).
	// 名簿へ追加し、back-ptrを設定（設定経路はここのみ）。
	Members.Add(Member);
	Member->SetEnemyGroup(this);

	// [2] 파괴 통지 구독. 액터 파괴가 바인딩도 함께 정리하므로 개별 해제 불요.
	// 破壊通知を購読。Actor破壊時にバインドも消えるため個別解除は不要。
	Member->OnDestroyed.AddDynamic(this, &AEnemyGroup::HandleMemberDestroyed);

	UE_LOG(LogEnemyGroup, Log, TEXT("グループにメンバーを登録しました。Group=%s, Member=%s, 現在数=%d"),
		*GetName(), *GetNameSafe(Member), Members.Num());
}

TArray<AEnemyCharacter*> AEnemyGroup::GetAliveMembers() const
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

bool AEnemyGroup::IsDefeated() const
{
	return GetAliveMembers().Num() == 0;
}

void AEnemyGroup::HandleMemberDestroyed(AActor* DestroyedActor)
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
		UE_LOG(LogEnemyGroup, Log, TEXT("グループが全滅しました。Group=%s"), *GetName());
		OnGroupDefeated.Broadcast(this);
	}
}

void AEnemyGroup::DrawDebugGroup() const
{
	const FVector Origin = GetActorLocation();
	DrawDebugSphere(GetWorld(), Origin, 40.f, 8, FColor::Red, false, -1.f);
	for (const AEnemyCharacter* Member : GetAliveMembers())
	{
		DrawDebugLine(GetWorld(), Origin, Member->GetActorLocation(), FColor::Orange, false, -1.f);
	}
	
	// TacticalAI.DebugEnemyGroup 1
	// 状態をアンカー上にテキスト表示。
	static const TMap<EEnemyGroupState, FColor> StateColors = {
		{ EEnemyGroupState::Idle, FColor::Silver }, { EEnemyGroupState::Alert, FColor::Yellow },
		{ EEnemyGroupState::Engaged, FColor::Red }, { EEnemyGroupState::Return, FColor::Cyan } };
	DrawDebugString(GetWorld(), Origin + FVector(0, 0, 120.f),
		UEnum::GetValueAsString(GroupState), nullptr, StateColors[GroupState], 0.f);
}

void AEnemyGroup::TickSensing(float DeltaTime)
{
	TimeUntilNextSense -= DeltaTime;
	if (TimeUntilNextSense > 0.f || IsDefeated())
	{
		return;
	}
	TimeUntilNextSense = SensingInterval;

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

	// [2] 상태별 전이 판정. Exit < Enter 설정 실수는 Enter 거리로 방어.
	// 状態別の遷移判定。Exit<Enterの設定ミスはEnter距離で防御。
	const float SafeAlertExit = FMath::Max(AlertExitDistance, AlertEnterDistance);

	switch (GroupState)
	{
	case EEnemyGroupState::Idle:
		if (DistToNearestMember <= AlertEnterDistance) SetGroupState(EEnemyGroupState::Alert);
		break;

	case EEnemyGroupState::Alert:
		if (DistToNearestMember <= EngageDistance)     SetGroupState(EEnemyGroupState::Engaged);
		else if (DistToNearestMember > SafeAlertExit)  SetGroupState(EEnemyGroupState::Idle);
		break;

	case EEnemyGroupState::Engaged:
		// 追跡諦めのみアンカー基準。
		if (DistToAnchor > ChaseGiveUpDistance)        SetGroupState(EEnemyGroupState::Return);
		break;

	case EEnemyGroupState::Return:
		if (DistToNearestMember <= EngageDistance)     SetGroupState(EEnemyGroupState::Engaged);
		else if (AreAllMembersNearAnchor())            SetGroupState(EEnemyGroupState::Idle);
		break;
	}
}

void AEnemyGroup::NotifyAttackedBy(AActor* Attacker)
{
	// 적 그룹 일원이 피격당하면 어느 상태에서든 즉시 교전. 공격자 축적은 Step B에서 소비자와 함께.
	// どの状態からでも即時交戦へ。
	if (IsDefeated())
	{
		return;
	}
	UE_LOG(LogEnemyGroup, Log, TEXT("被弾通知を受けました。Group=%s, Attacker=%s"),
		*GetName(), *GetNameSafe(Attacker));
	SetGroupState(EEnemyGroupState::Engaged);
}

void AEnemyGroup::SetGroupState(const EEnemyGroupState NewState)
{
	if (GroupState == NewState)
	{
		return;
	}

	const EEnemyGroupState OldState = GroupState;
	GroupState = NewState;

	UE_LOG(LogEnemyGroup, Log, TEXT("グループ状態が遷移しました。Group=%s, %s → %s"),
		*GetName(), *UEnum::GetValueAsString(OldState), *UEnum::GetValueAsString(NewState));

	OnGroupStateChanged.Broadcast(this, OldState, NewState);
}

const APawn* AEnemyGroup::GetSensedPlayerPawn() const
{
	return UGameplayStatics::GetPlayerPawn(this, 0);
}

bool AEnemyGroup::AreAllMembersNearAnchor() const
{
	const FVector Anchor = GetActorLocation();
	for (const AEnemyCharacter* Member : GetAliveMembers())
	{
		if (FVector::Dist(Anchor, Member->GetActorLocation()) > ReturnHomeRadius)
		{
			return false;
		}
	}
	return true;
}