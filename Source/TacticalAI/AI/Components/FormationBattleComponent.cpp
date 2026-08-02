#include "AI/Components/FormationBattleComponent.h"
#include "AI/Strategies/SlotGeneratorStrategy.h"
#include "AI/Targeting/Targetable.h"
#include "DrawDebugHelpers.h"
#include "NavigationSystem.h"
#include "AI/CombatRoleTags.h"
#include "AI/Targeting/TargetSelectorComponent.h"
#include "Party/PartyManager.h"
#include "Characters/PartyCharacter.h"


// 그룹 구분용 디버그 색상. 그룹 순회 인덱스로 팔레트 순환 — 디버그 전용.
static FColor GroupDebugColor(int32 GroupIndex)
{
	static const FColor Palette[] = { FColor::Cyan, FColor::Orange, FColor::Green, FColor::Yellow };
	return Palette[GroupIndex % UE_ARRAY_COUNT(Palette)];
}

UFormationBattleComponent::UFormationBattleComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UFormationBattleComponent::Activate(bool bReset)
{
	Super::Activate(bReset);
	// 전투 진입 시 커밋 비움 → 첫 평가에서 전원 초기 배치.
	// 戦闘進入時：コミットを空にして全員初期配置。
	CommitSnapshots.Reset();
}

void UFormationBattleComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!IsActive()) return;

	TArray<APartyCharacter*> Followers = GetPartyFollowers();
	if (Followers.Num() == 0) return;

	// [1] 리더 위치 1회 캐시. 전 멤버 공유하는 목적지 편향 입력 — 루프 밖에서 한 번만.
	// リーダー位置はメンバー共通。ループ外で一度だけ。
	FVector LeaderLocation = FVector::ZeroVector;
	if (const APartyManager* Manager = GetOwningPartyManager())
	{
		if (const APartyCharacter* Leader = Manager->GetLeader())
		{
			LeaderLocation = Leader->GetActorLocation();
		}
	}

	// [2] 인지한 적 목록. Manager가 단일 소스 — 멤버 무관이라 루프 밖에서 1회.
	// 知覚した敵リストはManagerが単一ソース。ループ外で一度だけ。
	TArray<TWeakObjectPtr<const AActor>> PerceivedEnemies;
	if (const APartyManager* Manager = GetOwningPartyManager())
	{
		for (const AActor* Enemy : Manager->GetPerceivedEnemies())
		{
			PerceivedEnemies.Add(Enemy);
		}
	}

	// [3] 같은 틱 첫 커밋 순서: PlacementPriority 낮은 순 (StableSort — 동순위 순서 틱 간 고정).
	//     전투 진입 첫 틱에 전원 동시 커밋할 때 뒤 멤버가 앞 멤버의 커밋을 점유로 보게 하는 장치.
	//     운용 중엔 타겟 재평가의 어긋난 박동 덕에 동시 커밋이 드물어, 이 순서는 진입 순간에만 의미.
	// 同ティック初回コミットの順序決定。運用中はずれた鼓動により同時コミットは稀。
	Followers.StableSort([this](const APartyCharacter& A, const APartyCharacter& B)
	{
		auto GetPriority = [this](const APartyCharacter& Character) -> int32
		{
			FGameplayTag Role = Character.GetCombatRole();
			if (!Role.IsValid() || FindConfigForRole(Role) == nullptr)
			{
				Role = CombatRoleTags::Melee;
			}
			const FRoleSlotConfig* Config = FindConfigForRole(Role);
			return Config ? Config->PlacementPriority : 0;
		};
		return GetPriority(A) < GetPriority(B);
	});

	const float NowSeconds = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;

	// [4] 멤버별 게이트 파이프라인. 그룹 없음 — 전 Strategy 개별형(1인→1슬롯 항등 배정).
	//     슬롯이 인원 수 N에 종속되지 않으므로, 한 명의 타겟 변동이 남은 멤버로 전파되지 않는다.
	// メンバー別ゲート。グループ無し。1人のターゲット変動が他へ伝播しない。
	int32 MemberIndex = 0;
	for (APartyCharacter* Member : Followers)
	{
		if (!Member) continue;
		const int32 DebugIndex = MemberIndex++;

		// [4-1] 역할 → 설정. 태그 미지정·미등록은 Melee 폴백.
		FGameplayTag Role = Member->GetCombatRole();
		if (!Role.IsValid() || FindConfigForRole(Role) == nullptr)
		{
			Role = CombatRoleTags::Melee;
		}
		const FRoleSlotConfig* Config = FindConfigForRole(Role);
		if (!Config || !Config->SlotGenerator) continue;

		// [4-2] 개별 타겟 = 멤버 셀렉터의 커밋 결과. 없으면 skip(배치 갱신 정지).
		//       무타겟 시의 행동(복귀·대기)은 행동 레이어 몫 — 여기선 아무것도 안 민다.
		// 個別ターゲット＝セレクタのコミット結果。無ければskip（行動は行動レイヤーの責務）。
		const AActor* MemberTarget = nullptr;
		if (const UTargetSelectorComponent* Selector = Member->GetTargetSelector())
		{
			MemberTarget = Selector->GetCurrentTarget();
		}
		if (!MemberTarget) continue;

		FCommitSnapshot& Snapshot = CommitSnapshots.FindOrAdd(Member);
		const float TimeSinceCommit = Snapshot.bHasCommitted ? (NowSeconds - Snapshot.CommitTime) : 0.f;

		// [4-3] 컨텍스트 조립 (커밋 경로 + 매틱 디버그 양쪽이 쓴다).
		FSlotGenContext SlotGenContext;
		SlotGenContext.BaseRadius        = ComputeBaseRadius(MemberTarget) + Config->RadiusOffset;
		SlotGenContext.AttackRange       = Member->GetAttackRange();
		SlotGenContext.RequesterLocation = Member->GetActorLocation();
		SlotGenContext.LeaderLocation    = LeaderLocation;
		SlotGenContext.PrimaryTarget     = MemberTarget;
		SlotGenContext.PerceivedEnemies  = PerceivedEnemies;
		SlotGenContext.World             = GetWorld();
		SlotGenContext.OccupiedSlots     = GatherOccupiedSlots(Member);

		// [4-4] 게이트. 첫 커밋(생성 방식 무관 = 컴포넌트 소관) / 유효성(생성의 따름정리 = Strategy 소관).
		//       타겟이 바뀌면 새 타겟 기준 판정이 스스로 무효화를 만든다 — 별도 "타겟 변경 트리거" 불필요.
		// ゲート。ターゲット変更は新ターゲット基準の判定が自然に無効化する。
		const bool bNeedsReposition =
			   !Snapshot.bHasCommitted
			|| Config->SlotGenerator->ShouldReposition(SlotGenContext, Snapshot.CommittedSlot, TimeSinceCommit);

		if (bNeedsReposition)
		{
			CommitReposition(Member, SlotGenContext, *Config, Snapshot);
		}
#if ENABLE_DRAW_DEBUG
		else
		{
			// 홀드 중에도 매 틱 후보 점수장을 재평가·그리기 (커밋 X, 반환 버림).
			// ゲート＝コミット判断 / デバッグ可視化＝毎ティック独立に再評価。
			Config->SlotGenerator->GenerateSlot(SlotGenContext);
		}
#endif

		DrawDebugSphere(GetWorld(), Snapshot.CommittedSlot, 30.f, 12, GroupDebugColor(DebugIndex), false, -1.f, 0, 2.f);
	}
}

