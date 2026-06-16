#include "AI/Components/FormationBattleComponent.h"
#include "AI/Strategies/SlotGeneratorStrategy.h"
#include "Enemies/TargetDummy.h"   // TODO: 인터페이스로 분리 (구현체 2개째 생기면)
#include "DrawDebugHelpers.h"
#include "NavigationSystem.h"
#include "AI/CombatRoleTags.h"
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

void UFormationBattleComponent::BeginPlay()
{
	Super::BeginPlay();
	CurrentTarget = DebugTargetActor;
}

void UFormationBattleComponent::Activate(bool bReset)
{
	Super::Activate(bReset);
	// 전투 진입 시: 집합형은 헝가리안 재배정 예약, 개별형은 커밋 비움 → 첫 평가에서 전원 초기 배치.
	// 戦闘進入時：集合型は再割当予約、個別型はコミットを空にして全員初期配置。
	bNeedsReassignment = true;
	CommitSnapshots.Reset();
}

void UFormationBattleComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!IsActive()) return;
	
	// [1] anchor 획득. 타겟 소멸 시 파이프라인 정지.
	const TOptional<FTransform> AnchorOpt = GetFormationAnchor();
	if (!AnchorOpt.IsSet()) return;

	const FTransform& Anchor = AnchorOpt.GetValue();
	const FVector AnchorOrigin = Anchor.GetLocation();

	// 리더 위치 1회 캐시. 모든 그룹·멤버가 공유하는 전선 기준(목적지 편향용).
	// リーダー位置はグループ・メンバー共通。ループ外で一度だけ。
	FVector LeaderLocation = FVector::ZeroVector;
	if (const APartyManager* Manager = GetOwningPartyManager())
	{
		if (const APartyCharacter* Leader = Manager->GetLeader())
		{
			LeaderLocation = Leader->GetActorLocation();
		}
	}

	TArray<APartyCharacter*> Followers = GetPartyFollowers();
	if (Followers.Num() == 0) return;

	// [2] 인지한 적 목록. Manager가 단일 소스 — 그룹 무관이라 루프 밖에서 1회.
	// 知覚した敵リストはManagerが単一ソース。ループ外で一度だけ。
	TArray<TWeakObjectPtr<const AActor>> PerceivedEnemies;
	if (const APartyManager* Manager = GetOwningPartyManager())
	{
		for (const AActor* Enemy : Manager->GetPerceivedEnemies())
		{
			PerceivedEnemies.Add(Enemy);
		}
	}

	// [3] 역할 분류. 태그 미지정·설정 미등록은 Melee로 폴백 (경고는 재배정 시점에만).
	TMap<FGameplayTag, TArray<APartyCharacter*>> GroupedFollowers;
	for (APartyCharacter* Follower : Followers)
	{
		FGameplayTag Role = Follower->GetCombatRole();
		if (!Role.IsValid() || FindConfigForRole(Role) == nullptr)
		{
			Role = CombatRoleTags::Melee;
		}
		GroupedFollowers.FindOrAdd(Role).Add(Follower);
	}

	// [3.5] 그룹 순회 순서를 PlacementPriority로 정렬 (TMap 순회는 순서 불확정).
	//       낮은 priority가 먼저 자리를 잡는다(이후 점유 누적의 기준).
	TArray<FGameplayTag> OrderedRoles;
	GroupedFollowers.GetKeys(OrderedRoles);
	OrderedRoles.Sort([this](const FGameplayTag& A, const FGameplayTag& B)
	{
		const FRoleSlotConfig* ConfigA = FindConfigForRole(A);
		const FRoleSlotConfig* ConfigB = FindConfigForRole(B);
		const int32 PriorityA = ConfigA ? ConfigA->PlacementPriority : 0;
		const int32 PriorityB = ConfigB ? ConfigB->PlacementPriority : 0;
		return PriorityA < PriorityB;
	});

	// [3.6] 집합형(Arc) 점유 누적 버퍼. 그룹 루프 밖에 선언 —
	//       먼저 배치된 집합형 그룹의 슬롯이 다음 그룹(개별형 포함) 평가의 입력이 된다.
	//       개별형끼리의 점유는 이 버퍼가 아니라 CommitSnapshots에서 모은다(아래 Gather…).
	TArray<FVector> OccupiedSlots;

	const float NowSeconds = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;

	// [4] 그룹별 파이프라인. 배정 정책으로 갈린다.
	int32 GroupIndex = 0;
	for (const FGameplayTag& Role : OrderedRoles)
	{
		TArray<APartyCharacter*>& GroupMembers = GroupedFollowers[Role];

		const FRoleSlotConfig* Config = FindConfigForRole(Role);
		if (!Config || !Config->SlotGenerator)
		{
			++GroupIndex;
			continue;
		}

		const float GroupBaseRadius = ComputeBaseRadius() + Config->RadiusOffset;
		const bool bMemberSpecific =
			Config->SlotGenerator->GetAssignmentPolicy() == ESlotAssignmentPolicy::MemberSpecific;

		// =================================================================
		// [4-A] 집합형(Arc) 경로 — 기존 그대로. 진입 시 헝가리안 1회, 타겟에 붙어 안정.
		// =================================================================
		if (!bMemberSpecific)
		{
			// [A1] 멤버별 슬롯 생성(월드 좌표) → 환경보정.
			TArray<FVector> WorldSlots;
			WorldSlots.Reserve(GroupMembers.Num());
			for (int32 MemberIndex = 0; MemberIndex < GroupMembers.Num(); ++MemberIndex)
			{
				FSlotGenContext SlotGenContext;
				SlotGenContext.TotalSlots        = GroupMembers.Num();
				SlotGenContext.SlotIndex         = MemberIndex;
				SlotGenContext.BaseRadius        = GroupBaseRadius;
				SlotGenContext.Anchor            = Anchor;
				SlotGenContext.AttackRange       = GroupMembers[MemberIndex]->GetAttackRange();
				SlotGenContext.RequesterLocation = GroupMembers[MemberIndex]->GetActorLocation();
				SlotGenContext.LeaderLocation    = LeaderLocation;
				SlotGenContext.PrimaryTarget     = CurrentTarget.Get();
				SlotGenContext.PerceivedEnemies  = PerceivedEnemies;
				SlotGenContext.World             = GetWorld();
				SlotGenContext.OccupiedSlots     = OccupiedSlots;

				const FVector RawSlot = Config->SlotGenerator->GenerateSlot(SlotGenContext);
				WorldSlots.Add(AdjustLocationForEnvironment(RawSlot, AnchorOrigin));
			}

			// [A2] 배정 (진입 시 또는 인원 변동 시 헝가리안 1회, 이후 유지).
			FRoleGroupRuntime& Runtime = RuntimeGroups.FindOrAdd(Role);
			if (bNeedsReassignment || Runtime.SlotAssignment.Num() != GroupMembers.Num())
			{
				TArray<APartyCharacter*> Assigned = SolveSlotAssignment(GroupMembers, WorldSlots);
				Runtime.SlotAssignment.Empty(Assigned.Num());
				for (APartyCharacter* Character : Assigned)
				{
					Runtime.SlotAssignment.Add(Character);
				}
			}

			// [A3] 저장된 배정대로 push.
			for (int32 i = 0; i < WorldSlots.Num() && i < Runtime.SlotAssignment.Num(); ++i)
			{
				if (APartyCharacter* Character = Runtime.SlotAssignment[i].Get())
				{
					Character->UpdateTargetSlotLocation(WorldSlots[i], false);
				}
				DrawDebugSphere(GetWorld(), WorldSlots[i], 30.f, 12, GroupDebugColor(GroupIndex), false, -1.f, 0, 2.f);
			}

			// 집합형 슬롯을 점유 리스트에 누적 → 다음(낮은 우선순위) 그룹이 피한다.
			OccupiedSlots.Append(WorldSlots);
			++GroupIndex;
			continue;
		}

		// =================================================================
		// [4-B] 개별형(RangedSafe) 경로 — 유닛별 결정 게이트 (커밋/홀드/재배치).
		//       매 틱 재결정 제거 → 이동 중 정지(Bug A)·프레임 회전 트위치(Bug B) 사망.
		// 個別型はユニット別ゲート。毎ティック再決定を排除。
		// =================================================================
		const AActor* Target = CurrentTarget.Get();
		for (APartyCharacter* Member : GroupMembers)
		{
			if (!Member) continue;

			FCommitSnapshot& Snapshot = CommitSnapshots.FindOrAdd(Member);
			const float TimeSinceCommit = Snapshot.bHasCommitted ? (NowSeconds - Snapshot.CommitTime) : 0.f;

			// ── 컨텍스트를 결정 '전에' 조립 (커밋 경로 + 매틱 디버그 양쪽이 쓴다) ──
			FSlotGenContext SlotGenContext;
			SlotGenContext.BaseRadius        = GroupBaseRadius;
			SlotGenContext.Anchor            = Anchor;
			SlotGenContext.AttackRange       = Member->GetAttackRange();
			SlotGenContext.RequesterLocation = Member->GetActorLocation();
			SlotGenContext.LeaderLocation    = LeaderLocation;
			SlotGenContext.PrimaryTarget     = CurrentTarget.Get();
			SlotGenContext.PerceivedEnemies  = PerceivedEnemies;
			SlotGenContext.World             = GetWorld();
			SlotGenContext.OccupiedSlots     = GatherOccupancyForMemberSpecific(Member, OccupiedSlots);

			const bool bNeedsReposition =
				   !Snapshot.bHasCommitted
				|| IsSlotOutOfRange(Snapshot, Target, Member->GetAttackRange())
				|| ShouldFleeThreat(Snapshot, PerceivedEnemies, TimeSinceCommit);

			if (bNeedsReposition)
			{
				CommitReposition(Member, SlotGenContext, *Config, Snapshot); // GenerateSlot이 후보 그림
			}
#if ENABLE_DRAW_DEBUG
			else if (Config->SlotGenerator)
			{
				// 홀드 중에도 매 틱 후보 점수장을 재평가·그리기 (커밋 X, 반환 버림).
				// ゲート＝コミット判断 / デバッグ可視化＝毎ティック独立に再評価。
				Config->SlotGenerator->GenerateSlot(SlotGenContext);
			}
#endif

			DrawDebugSphere(GetWorld(), Snapshot.CommittedSlot, 30.f, 12, GroupDebugColor(GroupIndex), false, -1.f, 0, 2.f);
		}

		// 개별형 그룹의 커밋 슬롯은 OccupiedSlots에 누적하지 않는다 —
		// 다음 그룹 평가는 GatherOccupancyForMemberSpecific가 CommitSnapshots에서 직접 모으므로.
		// (집합형이 먼저(낮은 priority) 배치되는 설계 전제. 역순이 필요해지면 여기서 누적 추가.)
		++GroupIndex;
	}

	// [5] 재배정 플래그는 모든 그룹 처리 후 일괄 해제.
	bNeedsReassignment = false;

	DrawDebugDirectionalArrow(GetWorld(), AnchorOrigin,
		AnchorOrigin + Anchor.GetRotation().GetForwardVector() * 150.f,
		60.f, FColor::Red, false, -1.f, 0, 3.f);
}

