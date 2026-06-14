# ADR-0003: 전투 행동 레이어링 — Home Slot, Movement Intent, 인터럽트 우선순위

* **Status**: Proposed
* **Date**: 2026-06-14
* **관련**:

    * ADR-0001: Traversal 결정 권한 A/B 분리
    * ADR-0002: 슬롯 생성 추상화

---

## Context

전투 중 동료 캐릭터의 최종 위치는 단순히 “슬롯 좌표”만으로 결정되지 않는다.

슬롯 배치 레이어는 동료가 기준 삼을 위치를 제공한다. 그러나 실제 전투에서는 그 위에 다음과 같은 행동들이 겹친다.

* 슬롯 기준 위치로 재배치
* 공격/스킬 모션 중 자연스럽게 슬롯에서 이탈
* 공격 후 즉시 복귀할지, 계속 교전할지 결정
* 슬롯 주변에서 횡이동 또는 미세 maneuver
* 원거리 캐릭터의 압박 재배치
* 보스 전체공격 회피
* Traversal 중 이동권 잠금
* 피격, 경직, 다운, 부활 지원 같은 강제 행동

따라서 핵심 질문은 세 가지다.

1. **위치와 행동을 어떻게 분리할 것인가?**
2. **여러 시스템이 서로 다른 이동 목표를 요구할 때, 최종 이동 명령은 누가 결정하는가?**
3. **공격, 회피, traversal, 슬롯 복귀 같은 행동의 우선순위와 인터럽트 가능성은 어떻게 다룰 것인가?**

이 프로젝트의 전투 정책은 완전한 단일 모델이 아니라 하이브리드다.

* 근거리 캐릭터는 Endfield식 target-centric hard slot에 가깝다.
* 원거리 캐릭터는 Relink식 soft/personal positioning에 가깝다.
* 전투 행동은 각 동료가 자기 상태에 따라 공격, 스킬, 회피를 수행하는 자율 전투를 따른다.
* 슬롯은 “정확히 서야 하는 점”이 아니라, 행동의 기준점인 **home**으로 취급한다.

---

## Decision Summary

이 ADR의 핵심 결정은 다음과 같다.

1. Formation은 최종 이동 명령자가 아니라 **Home Provider**다.
2. StateTree는 현재 행동 상태와 인터럽트 우선순위를 관리한다.
3. 실제 MoveTo 또는 이동 명령은 하나의 **MovementIntentResolver**가 결정한다.
4. 각 시스템은 직접 이동 명령을 내리지 않고, MovementIntent를 제출한다.
5. 공격/스킬/회피 중 슬롯을 삭제하거나 옮기지 않는다. Home은 유지하고, 추적만 일시 양보한다.
6. 보스 전체공격처럼 파티 단위 조율이 필요한 이벤트는 개별 StateTree만으로 처리하지 않고, Squad/Event 레벨에서 위치 배정을 조율한다.
7. 우선순위와 실제 캔슬 가능성은 분리한다.

---

## Layer Separation

전투 위치 결정은 세 층으로 나눈다.

```text
┌──────────────────────────────────────────────────────────────┐
│ Layer 1 — Home / Position Reference                           │
│   “이 캐릭터가 기준 삼을 전투 위치는 어디인가?”                  │
│   담당: FormationBattleComponent, SlotGeneratorStrategy        │
│   산출: HomeLocation / SlotAnchor                              │
├──────────────────────────────────────────────────────────────┤
│ Layer 2 — Combat State / Action Policy                         │
│   “지금 무엇을 하고 있는가?”                                    │
│   담당: StateTree, Combat Action Layer                         │
│   산출: Attack, Skill, Evade, Idle, Reposition 등의 상태          │
├──────────────────────────────────────────────────────────────┤
│ Layer 3 — Final Movement Resolution                            │
│   “이번 프레임에 실제로 어디로 이동해야 하는가?”                  │
│   담당: MovementIntentResolver                                 │
│   산출: FinalMoveTarget / MovementLock / NoMove                 │
└──────────────────────────────────────────────────────────────┘
```

Formation은 위치 기준점을 제공할 뿐이다.
StateTree는 행동 상태를 결정한다.
MovementIntentResolver는 여러 이동 의도 중 실제 실행할 하나를 선택한다.

