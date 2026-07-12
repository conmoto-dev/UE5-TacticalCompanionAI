#include "Enemies/Group/EnemyGroup.h"
#include "Characters/EnemyCharacter.h"
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
}

void AEnemyGroup::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	
	if (!CVarDebugEnemyGroup.GetValueOnGameThread()) return;
	DrawDebugGroup();
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
}