// =========================================================================
// 재배치 게이트 구현 (개별형 전용)
// =========================================================================

bool UFormationBattleComponent::IsSlotOutOfRange(
	const FCommitSnapshot& Snapshot, const AActor* Target, float AttackRange) const
{
	// 하드 트리거: 커밋 슬롯에서 타겟을 못 때리면 그 자리는 쓸모없음 → reluctance 무시하고 재배치.
	// ハード：コミットスロットからターゲットを撃てないなら無価値 → reluctance無視で再配置。
	if (!Target) return false; // 타겟 없으면 GetFormationAnchor 단계에서 이미 정지.
	return FVector::Dist2D(Snapshot.CommittedSlot, Target->GetActorLocation()) > AttackRange;
}

bool UFormationBattleComponent::ShouldFleeThreat(
	const FCommitSnapshot& Snapshot,
	const TArray<TWeakObjectPtr<const AActor>>& PerceivedEnemies, float TimeSinceCommit) const
{
	// 검증 대상은 커밋 슬롯(목적지)이지 캐릭터 live 위치가 아님 — 이동 중 노이즈 차단.
	// 가장 가까운 적까지의 거리만 본다("지금 제일 붙은 놈").
	float NearestSq = TNumericLimits<float>::Max();
	for (const TWeakObjectPtr<const AActor>& EnemyPtr : PerceivedEnemies)
	{
		if (const AActor* Enemy = EnemyPtr.Get())
		{
			NearestSq = FMath::Min(NearestSq, FVector::DistSquared2D(Snapshot.CommittedSlot, Enemy->GetActorLocation()));
		}
	}
	if (NearestSq == TNumericLimits<float>::Max()) return false; // 적 없음.

	const float Nearest = FMath::Sqrt(NearestSq);

	// [1] 비상: 코앞이면 reluctance 무시하고 즉시 회피.
	// 緊急：目前ならreluctance無視で即時回避。
	if (Nearest < EmergencyFleeRadius) return true;

	// [2] reluctance 게이트: 막 배치한 직후엔 회피를 억제하고, 시간이 지날수록 회피 반경을 0→full로 연다.
	//     이게 "이동-한대-쫓겨서 또 이동"의 무한 트위치를 "회피 주기당 한 번"으로 줄이는 핵심.
	//     (끝까지 쫓기면 어떻게 = 상위 행동 레이어/StateTree 몫. 여기선 댐핑만.)
	// reluctanceゲート：配置直後は回避抑制、時間で回避半径を0→fullに開く（無限カイティング防止）。
	const float SettleFactor = FMath::Clamp(TimeSinceCommit / FMath::Max(ThreatReluctanceTime, KINDA_SMALL_NUMBER), 0.f, 1.f);
	const float EffectiveFleeRadius = ThreatOnTopRadius * SettleFactor;

	return Nearest < EffectiveFleeRadius;
}