---

## D0. 최종 이동 명령은 단일 MovementIntentResolver가 결정한다

여러 시스템이 동시에 `MoveToLocation`을 호출하는 구조를 금지한다.

Formation, StateTree, Traversal, BossEvade, RangedRelocation 등은 직접 이동 명령을 내리지 않고, 각자의 `MovementIntent`를 제출한다.

최종적으로 하나의 `MovementIntentResolver`가 우선순위, 인터럽트 가능성, 만료 시간, 이동 잠금 여부를 평가해 캐릭터의 최종 이동 목표를 결정한다.

```cpp
enum class EMovementIntentType : uint8
{
    None,

    HomeSlot,          // 슬롯/home으로 수렴
    Maneuver,          // 슬롯 주변 미세 이동
    RangedRelocation,  // 원거리 압박 재배치

    AttackCommit,      // 공격 모션/공격 위치가 이동권 소유
    SkillCommit,       // 스킬 모션/스킬 위치가 이동권 소유

    Evade,             // 회피 이동
    BossEvade,         // 보스 패턴 회피

    Traversal,         // 점프/낙하/지형 극복
    ForcedEvent,       // 링크어택, 부활 지원, 강제 연출 등
    Staggered          // 피격/경직/다운
};

struct FMovementIntent
{
    EMovementIntentType Type = EMovementIntentType::None;

    FVector TargetLocation = FVector::ZeroVector;

    int32 Priority = 0;

    bool bLocksMovement = false;
    bool bReleasesHomeSlot = false;
    bool bCanBeInterrupted = true;

    float AcceptanceRadius = 80.f;
    float ValidUntilTime = 0.f;

    FName DebugReason;
};
```

이 원칙의 목적은 명확하다.

```text
여러 시스템이 서로 MoveTo를 덮어쓰는 상황을 막는다.
각 시스템은 “내가 원하는 이동 의도”만 제출한다.
최종 실행권은 Resolver 하나가 가진다.
```

---

## D1. Formation은 Home Provider다

Formation 레이어는 캐릭터의 현재 전투 기준점인 `HomeLocation`을 계산한다.

기존의 슬롯 좌표는 더 이상 “항상 정확히 서야 하는 점”이 아니다.
슬롯은 다음 의미를 가진다.

```text
HomeLocation:
  캐릭터가 할 일이 없거나,
  너무 멀어졌거나,
  전투 상태가 안정되었을 때 수렴할 기준점
```

근거리와 원거리에서 Home의 의미는 다르다.

| 역할          | Home 의미                                 |
| ----------- | --------------------------------------- |
| Melee       | Target-centric hard combat slot         |
| Ranged      | Soft combat position / 사거리·안전·시야 기준점    |
| Support     | Leader 또는 party-relative support pocket |
| Skill/Event | 일시적 override pose, Home과 별개             |

근거리는 보스 주변의 점유 슬롯을 적극적으로 사용한다.
원거리는 정확한 대형 슬롯이 아니라, 유효한 전투 위치를 느슨하게 유지한다.

---

## D2. StateTree는 행동 상태를 결정한다

StateTree는 캐릭터가 현재 어떤 행동 모드에 있는지 결정한다.

예시 상태:

| 상태               | Home과의 관계                  |
| ---------------- | -------------------------- |
| CombatIdle       | Home을 기준으로 대기 또는 미세 이동     |
| Reposition       | Home 또는 soft position으로 이동 |
| AttackReady      | 사거리와 방향을 맞추며 공격 준비         |
| AttackCommit     | 공격 모션이 이동권 소유. Home 추적 양보  |
| SkillCommit      | 스킬이 요구하는 위치/모션 우선          |
| Evade            | Home 추적 중단. 회피 위치 우선       |
| BossEvade        | 파티/이벤트가 지정한 안전 위치 우선       |
| Traversal        | 이동 실행권 잠금. 공중/도약 중 간섭 금지   |
| Staggered/Downed | 모든 일반 이동 intent 무시         |

StateTree는 직접 `MoveTo`를 호출하지 않는다.
대신 현재 상태에 맞는 `MovementIntent`를 제출한다.

---

## D3. 슬롯 해제는 “Home 삭제”가 아니라 “Home 추적 양보”다

