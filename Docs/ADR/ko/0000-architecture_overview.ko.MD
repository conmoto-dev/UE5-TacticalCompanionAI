# 아키텍처 개관: 전투 중 이동·위치·행동의 레이어 분담과 MovementIntent 중재

> 작성 원칙 (README_ko·ADR-0001과 동일): 압축해서 적으면 몇 주 뒤에 내가 못 읽는다.
> 결정은 "왜"를 문장으로 풀어쓴다. 기각한 대안과 그 이유까지 남긴다 —
> 이 문서의 목적은 "이미 끝낸 설계를 나중에 다시 엎으려는 착각"을 막는 것이다.

> 📌 **이 문서의 위치 (다른 ADR과 다름):** 이것은 단일 결정을 기록하는 번호 ADR이
> 아니라, **여러 ADR이 공유하는 상위 아키텍처 개관**이다. 그래서 번호(ADR-000X)를 갖지
> 않고 `0000-architecture-overview`로 둔다. 전투 중 이동/위치가 *여러 레이어*에 걸쳐
> 어떻게 분담·중재되는가라는 층간(cross-layer) 틀을 정의하며, 개별 ADR(0001~0004)은
> 이 틀 안의 구체 결정이다. 층을 넘나드는 이동 관련 결정은 새 번호 ADR로 분기하지 말고
> **이 문서를 갱신**한다. (이 문서는 원래 "ADR-0004: 전투 행동 레이어링"으로 작성됐다가,
> 단일 결정이 아니라 전체 틀임이 분명해져 개관으로 승격됐다. 그 자리의 0004 번호는
> 이후 "RangedSafe 360도 영향맵" ADR이 차지한다.)

- **상태**: 살아있는 문서 (Living) — 행동 레이어([3]) 구현하며 계속 갱신한다.
- **최초 작성**: 2026-06
- **관련 코드**: `UFormationBattleComponent`(Home Provider 역할), (예정)
  `CompanionStateTree`, (예정) `UCombatMovementIntentComponent`(=MovementIntentResolver),
  `UTacticalTraversalComponent`, (예정) `BattleEventCoordinator`/`SquadEvadeCoordinator`
- **이 틀 안의 구체 ADR**: ADR-0001 (점프 A/B 분리), ADR-0002 (슬롯 추상화),
  ADR-0003 (BattleFormation 결정/실행 분리), ADR-0004 (RangedSafe 360도 영향맵)

---

## 0. 범위 메모

(이 문서가 왜 번호 ADR이 아니라 개관인지는 위 📌 박스 참조.) 단일 컴포넌트 안에서
닫히는 결정 — 예: ADR-0003의 RangedSafe 게이트, ADR-0004의 360도 영향맵 — 은 별도 번호
ADR로 두되, 그것이 *층간 모델*에 어떻게 편입되는지는 이 문서에 적는다.

---

## 1. 한 줄 요약

> Formation은 최종 이동 명령자가 아니라 **Home Provider**다. StateTree는 **행동 상태**를,
> 단일 **MovementIntentResolver**는 여러 이동 의도 중 *실제 실행할 하나*를 결정한다. 각
> 시스템(Formation/StateTree/Traversal/BossEvade/RangedRelocation/HitReaction)은 직접
> `MoveTo`를 부르지 않고 **`MovementIntent`를 제출**한다. 공격·스킬·회피는 슬롯(Home)을
> *삭제*하지 않고 **추적 우선순위만 일시 낮춘다**. **우선순위(무엇이 중요)와 인터럽트
> 가능성(지금 끊을 수 있나)은 분리**한다. 보스 전체공격 같은 파티 단위 조율은 개별 StateTree가
> 아니라 Squad/Event 레벨이 위치를 배정한다.

---

## 2. 맥락 (왜 이 결정이 필요했나)

### 2.1. 슬롯 좌표 하나로는 최종 위치가 안 정해진다

전투 중 동료의 최종 위치 위에는 여러 행동이 겹친다: 슬롯 기준 재배치, 공격/스킬 모션 중
자연스러운 이탈, 공격 후 복귀할지 계속 교전할지, 슬롯 주변 횡이동, 원거리 압박 재배치,
보스 전체공격 회피, traversal 중 이동권 잠금, 피격·경직·다운·부활 지원 같은 강제 행동.

