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

	// [4] 그룹별 독립 파이프라인: 슬롯 생성 → 환경보정 → (진입 시) 헝가리안 → push.
	//     비용행렬이 그룹 안에 갇힘 → 역할 교차 배정 구조적 불가.
	// グループ毎の独立パイプライン。コスト行列が混ざらない＝役割交差割当は不可能。
	int32 GroupIndex = 0;
	for (auto& [Role, GroupMembers] : GroupedFollowers)
	{
		const FRoleSlotConfig* Config = FindConfigForRole(Role);
		if (!Config || !Config->SlotGenerator)
		{
			++GroupIndex;
			continue;
		}

		// [4a] Context 조립 → 슬롯 생성(월드 좌표) → 환경보정. (매 틱 — 타겟이 움직이므로)
		//      반경 해석·anchor 사용 방식은 Strategy 소관. 환경보정은 공통 파이프라인 소관.
		// Context組立→スロット生成（ワールド座標）→環境補正。補正は共通パイプラインの責務。
		FSlotGenContext SlotGenContext;
		SlotGenContext.NumSlots = GroupMembers.Num();
		SlotGenContext.BaseRadius = ComputeBaseRadius() + Config->RadiusOffset;
		SlotGenContext.Anchor = Anchor;
		SlotGenContext.PrimaryTarget = CurrentTarget.Get();
		SlotGenContext.PerceivedEnemies = PerceivedEnemies;
		SlotGenContext.World = GetWorld();

		TArray<FVector> WorldSlots;
		Config->SlotGenerator->GenerateSlots(SlotGenContext, WorldSlots);

		for (FVector& SlotLocation : WorldSlots)
		{
			SlotLocation = AdjustLocationForEnvironment(SlotLocation, AnchorOrigin);
		}

		// [4b] 배정: 진입 시 그룹별 1회 헝가리안. 그룹 인원 변동 시에도 재배정.
		// 進入時のみグループ単位でハンガリアン。人数変動時も再割当。
		FRoleGroupRuntime& Runtime = RuntimeGroups.FindOrAdd(Role);
		if (bNeedsReassignment || Runtime.SlotAssignment.Num() != GroupMembers.Num())
		{
			const TArray<APartyCharacter*> Assigned = SolveSlotAssignment(GroupMembers, WorldSlots);
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