공격, 스킬, 회피 중에 슬롯 좌표 자체를 삭제하거나 이동시키지 않는다.

```text
잘못된 방식:
  공격 중이니까 슬롯을 비활성화한다.
  스킬 위치로 슬롯 자체를 옮긴다.

채택 방식:
  Home은 계속 계산된다.
  다만 현재 행동 intent가 HomeSlot intent보다 높은 우선순위를 가진다.
```

즉, 공격 중 캐릭터가 슬롯에서 벗어나도 Formation은 계속 Home을 제공한다.
공격이 끝나고 더 높은 우선순위 intent가 사라지면, Resolver가 다시 HomeSlot 또는 Return intent를 선택할 수 있다.

이 방식은 평시 Follow 시스템의 Yield, TacticalTraversalComponent의 “진행 중 슬롯 push 스킵” 패턴을 일반화한 것이다.
다만 장기적으로는 `if (IsAttacking) continue;` 같은 예외문을 Formation에 계속 추가하기보다, MovementIntentResolver의 우선순위 선택으로 통일한다.

---

## D4. Priority와 Interruptibility는 분리한다

높은 우선순위 행동이라고 해서 항상 즉시 현재 행동을 끊을 수 있는 것은 아니다.

```text
Priority:
  무엇이 더 중요한가?

Interruptibility:
  현재 행동을 실제로 끊을 수 있는가?
```

예를 들어 보스 즉사 패턴 회피는 P0 우선순위지만, 캐릭터가 이미 공중 traversal 중이면 즉시 회피 이동이 불가능할 수 있다.
또한 강한 공격 모션 중에는 캔슬 가능 프레임에서만 회피가 허용될 수 있다.

따라서 Resolver는 다음을 함께 본다.

* 새 intent의 priority
* 현재 intent의 interruptibility
* 현재 액션의 cancel window
* movement lock 여부
* forced event 여부
* emergency override 여부

예시 우선순위:

```text
[P0] Forced / Survival
  BossEvade, 즉사 패턴, 강제 회피, 피격/다운

[P1] Traversal / Movement Lock
  점프, 낙하, 공중 이동, 지형 극복

[P2] Skill / Attack Commit
  스킬 발동, 공격 모션, 링크/연계 이벤트

[P3] Tactical Reposition
  원거리 압박 재배치, 위치 조정, maneuver

[P4] Home Slot
  슬롯/home 복귀, 일반 대기 위치 수렴
```

단, P0가 들어와도 현재 상태가 `Uninterruptible`이면 즉시 이동하지 못할 수 있다.
이 경우 다음 중 하나로 처리한다.

* 회피 예약
* 방어/가드 전환
* 피해 감수
* 슈퍼아머
* 피격 리액션
* 다음 cancel window에서 회피

---

## D5. 보스 전체공격은 개별 StateTree만으로 위치를 고르지 않는다

보스 전체공격처럼 여러 동료가 동시에 회피해야 하는 상황은 개별 캐릭터가 독립적으로 안전 위치를 고르면 문제가 생길 수 있다.

예상 문제:

```text
동료들이 같은 안전지점으로 몰림
좁은 입구에서 서로 낑김
플레이어 이동을 방해함
회피 경로가 겹침
```

따라서 보스 전체공격 이벤트는 두 단계로 나눈다.

```text
1. Squad/Event level
   - 위험 이벤트 감지
   - safe zone 후보 생성
   - 파티원별 회피 위치 할당
   - EvadeIntent 발행

2. Individual StateTree
   - 현재 행동을 끊을 수 있는지 판단
   - Evade/BossEvade 상태로 전이
   - 할당된 위치로 이동 intent 제출
```

Formation 레이어는 보스 패턴을 알 필요가 없다.
그러나 개별 StateTree도 safe position allocation을 독자적으로 해결하지 않는다.

이 책임은 추후 `BattleEventCoordinator`, `SquadEvadeCoordinator`, 또는 `PartyCombatDirector` 같은 별도 레이어가 담당한다.

---

## D6. Return Policy를 명시한다

공격, 스킬, 회피 후 Home으로 복귀하는 방식은 행동별로 다르다.

공격이 끝났다고 항상 즉시 Home으로 돌아가면 다음 문제가 생긴다.

