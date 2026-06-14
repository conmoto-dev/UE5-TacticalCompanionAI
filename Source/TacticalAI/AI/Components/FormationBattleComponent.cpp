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
	// 전투 진입 시 슬롯 재배정 예약. 다음 Tick에서 헝가리안 1회.
	// 戦闘進入時に再割当を予約。
	bNeedsReassignment = true;
}

void UFormationBattleComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// [1] anchor 획득. 타겟 소멸 시 파이프라인 정지.
	//     지금 단계에선 모든 그룹이 타겟 기준 anchor를 공유 (Ranged 전용 산정은 RangedSafe Strategy에서).
	const TOptional<FTransform> AnchorOpt = GetFormationAnchor();
	if (!AnchorOpt.IsSet()) return;

	const FTransform& Anchor = AnchorOpt.GetValue();
	const FVector AnchorOrigin = Anchor.GetLocation();
	
	// 리더 위치 1회 캐시. 모든 그룹·멤버가 공유하는 전선 기준.
	// リーダー位置はグループ・メンバー共通の前線基準。ループ外で一度だけ。
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

	// [2] 인지한 적 목록 수집. Manager가 단일 소스 — 그룹 무관이라 루프 밖에서 1회.
	// 知覚した敵リストはManagerが単一ソース。グループ非依存なのでループ外で一度だけ取得。
	TArray<TWeakObjectPtr<const AActor>> PerceivedEnemies;
	if (const APartyManager* Manager = GetOwningPartyManager())
	{
		for (const AActor* Enemy : Manager->GetPerceivedEnemies())
		{
			PerceivedEnemies.Add(Enemy);
		}
	}

	// [3] 역할 분류. 태그 미지정·설정 미등록은 Melee로 폴백.
	//     경고 로그는 재배정 시점에만.
	// 役割分類。未設定はMeleeへフォールバック（警告は再割当時のみ）。
	TMap<FGameplayTag, TArray<APartyCharacter*>> GroupedFollowers;
	for (APartyCharacter* Follower : Followers)
	{
		FGameplayTag Role = Follower->GetCombatRole();
		if (!Role.IsValid() || FindConfigForRole(Role) == nullptr)
		{
			if (bNeedsReassignment)
			{
				UE_LOG(LogTemp, Warning, TEXT("[BattleFormation] %s: 역할 '%s' 설정 없음 -> Melee 폴백"),
					*GetNameSafe(Follower), *Role.ToString());
			}
			Role = CombatRoleTags::Melee;
		}
		GroupedFollowers.FindOrAdd(Role).Add(Follower);
	}

	// [3.5] 그룹 순회 순서를 PlacementPriority로 정렬.
	//       TMap 순회는 순서 불확정(해시 버킷)이라, 배치 순서를 정책으로 보장하려면
	//       명시적으로 정렬해야 한다. "근접이 전선을 먼저 잡고 원거리가 그 뒤"는
	//       게임 디자인 의도 — 낮은 priority가 먼저 자리를 잡는다(이후 점유 누적의 기준).
	// TMapの巡回順は不確定。配置順序は設計意図なのでPlacementPriorityで明示的にソート。
	TArray<FGameplayTag> OrderedRoles;
	GroupedFollowers.GetKeys(OrderedRoles);
	OrderedRoles.Sort([this](const FGameplayTag& A, const FGameplayTag& B)
	{
		const FRoleSlotConfig* ConfigA = FindConfigForRole(A);
		const FRoleSlotConfig* ConfigB = FindConfigForRole(B);

		// 폴백(Melee로 떨어진) 그룹은 Config가 없을 수 있음 → priority 0으로 취급.
		const int32 PriorityA = ConfigA ? ConfigA->PlacementPriority : 0;
		const int32 PriorityB = ConfigB ? ConfigB->PlacementPriority : 0;
		return PriorityA < PriorityB;
	});
	
	// [3.6] 점유 슬롯 누적 버퍼. 그룹 루프 "밖"에 선언하는 게 핵심 —
	//       priority 순으로 먼저 배치된 그룹의 슬롯이 다음 그룹 평가의 입력이 된다.
	//       (먼저 자리 잡은 쪽이 우선권 → 순서가 결과에 반영되지만, 그게 명시적 정책.)
	// 累積バッファはループ外で宣言。先に配置されたグループのスロットが次の入力になる。
	TArray<FVector> OccupiedSlots;
	
	// [4] 그룹별 독립 파이프라인: 슬롯 생성 → 환경보정 → (진입 시) 헝가리안 → push.
	//     순회 순서 = PlacementPriority 오름차순 (위 [3.5]에서 정렬). 비용행렬이 그룹 안에 갇힘.
	// グループ毎の独立パイプライン。巡回順はPlacementPriority昇順。
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

		// [4a] 멤버별로 Context 조립 → 슬롯 1개 생성(월드 좌표) → 환경보정.
		//      순회는 여기(컴포넌트)가 돈다 — 멤버를 아는 주체가 순회하고 Strategy는 1인분만 답한다.
		//      집합형(Arc)은 TotalSlots·SlotIndex로, 개별형(RangedSafe)은 AttackRange·적분포로 계산.
		// メンバーを知る側が巡回し、Strategyは1人分のみ。集合型はN・index、個別型は射程・敵分布で算出。
		const float GroupBaseRadius = ComputeBaseRadius() + Config->RadiusOffset;

		TArray<FVector> WorldSlots;
		WorldSlots.Reserve(GroupMembers.Num());

		for (int32 MemberIndex = 0; MemberIndex < GroupMembers.Num(); ++MemberIndex)
		{
			FSlotGenContext SlotGenContext;
			SlotGenContext.TotalSlots = GroupMembers.Num();
			SlotGenContext.SlotIndex = MemberIndex;
			SlotGenContext.BaseRadius = GroupBaseRadius;
			SlotGenContext.Anchor = Anchor;
			SlotGenContext.AttackRange = GroupMembers[MemberIndex]->GetAttackRange();
			SlotGenContext.RequesterLocation = GroupMembers[MemberIndex]->GetActorLocation();
			SlotGenContext.LeaderLocation = LeaderLocation;
			SlotGenContext.PrimaryTarget = CurrentTarget.Get();
			SlotGenContext.PerceivedEnemies = PerceivedEnemies;
			SlotGenContext.World = GetWorld();
			SlotGenContext.OccupiedSlots = OccupiedSlots; 
			
			const FVector RawSlot = Config->SlotGenerator->GenerateSlot(SlotGenContext);
			WorldSlots.Add(AdjustLocationForEnvironment(RawSlot, AnchorOrigin));
		}

		// [4b] 배정. 정책에 따라 분기:
		//      GroupHungarian (Arc류): 슬롯이 먼저 존재 → 거리 비용으로 멤버↔슬롯 최적 배정.
		//      MemberSpecific (RangedSafe류): 슬롯이 이미 "그 멤버의 것"(RequesterLocation 기준
		//        생성) → 헝가리안 돌리면 자기 기준 계산한 자리가 남에게 가서 의미가 깨짐 → 항등 배정.
		//      배정 방식은 Strategy가 선언, 실행(헝가리안 호출)은 여기 컴포넌트가 한다.
		// 配置方針で分岐。GroupHungarianは距離コストで割当、MemberSpecificは生成順=メンバー順(恒等)。
		// 方針はStrategyが宣言、実行はコンポーネント。
		FRoleGroupRuntime& Runtime = RuntimeGroups.FindOrAdd(Role);
		if (bNeedsReassignment || Runtime.SlotAssignment.Num() != GroupMembers.Num())
		{
			TArray<APartyCharacter*> Assigned;

			if (Config->SlotGenerator->GetAssignmentPolicy() == ESlotAssignmentPolicy::GroupHungarian)
			{
				// 슬롯 i ← 거리 비용으로 정해진 멤버.
				Assigned = SolveSlotAssignment(GroupMembers, WorldSlots);
			}
			else // MemberSpecific
			{
				// 슬롯 i ← 멤버 i (항등). [4a]에서 멤버별로 생성했으므로 순서가 곧 주인.
				// スロットi ← メンバーi（恒等）。[4a]でメンバー別に生成済み。
				Assigned = GroupMembers;
			}

			Runtime.SlotAssignment.Empty(Assigned.Num());
			for (APartyCharacter* Character : Assigned)
			{
				Runtime.SlotAssignment.Add(Character);
			}
		}

		// [4c] 저장된 배정대로 push. SlotAssignment[i] = 슬롯 i에 갈 동료.
		for (int32 i = 0; i < WorldSlots.Num() && i < Runtime.SlotAssignment.Num(); ++i)
		{
			if (APartyCharacter* Character = Runtime.SlotAssignment[i].Get())
			{
				Character->UpdateTargetSlotLocation(WorldSlots[i], false);
			}

			DrawDebugSphere(GetWorld(), WorldSlots[i], 30.f, 12, GroupDebugColor(GroupIndex), false, -1.f, 0, 2.f);
		}

		// 이 그룹의 최종 슬롯을 점유 리스트에 누적 → 다음(낮은 우선순위) 그룹이 피한다.
		// 환경보정+헝가리안까지 끝난 WorldSlots가 "실제로 설 자리" → 이걸 누적.
		// このグループの最終スロットを累積。次グループが回避する。
		OccupiedSlots.Append(WorldSlots);
		++GroupIndex;
	}

	// [5] 재배정 플래그는 모든 그룹 처리 후 일괄 해제.
	// フラグ解除は全グループ処理後。ループ内で消すと後続グループが再割当を逃す。
	bNeedsReassignment = false;

	DrawDebugDirectionalArrow(GetWorld(), AnchorOrigin,
		AnchorOrigin + Anchor.GetRotation().GetForwardVector() * 150.f,
		60.f, FColor::Red, false, -1.f, 0, 3.f);
}


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
	//       지금은 구현체 하나라 조기 추상화 회피 (캐스트 유지).
	if (const ATargetDummy* Dummy = Cast<ATargetDummy>(CurrentTarget.Get()))
	{
		TargetRadius = Dummy->GetEncircleRadius();
	}

	return DesignerBaseRadius + TargetRadius;
}