void UFormationBattleComponent::CommitReposition(
	APartyCharacter* Member, const FSlotGenContext& Context,
	const FRoleSlotConfig& Config, FCommitSnapshot& OutSnapshot)
{
	// [1] 이 순간의 월드로 슬롯 1개 생성. 플레이어 방향은 여기(Strategy 내부 목적지 편향)서만 반영 — 상시 추적 아님.
	// プレイヤー方向はここで一度だけ反映（常時追跡ではない）。
	const FVector RawSlot = Config.SlotGenerator->GenerateSlot(Context);

	// [2] 환경보정(NavMesh·벽·슬로프). 부모 공통 파이프라인 재사용.
	const FVector AdjustedSlot = AdjustLocationForEnvironment(RawSlot, Context.Anchor.GetLocation());

	// [3] 커밋 갱신 + locomotion에 전달. 이후 트리거 전까지 다시 안 건드린다(커밋 잠금).
	OutSnapshot.CommittedSlot = AdjustedSlot;
	OutSnapshot.CommitTime    = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
	OutSnapshot.bHasCommitted = true;
	Member->UpdateTargetSlotLocation(AdjustedSlot, false);

	// [다음 조각] TargetAtCommit·EnemyCenterAtCommit·점수 스냅샷(T2 드리프트), staleness 타이머 여기.
}