```text
공격으로 앞으로 나감
공격 종료
즉시 슬롯으로 후퇴
다시 공격하러 전진
반복
```

이는 전투를 어색하게 만든다.

따라서 행동별 Return Policy를 둔다.

```cpp
enum class EReturnToHomePolicy : uint8
{
    Immediate,          // 행동 종료 즉시 Home 복귀
    WhenIdle,           // 다음 행동이 없을 때만 복귀
    IfTooFarFromHome,   // Home 허용 반경을 벗어났을 때만 복귀
    BlendTowardHome,    // 행동 중 이동 목표에 Home 방향을 약하게 섞음
    NeverUntilRetarget  // 현재 교전 중에는 복귀하지 않음
};
```

역할별 기본값:

| 역할/행동                      | Return Policy                 |
| -------------------------- | ----------------------------- |
| Melee basic attack         | IfTooFarFromHome              |
| Melee heavy skill          | WhenIdle or IfTooFarFromHome  |
| Ranged attack              | BlendTowardHome or WhenIdle   |
| Ranged pressure relocation | IfTooFarFromHome              |
| BossEvade                  | WhenIdle after danger cleared |
| Traversal                  | Immediate or IfTooFarFromHome |
| ForcedEvent                | 이벤트 종료 후 상황에 따라 결정            |

Home 복귀는 가장 낮은 우선순위 intent다.
다른 공격/회피/스킬 intent가 있으면 Home 복귀는 실행되지 않는다.

---

## D7. Maneuver는 독립 State가 아니라 Modifier로 시작한다

슬롯 주변 횡이동이나 미세 거리 조정은 처음부터 독립 State로 만들지 않는다.

초기 구현에서는 다음처럼 처리한다.

```text
CombatIdle:
  Home 근처에서 약한 strafe 가능

AttackReady:
  사거리와 시야를 맞추기 위한 미세 이동 가능

AttackCommit:
  공격 모션이 이동권 소유

Reposition:
  Home 또는 soft position으로 이동
```

즉, Maneuver는 처음에는 `HomeSlot` 또는 `AttackReady` intent의 target을 약간 변형하는 modifier로 취급한다.
나중에 행동 종류가 늘어나고 독립 전술 가치가 생기면 별도 State로 승격한다.

---

## D8. 원거리 재배치는 포지션 점수만으로 자동 카이팅하지 않는다

원거리 캐릭터의 안전 위치 계산은 필요하다.
하지만 “위험하면 항상 더 안전한 위치로 이동”을 점수만으로 처리하면 무한 카이팅이 발생할 수 있다.

따라서 원거리 재배치는 두 단계로 나눈다.

```text
1. Relocation Gate
   지금 위치를 버릴 만큼 나쁜가?

2. Position Evaluation
   움직인다면 어디가 좋은가?
```

Relocation Gate는 다음을 고려한다.

* 가장 가까운 적 1~3명의 압박
* personal space 침범
* 현재 위치에서 공격 가능 여부
* line of sight
* 최근 피격
* 이동 후 경과 시간
* emergency threshold
* soft relocation urgency

재배치 점수는 다음과 같이 intent에 따라 가중치를 바꾼다.

```text
InitialPlacement:
  Range / FriendlySide / Safety 중심
  AllyCover 낮음
  Stickiness 없음

Maintain:
  Current position stickiness 강함
  작은 개선으로는 이동하지 않음

PressureRelocation:
  Top-K Threat / AllyCover / FriendlySide 강화
  Stickiness 약화

EmergencyDisengage:
  Immediate danger 최우선
  cooldown과 hold time 일부 무시
```

원거리 AI의 기본 철학:

```text
좋은 자리가 있다고 항상 이동하지 않는다.
현재 위치가 나빠졌을 때만 이동한다.
위험 이동은 허용하되, 최적화 이동은 억제한다.
```

---

## MovementIntent Resolution 예시

각 시스템은 매 tick 또는 상태 변경 시 intent를 제출한다.

```text
Formation:
  HomeSlot intent

StateTree CombatIdle:
  HomeSlot or Maneuver intent

StateTree AttackCommit:
  AttackCommit intent

RangedRelocationPolicy:
  RangedRelocation intent

BossEventCoordinator:
  BossEvade intent

TraversalComponent:
  Traversal intent / movement lock

HitReaction:
  Staggered intent
```