float UFormationBattleComponent::ComputeBaseRadius(const AActor* TargetActor) const
{
	// 단일 CurrentTarget 버전을 "그 멤버의 타겟" 파라미터로 일반화. ITargetable 조회는 기존 방식 그대로 —
	// ※ 게터가 BlueprintNativeEvent면 기존 본문의 Execute_ 호출 스타일을 유지할 것.
	// 単一ターゲット版をメンバー別ターゲットへ一般化。照会方式は既存のまま。
	if (TargetActor && TargetActor->Implements<UTargetable>())
	{
		return ITargetable::Execute_GetEncircleRadius(const_cast<AActor*>(TargetActor));
	}
	
	return 0.f;
}

TArray<FVector> UFormationBattleComponent::GatherOccupiedSlots(const APartyCharacter* Requester) const
{
	// 점유 = 다른 멤버들의 "커밋된" 슬롯 (live 위치 아님 — 이동 중 노이즈 차단, ADR-0003과 동일 기준).
	// 소멸한 weak 키·미커밋 스냅샷은 제외. 요청자 자신도 제외 (자기 자리를 자기가 피하면 안 됨).
	// 占有＝他メンバーの「コミット済み」スロット。無効キー・未コミット・自分自身は除外。
	TArray<FVector> Result;
	Result.Reserve(CommitSnapshots.Num());
	for (const auto& Pair : CommitSnapshots)
	{
		const APartyCharacter* Other = Pair.Key.Get();
		if (!Other || Other == Requester) continue;
		if (!Pair.Value.bHasCommitted) continue;

		Result.Add(Pair.Value.CommittedSlot);
	}
	return Result;
}

// =========================================================================
// 재배치 게이트
// =========================================================================
void UFormationBattleComponent::CommitReposition(
	APartyCharacter* Member, const FSlotGenContext& Context,
	const FRoleSlotConfig& Config, FCommitSnapshot& OutSnapshot)
{
	// [1] 이 순간의 월드로 슬롯 1개 생성. 플레이어 방향은 Strategy 내부 목적지 편향으로만 반영 — 상시 추적 아님.
	// プレイヤー方向はここで一度だけ反映（常時追跡ではない）。
	const FVector RawSlot = Config.SlotGenerator->GenerateSlot(Context);

	// [2] 환경보정(NavMesh·벽·슬로프). 끌어당김 기준점.
	// 環境補正。引き寄せ基準点＝そのメンバーのターゲット。
	const AActor* Target = Context.PrimaryTarget.Get();
	const FVector PullOrigin = Target ? Target->GetActorLocation() : Context.RequesterLocation;
	const FVector AdjustedSlot = AdjustLocationForEnvironment(RawSlot, PullOrigin);

	// [3] 커밋: 스냅샷 갱신 + locomotion 전달.
	// コミット：スナップショット更新＋locomotionへ伝達。
	OutSnapshot.CommittedSlot = AdjustedSlot;
	OutSnapshot.CommitTime    = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
	OutSnapshot.bHasCommitted = true;

	Member->UpdateTargetSlotLocation(AdjustedSlot, false);
}

// ===========
// 헬퍼
// ===========
const FRoleSlotConfig* UFormationBattleComponent::FindConfigForRole(const FGameplayTag& Role) const
{
	// 설정 엔트리는 역할당 2~4개 수준 — 선형 탐색으로 충분.
	return RoleSlotConfigs.FindByPredicate([&Role](const FRoleSlotConfig& Config)
	{
		return Config.Role == Role;
	});
}