따라서 세 질문이 남는다.

1. **위치와 행동을 어떻게 분리하는가?**
2. **여러 시스템이 서로 다른 이동 목표를 요구할 때, 최종 이동 명령은 누가 정하는가?**
3. **공격·회피·traversal·복귀의 우선순위와 인터럽트 가능성은 어떻게 다루는가?**

### 2.2. ADR-0001의 함정이 이동 전반에서 재발한다

0001은 traversal에서 이미 깨달았다 — 여러 주체가 직접 명령을 내리면 출처가 흩어지고 "왜
이렇게 움직였나"를 추적 못 한다. 그래서 "현재 모드의 *단일 주체*"를 세웠다. **이 문서는 그
'단일 결정권' 직관을 점프 한정에서 *이동 전반*으로 일반화한다.** Formation·StateTree·Traversal·
BossEvade가 각자 `MoveToLocation`을 부르면 0001이 경고한 바로 그 회색지대(서로 덮어쓰기,
"쟤가 막았겠지")가 이동 전체에서 터진다.

### 2.3. 이 프로젝트의 전투 정책은 하이브리드다

- **근거리**: Endfield식 target-centric **hard slot**에 가깝다.
- **원거리**: Relink식 **soft/personal positioning**에 가깝다.
- **전투 행동**: 각 동료가 자기 상태로 공격·스킬·회피를 수행하는 **자율 전투**.
- **슬롯**: "정확히 서야 하는 점"이 아니라 행동의 기준점 = **Home**.

단일 모델로 욱여넣으면 한쪽이 망가진다. 그래서 레이어로 나눈다.

---

## 3. 결정

전투 위치 결정을 세 층으로 나눈다.

```text
┌───────────────────────────────────────────────────────────────┐
│ Layer 1 — Home / Position Reference                            │
│   "이 캐릭터가 기준 삼을 전투 위치는 어디인가?"                  │
│   담당: FormationBattleComponent, SlotGeneratorStrategy         │
│   산출: HomeLocation / SlotAnchor                               │
├───────────────────────────────────────────────────────────────┤
│ Layer 2 — Combat State / Action Policy                         │
│   "지금 무엇을 하고 있는가?"                                     │
│   담당: StateTree, Combat Action Layer                          │
│   산출: Attack / Skill / Evade / Idle / Reposition 등 상태       │
├───────────────────────────────────────────────────────────────┤
│ Layer 3 — Final Movement Resolution                            │
│   "이번 프레임에 실제로 어디로 가야 하는가?"                     │
│   담당: MovementIntentResolver                                  │
│   산출: FinalMoveTarget / MovementLock / NoMove                 │
└───────────────────────────────────────────────────────────────┘
```

### 3.1. (D0) 최종 이동은 단일 MovementIntentResolver가 정한다

여러 시스템이 동시에 `MoveToLocation`을 부르는 구조를 **금지**한다. 각자 직접 명령 대신
`MovementIntent`를 제출하고, 하나의 Resolver가 우선순위·인터럽트 가능성·만료·이동 잠금을
평가해 최종 목표를 정한다.

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
    ForcedEvent,       // 링크어택·부활 지원·강제 연출
    Staggered          // 피격/경직/다운
};