Resolver는 다음 순서로 처리한다.

```text
1. 만료된 intent 제거
2. 현재 movement lock 확인
3. 현재 intent의 interruptibility 확인
4. 새 intent priority 비교
5. cancel window 확인
6. tie-breaker 적용
7. 최종 intent 선택
8. PartyCharacter에 단일 FinalMoveTarget 전달
```

Tie-breaker 예시:

```text
- 같은 priority면 forced event 우선
- 같은 priority면 더 최근 emergency event 우선
- 같은 priority면 현재 intent 유지
- HomeSlot은 기본 fallback
```

---

## Rejected Alternatives

### A. Formation 레이어가 행동까지 처리

FormationBattleComponent가 슬롯뿐 아니라 공격, 회피, 스킬, 보스 패턴까지 판단하는 방식.

**기각.**

위치 계산과 전투 전술이 섞여 컴포넌트가 비대해진다.
인터럽트 우선순위, 캔슬 가능성, 공격 모션 같은 액션 레이어 지식이 Formation에 침투한다.

---

### B. 각 시스템이 직접 MoveTo를 호출

Formation, StateTree, BossEvade, Traversal이 각자 필요할 때 `MoveToLocation`을 호출하는 방식.

**기각.**

이동 명령 출처가 여러 개가 되어 서로 덮어쓰기 쉽다.
디버그 시 “왜 이 캐릭터가 이 위치로 갔는가”를 추적하기 어려워진다.

MovementIntentResolver로 단일화한다.

---

### C. 공격/스킬 중 슬롯 좌표 자체를 이동하거나 삭제

공격 시 슬롯을 스킬 위치로 옮기거나, 슬롯을 비활성화하는 방식.

**기각.**

Home 기준점이 사라져 행동 종료 후 복귀가 불안정해진다.
슬롯은 계속 계산하되, 높은 우선순위 행동 intent가 HomeSlot intent를 덮는다.

---

### D. 인터럽트를 Formation이 처리

보스 전체공격 회피를 FormationBattleComponent가 받아서 슬롯을 안전 위치로 재배치하는 방식.

**기각.**

보스 패턴은 위치 계산이 아니라 전투 행동 인터럽트다.
Formation은 보스 패턴을 몰라야 한다.

다만 safe position allocation은 개별 StateTree만으로 처리하지 않고, Squad/Event 레벨에서 조율할 수 있다.

---

### E. 원거리 카이팅을 RangedSafe 점수만으로 구현

안전 점수가 높은 위치로 계속 이동하게 만드는 방식.

**기각.**

빠른 적이나 끈질긴 어그로를 상대로 무한 카이팅이 발생할 수 있다.
원거리 재배치는 Relocation Gate와 Position Evaluation으로 분리한다.

---

## Open Questions

### OQ-1. MovementIntentResolver의 소속

Resolver를 어디에 둘 것인가?

후보:

* PartyCharacter 내부
* 별도 `UMovementIntentComponent`
* AIController
* StateTree Task 내부
* Formation/Combat system과 독립된 `UCombatMovementComponent`

현재 추천은 `UCombatMovementIntentComponent` 또는 `UCompanionMovementCoordinatorComponent` 같은 독립 컴포넌트다.
PartyCharacter는 최종 명령 실행자에 가깝고, Resolver는 정책/중재자에 가깝기 때문이다.

---

### OQ-2. StateTree와 Resolver 인터페이스

StateTree가 intent를 직접 제출할지, Blackboard/StateTree external data를 통해 간접 제출할지 결정 필요.

초기 구현은 단순하게 함수 호출로 시작할 수 있다.

```cpp
MovementIntentComponent->SubmitIntent(Character, Intent);
```

나중에 StateTree 전용 External Data로 정리할 수 있다.

---

### OQ-3. BossEvade 안전 위치 할당 방식

보스 전체공격에서 safe zone 후보를 어떻게 생성하고, 파티원에게 어떻게 나눌 것인가?

후보:

* 보스 공격 데이터에서 safe area 제공
* NavMesh 주변 샘플링
* 플레이어 주변 회피 포켓
* 파티원별 nearest-safe assignment
* Hungarian 또는 greedy assignment