TArray<FVector> UFormationBattleComponent::GatherOccupancyForMemberSpecific(
	const APartyCharacter* Self, const TArray<FVector>& GroupOccupied) const
{
	// 이미 배치된 집합형 그룹 슬롯 + 나를 제외한 다른 유닛들의 "현재 커밋 슬롯".
	// 매 틱 재생성한 임시 슬롯이 아니라 남들이 실제로 향하는 확정 위치를 읽으므로
	// 같은 그룹 내 동시 생성 충돌이 구조적으로 없다(Bug 1 수정).
	// 他ユニットの実コミット位置を読むため同時生成衝突なし（Bug 1修正）。
	TArray<FVector> Out = GroupOccupied;
	for (const TPair<TWeakObjectPtr<APartyCharacter>, FCommitSnapshot>& Pair : CommitSnapshots)
	{
		if (Pair.Value.bHasCommitted && Pair.Key.IsValid() && Pair.Key.Get() != Self)
		{
			Out.Add(Pair.Value.CommittedSlot);
		}
	}
	return Out;
}

// =========================================================================
// 헬퍼
// =========================================================================

const FRoleSlotConfig* UFormationBattleComponent::FindConfigForRole(const FGameplayTag& Role) const
{
	// 설정 엔트리는 역할당 2~4개 수준 — 선형 탐색으로 충분.
	return RoleSlotConfigs.FindByPredicate([&Role](const FRoleSlotConfig& Config)
	{
		return Config.Role == Role;
	});
}

TOptional<FTransform> UFormationBattleComponent::GetFormationAnchor() const
{
	// 약참조 유효성 = 타겟 살아있음. 죽었으면 빈 Optional → 호출부가 파이프라인 정지.
	if (const AActor* Target = CurrentTarget.Get())
	{
		return Target->GetActorTransform();
	}
	return TOptional<FTransform>();
}

float UFormationBattleComponent::ComputeBaseRadius() const
{
	float TargetRadius = 0.f;

	// TODO: ATargetDummy 직접 캐스트 = 결합. 진짜 적 추가 시 IEncircleTarget 인터페이스로 분리.
	if (const ATargetDummy* Dummy = Cast<ATargetDummy>(CurrentTarget.Get()))
	{
		TargetRadius = Dummy->GetEncircleRadius();
	}

	return DesignerBaseRadius + TargetRadius;
}