struct FMovementIntent
{
    EMovementIntentType Type = EMovementIntentType::None;
    FVector TargetLocation   = FVector::ZeroVector;
    int32   Priority         = 0;
    bool    bLocksMovement     = false;
    bool    bReleasesHomeSlot  = false;
    bool    bCanBeInterrupted  = true;
    float   AcceptanceRadius   = 80.f;
    float   ValidUntilTime     = 0.f;
    FName   DebugReason;
};
```

목적: 여러 시스템이 MoveTo를 덮어쓰는 상황을 막고, 각자 "원하는 의도"만 제출하며, 최종
실행권은 Resolver 하나가 갖는다. (0001의 "단일 주체"를 이동 전반으로 확장.)

### 3.2. (D1) Formation은 Home Provider다

Formation은 캐릭터의 현재 전투 기준점 `HomeLocation`을 계산한다. 슬롯은 "항상 정확히 서야
하는 점"이 아니라, *할 일이 없거나 / 너무 멀어졌거나 / 전투가 안정되었을 때 수렴할 기준점*이다.

| 역할 | Home 의미 |
|---|---|
| Melee | Target-centric hard combat slot |
| Ranged | Soft combat position / 사거리·안전·시야 기준점 |
| Support | Leader 또는 party-relative support pocket |
| Skill/Event | 일시적 override pose, Home과 별개 |

### 3.3. (D2) StateTree는 행동 상태를 정한다

StateTree는 현재 행동 모드를 정하고, **직접 `MoveTo`를 부르지 않는다.** 상태에 맞는
`MovementIntent`를 제출한다.

| 상태 | Home과의 관계 |
|---|---|
| CombatIdle | Home 기준 대기 또는 미세 이동 |
| Reposition | Home 또는 soft position으로 이동 |
| AttackReady | 사거리·방향 맞추며 공격 준비 |
| AttackCommit | 공격 모션이 이동권 소유, Home 추적 양보 |
| SkillCommit | 스킬이 요구하는 위치/모션 우선 |
| Evade / BossEvade | Home 추적 중단, 회피/지정 안전 위치 우선 |
| Traversal | 이동 실행권 잠금, 공중/도약 중 간섭 금지 |
| Staggered/Downed | 모든 일반 이동 intent 무시 |

### 3.4. (D3) 슬롯 해제는 "Home 삭제"가 아니라 "Home 추적 양보"다

공격·스킬·회피 중에 슬롯 좌표를 *삭제하거나 옮기지 않는다.* Home은 계속 계산되고, 다만 현재
행동 intent가 HomeSlot intent보다 높은 우선순위를 가질 뿐이다. 행동이 끝나 더 높은 intent가
사라지면 Resolver가 다시 HomeSlot/Return intent를 고를 수 있다.

이것은 **평시 Follow의 Yield, Traversal의 "진행 중 슬롯 push 스킵", 그리고 ADR-0003의 커밋
잠금**을 일반화한 것이다. 장기적으로 `if (IsAttacking) continue;` 같은 예외문을 Formation에
계속 쌓지 말고, **Resolver의 우선순위 선택으로 통일**한다.

### 3.5. (D4) 우선순위와 인터럽트 가능성은 분리한다

높은 우선순위라고 항상 즉시 현재 행동을 끊을 수 있는 건 아니다.

```text
Priority         : 무엇이 더 중요한가?
Interruptibility : 현재 행동을 실제로 끊을 수 있는가?
```

보스 즉사 패턴 회피는 P0지만, 캐릭터가 이미 공중 traversal 중이거나 강한 공격 모션의
non-cancel 프레임이면 즉시 이동이 불가능할 수 있다. Resolver는 함께 본다: 새 intent priority /
현재 intent interruptibility / 현재 액션 cancel window / movement lock / forced event /
emergency override.

```text
[P0] Forced / Survival   : BossEvade, 즉사 패턴, 강제 회피, 피격/다운
[P1] Traversal / Lock    : 점프, 낙하, 공중 이동, 지형 극복
[P2] Skill / Attack      : 스킬 발동, 공격 모션, 링크/연계
[P3] Tactical Reposition : 원거리 압박 재배치, 위치 조정, maneuver
[P4] Home Slot           : 슬롯/home 복귀, 일반 대기 수렴
```

P0가 들어와도 현재 상태가 `Uninterruptible`이면 즉시 이동 못 할 수 있고, 그땐 회피 예약 /
방어·가드 전환 / 피해 감수 / 슈퍼아머 / 피격 리액션 / 다음 cancel window 회피 중 하나로 처리한다.

### 3.6. (D5) 보스 전체공격은 개별 StateTree 단독으로 위치를 고르지 않는다

여러 동료가 동시에 회피해야 하는 상황을 각자 독립으로 풀면 — 같은 안전지점으로 몰리고,
좁은 입구에서 낑기고, 플레이어 이동을 방해하고, 회피 경로가 겹친다. 따라서 두 단계로 나눈다.

```text
1. Squad/Event level : 위험 감지 → safe zone 후보 생성 → 파티원별 회피 위치 할당 → EvadeIntent 발행
2. Individual StateTree : 현재 행동 캔슬 가능 판단 → Evade/BossEvade 전이 → 할당 위치로 intent 제출
```

Formation은 보스 패턴을 *몰라야* 하고, 개별 StateTree도 safe position allocation을 독자적으로
풀지 않는다. 이 책임은 추후 `BattleEventCoordinator`/`SquadEvadeCoordinator`/`PartyCombatDirector`
같은 별도 레이어가 맡는다.

### 3.7. (D6) Return Policy를 명시한다

공격이 끝났다고 항상 즉시 Home으로 돌아가면 "전진 공격 → 종료 → 즉시 후퇴 → 다시 전진"이
반복돼 어색해진다. 행동별 복귀 정책을 둔다.

```cpp
enum class EReturnToHomePolicy : uint8
{
    Immediate,          // 행동 종료 즉시 복귀
    WhenIdle,           // 다음 행동이 없을 때만
    IfTooFarFromHome,   // Home 허용 반경을 벗어났을 때만
    BlendTowardHome,    // 행동 중 이동 목표에 Home 방향을 약하게 섞음
    NeverUntilRetarget  // 현재 교전 중에는 복귀 안 함
};
```

| 역할/행동 | Return Policy |
|---|---|
| Melee basic attack | IfTooFarFromHome |
| Melee heavy skill | WhenIdle / IfTooFarFromHome |
| Ranged attack | BlendTowardHome / WhenIdle |
| Ranged pressure relocation | IfTooFarFromHome |
| BossEvade | WhenIdle after danger cleared |
| Traversal | Immediate / IfTooFarFromHome |
| ForcedEvent | 이벤트 종료 후 상황별 |

Home 복귀는 **가장 낮은 우선순위 intent**다. 다른 공격/회피/스킬 intent가 있으면 실행되지 않는다.

### 3.8. (D7) Maneuver는 독립 State가 아니라 Modifier로 시작한다

슬롯 주변 횡이동·미세 거리 조정을 처음부터 독립 State로 만들지 않는다. 초기엔 `HomeSlot` 또는
`AttackReady` intent의 target을 약간 변형하는 **modifier**로 취급한다. 행동 종류가 늘고 독립
전술 가치가 생기면 별도 State로 승격한다. (YAGNI — ADR-0002 대안 D와 같은 결.)

### 3.9. (D8) 원거리 재배치는 점수만으로 자동 카이팅하지 않는다 — 이미 착수됨

원거리 안전 위치 계산은 필요하지만 "위험하면 항상 더 안전한 곳으로"를 점수만으로 처리하면
무한 카이팅이 난다. 따라서 두 단계로 나눈다.

```text
1. Relocation Gate    : 지금 위치를 버릴 만큼 나쁜가?
2. Position Evaluation: 움직인다면 어디가 좋은가?
```

**이 D8은 ADR-0003과 ADR-0004로 이미 구체 구현됐다.** ADR-0003의 커밋 게이트(첫 커밋/
사거리 이탈/reluctance 게이트 위협 회피)가 곧 **Relocation Gate**이고("*언제* 옮기나"),
ADR-0004의 360도 영향맵이 곧 **Position Evaluation**이다("옮긴다면 *어디로*"). 두 ADR이
이 D8의 두 단계를 각각 맡는다. 이 문서는 그것을 intent별 가중치로 일반화한다.

```text
InitialPlacement    : Range / FriendlySide / Safety 중심, AllyCover 낮음, Stickiness 없음
Maintain            : Current stickiness 강함, 작은 개선으로는 안 움직임
PressureRelocation  : Top-K Threat / AllyCover / FriendlySide 강화, Stickiness 약화
EmergencyDisengage  : Immediate danger 최우선, cooldown/hold 일부 무시
```

원거리 AI 기본 철학(ADR-0003과 동일): **좋은 자리가 있다고 항상 이동하지 않는다. 현재 위치가
나빠졌을 때만 이동한다. 위험 이동은 허용하되 최적화 이동은 억제한다.**

### 3.10. (D9) 슬롯-앵커 국소 흐름 — 닻은 이산, 그 주변은 연속 (미래 방향)

> D7(Maneuver는 Modifier로 시작)의 **구체적 형태**다. 아직 구현 전이며, 어떤 모양이
> 될지를 미래에 잊지 않으려 기록한다.

**문제의식:** 슬롯 시스템은 위치를 이산적으로 정한다 — "저 슬롯으로 가라". 그런데 동경하는
레퍼런스(Relink/Endfield)의 동료는 슬롯에 도착한 뒤에도 죽은 듯 서 있지 않고, 적을 살피며
미세하게 흐른다(횡이동, 거리 조정). 이 "물처럼 흐르는" 느낌을 어떻게 줄 것인가.

**핵심 구분 — 두 개의 다른 문제를 섞지 않는다:**

```
문제 A: 동료가 어느 슬롯에 설지 (배치)        → 이산. 슬롯 레이어(Strategy + 커밋)가 푼다.
문제 B: 슬롯에 도착한 뒤의 미세 거동 (흐름)   → 연속. 이 D9가 다룬다. 별개 레이어.
```

이 둘을 한 시스템에 욱여넣으면(예: 슬롯 자체를 매 틱 미세하게 흔들기) ADR-0003의 커밋
게이트와 충돌한다 — 게이트는 "목적지를 자주 안 바꾸려고" 만든 건데, 흐름은 "목적지를 매 틱
바꾸려" 하기 때문. 그래서 **닻(슬롯)은 그대로 두고, 그 주변의 작은 오프셋만 흐르게** 한다.

**채택 방향 — 닻 + 국소 오프셋:**

```
닻 (CommittedSlot)   = 슬롯 레이어가 정함. 커밋 게이트가 잠금. 안 흔들림.
국소 오프셋 (매 틱)   = 닻 기준 좁은 반경 안에서 작은 포텐셜 필드로 계산.
최종 목적지          = 닻 + 국소 오프셋. 이걸 locomotion에 전달.
```

닻은 멀리 안 움직이니 큰 그림(슬롯 배치)은 유지되고, 오프셋이 매 틱 작게 흔들리니 닻 주변에서
흐른다. 큰 움직임(닻까지 가기 = 기존 그대로)과 작은 움직임(닻 주변 흐름 = D9 추가)이 분리된다.

**왜 "전역 연속 흐름"이 아니라 "닻 + 국소"인가 (기각 기록):** 슬롯 개념을 아예 버리고 맵 전체를
매 틱 그래디언트로 흐르는 순수 포텐셜 필드도 후보였다(ADR-0004 §5 대안 D). 그러나 그건
"목적지를 정하고 간다"는 이 프로젝트 전체의 State/커밋 패러다임과 충돌하고("목적지 개념 자체가
없는" 모델), 구현 리스크도 크다. 닻 + 국소는 **State는 살리고(닻=어느 슬롯에 속함) 흐름도
얻는(닻 주변 미세 거동)** 절충으로, 기존 아키텍처를 안 깬다.

**MovementIntent와의 관계 — 이 D9는 P4 HomeSlot intent의 일부다:** 국소 오프셋은 "HomeSlot
intent의 목적지 = 닻 + 오프셋"으로 흡수된다. 공격(P2)·회피(P0)가 끼어들면 P4가 지므로 흐름도
자동으로 멈추고, 행동이 끝나 P4로 복귀하면 흐름도 되살아난다. 즉 "공격 중엔 안 흐르고 평소엔
흐른다"가 별도 if문 없이 우선순위로 풀린다(D3의 "Home 추적 양보"와 같은 메커니즘).

**적용 범위:** 근거리(Arc)·원거리(RangedSafe) 양쪽에 적용 가능하다 — 둘 다 커밋 슬롯이 닻이
되므로. 근거리는 "타겟 주위 도는 느낌", 원거리는 "적 살피는 횡이동"으로 거동이 갈릴 수 있으나,
초기엔 공통 구현으로 시작하고 갈리면 그때 분리한다(YAGNI).

**구현 시점:** StateTree/MovementIntent 레이어가 들어온 뒤. 그 전까지는 "닻 + 국소 오프셋"을
임시로 컴포넌트 후처리(`Anchor + ComputeLocalDrift()`)로 둘 수 있으나, 어차피 MovementIntent로
흡수될 자리이므로 무리해서 당기지 않는다.

---

## 4. 결정 과정의 메타 교훈 (미래의 우리에게)

- **0001의 "단일 주체" 직관이 옳았고, 이 문서는 그 일반화다.** "점프 결정권은 모드별 단일
  주체"가 "이동 명령은 단일 Resolver"로 확장됐다. 새 패턴을 발명한 게 아니라 검증된 패턴을
  넓힌 것.
- **비대칭 ≠ 결함 (0001 §2.2 재확인).** "Battle엔 아직 행동 레이어가 없다"는 *미구현*이지
  오배치가 아니다. 코드 표면의 비대칭을 보고 "합치자"로 직행하지 말 것.
- **intent type을 처음부터 많이 만들면 상태 폭발.** 11종을 한 번에 만들지 말고 4종(HomeSlot/
  AttackCommit/Evade/Traversal)으로 시작해 충돌을 표현해본 뒤 늘린다.
- **priority와 interruptibility를 섞으면 행동이 딱딱해진다.** "중요하다"와 "지금 끊을 수 있다"는
  다른 축이다. 섞으면 P0가 모든 모션을 즉시 자르는 깡통 AI가 되거나, 반대로 못 자르는 무딘
  AI가 된다.

---

## 5. 기각한 대안

### 대안 A: Formation 레이어가 행동까지 처리

> FormationBattleComponent가 슬롯뿐 아니라 공격·회피·스킬·보스 패턴까지 판단한다.

**기각.** 위치 계산과 전투 전술이 섞여 컴포넌트가 비대해진다. 인터럽트 우선순위·캔슬 가능성·
공격 모션 같은 *액션 레이어 지식*이 Formation에 침투한다. (0001 §2.2의 "두 판단을 뭉쳐 봄"과
같은 함정.)

### 대안 B: 각 시스템이 직접 MoveTo를 호출

> Formation/StateTree/BossEvade/Traversal이 각자 필요할 때 `MoveToLocation`을 부른다.

**기각.** 이동 명령 출처가 여러 개가 되어 서로 덮어쓰고, "왜 이 위치로 갔나"를 추적 못 한다.
0001이 traversal에서 이미 거부한 구조. MovementIntentResolver로 단일화한다.

### 대안 C: 공격/스킬 중 슬롯 좌표 자체를 옮기거나 삭제

> 공격 시 슬롯을 스킬 위치로 옮기거나 슬롯을 비활성화한다.

**기각 (ADR-0003 대안과 동일 결).** Home 기준점이 사라져 행동 종료 후 복귀가 불안정해진다.
슬롯은 계속 계산하되 높은 우선순위 intent가 HomeSlot intent를 덮는다(D3).

### 대안 D: 인터럽트(보스 회피)를 Formation이 처리

> 보스 전체공격 회피를 FormationBattleComponent가 받아 슬롯을 안전 위치로 재배치한다.

**기각.** 보스 패턴은 위치 계산이 아니라 *전투 행동 인터럽트*다. Formation은 보스 패턴을
몰라야 한다(D5). 단, safe position allocation은 개별 StateTree 단독이 아니라 Squad/Event
레벨에서 조율한다.

### 대안 E: 원거리 카이팅을 RangedSafe 점수만으로 구현

> 안전 점수가 높은 곳으로 계속 이동하게 만든다.

**기각.** 빠른 적·끈질긴 어그로에 무한 카이팅이 난다. Relocation Gate와 Position Evaluation으로
분리한다(D8). — 이건 ADR-0003에서 이미 실증됐다.

---

## 6. 결과 (얻는 것 / 치르는 것)

**얻는 것:**

- 이동 명령 출처가 하나(Resolver)로 통합 → "왜 슬롯으로 안 돌아갔나"를 intent priority로 *설명
  가능*.
- Formation/StateTree/Traversal/BossEvent의 책임이 분리.
- 공격 중 이탈·회피·traversal·원거리 재배치를 *같은 언어*(intent)로 다룬다.
- 포트폴리오에서 아키텍처 의도를 한 모델로 보여줄 수 있다.

**치르는 것 / 리스크:**

- Resolver가 새 추상화로 추가되어 초기 구현량이 는다.
- StateTree ↔ Resolver 인터페이스 설계가 필요하다(OQ-2).
- priority/interruptibility를 잘못 설계하면 행동이 지나치게 딱딱해진다.
- intent type을 초기에 너무 많이 만들면 상태 폭발(메타 교훈).

---

## 7. 열린 질문 (구현 중 닫는다)

- **OQ-1. Resolver의 소속.** PartyCharacter 내부 / 별도 `UMovementIntentComponent` /
  AIController / StateTree Task / 독립 `UCombatMovementComponent` 중. 현재 추천은 독립
  `UCombatMovementIntentComponent` — PartyCharacter는 *최종 실행자*에 가깝고 Resolver는
  *정책/중재자*에 가깝기 때문.
- **OQ-2. StateTree ↔ Resolver 인터페이스.** 직접 함수 호출(`SubmitIntent(Character, Intent)`)로
  시작 → 나중에 StateTree 전용 External Data로 정리.
- **OQ-3. BossEvade 안전 위치 할당.** 공격 데이터 제공 safe area / NavMesh 샘플링 / 플레이어
  주변 포켓 / nearest-safe assignment / Hungarian·greedy. 초기엔 greedy로 충분.
- **OQ-4. 공격 중 캔슬 윈도우.** P0 시 즉시 캔슬 vs 특정 cancel window. 액션감 직결. 초기엔
  *생존 인터럽트만 즉시 캔슬*, 일반 회피는 cancel window.
- **OQ-5. 원거리 soft relocation urgency.** 재배치 후 시간 경과로 이동 허용을 높이는 soft
  urgency 도입 검증. `HoldAlpha = Clamp((TimeSinceLastRelocation - MinHoldTime)/RelaxDuration, 0, 1)`.
  적 이동 구현 이후 우선 검증. (ADR-0003의 reluctance와 같은 계열 — 한쪽은 회피 억제, 이쪽은
  복귀/이동 허용 완화.)

---

## 8. 구현 순서 (지금 당장 착수 X — [3] 진입 시)

1. **용어 정리 + 현 구조 유지.** Formation은 계속 슬롯/home 계산. 문서상 슬롯을 "HomeLocation"
   으로 재해석. 공격/회피 구현 전까진 기존 push 유지(ADR-0003 게이트 그대로).
2. **최소 intent 4종 도입:** `HomeSlot / AttackCommit / Evade / Traversal`. 이걸로 기본 충돌
   대부분 표현 가능.
3. **Resolver 얇게:** 처음엔 단순 우선순위 `Traversal > Evade > AttackCommit > HomeSlot`.
   이후 BossEvade/RangedRelocation/ForcedEvent 추가.
4. **Return Policy 도입:** 행동 종료 후 즉시 복귀 대신 D6 정책. 초기값 표 참조.
5. **BossEvade Coordinator:** 보스 전체공격 구현 시점에 파티 단위 회피 위치 할당을 별도
   coordinator로.

---

## 9. 구체 ADR과의 연결

- **ADR-0001:** "현재 모드의 단일 주체"(점프 A) → 이 문서의 "단일 MovementIntentResolver"로
  일반화. 또 A(정책)/B(역학) 분리가, 여기선 "Layer 2 행동 결정"과 "Layer 3 이동 실행"의 분리로
  재현된다. 모드 전환 트리거(0001 §6 숙제)는 이 모델에서 "어느 시스템이 어느 intent를 제출할
  자격이 있나"로 자연히 흡수된다.
- **ADR-0002:** Formation이 "Home(슬롯)을 제공"하는 것은 0002의 "Strategy가 슬롯을 생성"
  위에 얹힌다. Home은 SlotGenerator 산출물의 *재해석*(정확히 설 점 → 수렴 기준점)이다.
- **ADR-0003:** 이 문서가 일반화하는 **구체 씨앗**이다. ADR-0003의 유닛별 커밋 잠금 = HomeSlot/
  RangedRelocation intent의 우선순위 보존(D3), 재배치 게이트 = Relocation Gate의 첫 구현(D8),
  reluctance = soft urgency의 짝(OQ-5). 구체(0003) → 일반(이 문서) 순서로 읽으면 자연스럽다.
- **ADR-0004:** D8의 **Position Evaluation**("옮긴다면 어디로")의 구체 구현이다. ADR-0003이
  D8의 Gate("언제 옮기나")를 맡고, ADR-0004가 그 짝인 Evaluation을 맡는다. 또 ADR-0004 §5
  대안 D(연속 포텐셜 필드)에서 분리한 "흐르는 느낌"이 이 문서의 **D9(슬롯-앵커 국소 흐름)**로
  이어진다 — 360도 영향맵이 닻(목적지)을 정하고, D9가 그 닻 주변의 미세 거동을 다룬다.