초기 구현은 greedy assignment로 충분하다.

---

### OQ-4. 공격 중 캔슬 윈도우

P0 이벤트가 들어왔을 때 공격을 즉시 끊을 것인가, 특정 cancel window에서만 끊을 것인가?

이는 액션감에 직접 연결된다.

* 즉시 캔슬: AI 생존성과 반응성 증가
* 제한 캔슬: 공격의 무게와 모션 신뢰성 증가

초기에는 생존 인터럽트만 즉시 캔슬하고, 일반 회피는 cancel window를 따르는 방식이 무난하다.

---

### OQ-5. 원거리 soft relocation urgency

재배치 후 시간이 지날수록 이동 허용 점수를 높이는 soft urgency를 도입할지 검증 필요.

예시:

```cpp
float HoldAlpha = Clamp((TimeSinceLastRelocation - MinHoldTime) / RelaxDuration, 0, 1);
```

사용처:

* relocation pressure에 가산
* stickiness weight 감쇠
* move cost weight 감쇠

적 이동 구현 이후 우선적으로 검증한다.

---

## Consequences

### 긍정

* 이동 명령 출처가 하나로 통합된다.
* Formation, StateTree, Traversal, BossEvent의 책임이 분리된다.
* “왜 슬롯으로 안 돌아갔는가”를 intent priority로 설명할 수 있다.
* 공격 중 슬롯 이탈, 회피, traversal, 원거리 재배치를 같은 언어로 설명 가능하다.
* 포트폴리오에서 아키텍처 의도를 명확하게 보여줄 수 있다.

---

### 부정 / 리스크

* MovementIntentResolver가 새 추상화로 추가되어 초기 구현량이 늘어난다.
* StateTree와 Resolver 사이의 인터페이스 설계가 필요하다.
* priority와 interruptibility를 잘못 설계하면 행동이 지나치게 딱딱해질 수 있다.
* 너무 많은 intent type을 초기에 만들면 오히려 상태 폭발이 생긴다.

---

## Implementation Plan

### Step 1. 현재 구조 유지 + 용어 정리

* FormationBattleComponent는 계속 슬롯/home을 계산한다.
* 문서상 슬롯을 “HomeLocation”으로 재해석한다.
* 공격/회피 구현 전까지는 기존 push 방식 유지 가능.

---

### Step 2. 최소 MovementIntent 타입 도입

초기에는 다음 네 개만 둔다.

```text
HomeSlot
AttackCommit
Evade
Traversal
```

이 네 개로도 대부분의 기본 충돌을 표현할 수 있다.

---

### Step 3. Resolver를 얇게 구현

처음부터 복잡한 priority/cancel 시스템을 만들지 않는다.

초기 규칙:

```text
Traversal > Evade > AttackCommit > HomeSlot
```

이후 BossEvade, RangedRelocation, ForcedEvent를 추가한다.

---

### Step 4. Return Policy 도입

공격 종료 후 즉시 Home으로 복귀하지 않고, 행동별 Return Policy를 적용한다.

초기 기본값:

```text
Melee Attack: IfTooFarFromHome
Ranged Attack: WhenIdle
Evade: WhenDangerCleared
Traversal: Immediate or IfTooFarFromHome
```

---

### Step 5. BossEvade Coordinator 도입

보스 전체공격 구현 시점에 파티 단위 회피 위치 할당을 별도 coordinator로 만든다.

---

## Final Decision

전투 중 캐릭터 최종 위치는 슬롯 시스템 하나가 결정하지 않는다.

```text
Formation은 Home을 제공한다.
StateTree는 행동 상태를 결정한다.
각 시스템은 MovementIntent를 제출한다.
MovementIntentResolver가 최종 이동 목표를 하나로 결정한다.
```

슬롯은 정확히 서야 할 점이 아니라 행동의 기준점이다.
공격, 스킬, 회피, traversal은 Home을 삭제하지 않고 일시적으로 Home 추적 우선순위를 낮춘다.

이 구조를 통해 근거리 hard slot, 원거리 soft positioning, 공격 중 이탈, 보스 패턴 회피, traversal lock을 하나의 일관된 이동 중재 모델로 다룬다.
