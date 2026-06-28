# 개인 로드맵 및 핵심 메모

> 외부용이 아닌, 개발자 본인의 진행 관리·사고 정리·아이디어 노트.
> ⚠️ 압축해서 적으면 몇 주 뒤에 내가 못 읽는다. 결정은 "왜"를 문장으로 풀어쓸 것.
> 깊은 설계 근거는 `Docs/ADR/ko/`에 있다. 여기선 "언제 뭘 왜 했나"의 흐름 + 진행 관리 + 아이디어.

---

## 🎯 큰 그림

**최종 목표**: 일본 게임업계 언리얼 C++ 게임플레이 프로그래머 복귀

**나의 차별점**: 동료 AI 시스템 (대학생 때부터의 꿈, 이전 BT/네비 경험 기반)

**현재 상태**: 평시 추종 + 환경적응 + Yield 완성 → 전투 진형(배치/재배치 결정) 완성 → 적 스폰 초기배치(CompositeFormation) 완성 → 다음은 StateTree·전투 행동·적 그룹 전투

> ⚠️ **회사 다니면서 작업.** 예상보다 오래 걸리는 게 기본이다. "몇 주차"
> 같은 건 안 센다. 대신 "완료한 것 / 지금 하는 것 / 언젠가 할 것"으로 관리하고,
> 로드맵 전체 지도 위에서 매번 우선순위를 다시 새긴다.

---

## ✅ 완료한 것 (영역별)

### 평시 추종 진형
- 3-레이어 아키텍처 (Manager / Controller / Character), 역할 무관 캐릭터 클래스
- V자 진형 추종, Sphere Sweep + VectorPlaneProject 슬라이딩
- `UFormationDataAsset` (UPrimaryDataAsset + FFormationSlotData) — 디자이너가 코드 빌드 없이 진형 추가
- 환경 보정 파이프라인 (NavMesh as primary truth, 4단계 헬퍼: 슬로프 Z보정 → NavMesh 투영 → 벽 슬라이드 → anchor 끌어당김)
- 스프링 기반 GapScale 동적 변화 + Quaternion 지연 회전
- NavMesh raycast 기반 V↔I 자동 전환 (히스테리시스)
- Per-slot 거리 기반 슬롯 캐시 갱신 — 리더 이동이 아니라 *슬롯-플레이어 거리* 기준. 결과 좌표보다 *진입 시점* 통제가 중요하다는 통찰
- 경사면을 벽으로 오인하는 버그 수정 (ImpactNormal.Z 임계값)

### Yield (양보)
- Character → Component 마이그레이션 — Yield는 진형 단위 정책이라는 통찰
- Reaction Time delay — 동시 반응 방지, 시간 차원 디자인의 첫 사례
- 자연성 폴리싱 — entry delay + projected backward offset (cross-product 순서 버그 수정 포함)
- Hysteresis 안전망 (PostEditChangeProperty + BeginPlay) — YieldExitRadius ≥ YieldEnterRadius 코드 강제
- Velocity → Facing 기반 cone 판정 — 카메라/캐릭터 facing 분리 게임에서 의도 명확화
- **Yield Strategy 분리** — UYieldStrategy 추상 + _Standard / _None + IYieldContextProvider 인터페이스. 진형 단위 알고리즘 선택 + 미래 Component 재사용
- Hungarian-Yield 충돌 수정 (Yielding 중 매칭 skip)

### 회피 / 트래버설
- **RVO → Detour Crowd 마이그레이션** — RVO는 리액티브 + 그룹 인식 없어 회전/전환 후 근접 슬롯에서 "비비적"댔음. Detour는 예측 + 그룹 우선순위로 해결
- **컨트롤러가 회피 역할 결정** — Possess 단일 진입점, 컨트롤러가 `ITacticalAvoidanceController`로 역할(Leader/Normal/Yielding) 세팅
- `UPlayerCrowdAgentComponent` — 플레이어는 AIController 없어 Detour 미인식 → crowd agent 수동 등록
- `UTacticalCrowdFollowingComponent` — PathFollowing 교체(SetDefaultSubobjectClass), 그룹 비트마스크(1=Leader/2=Normal/4=Yielding) 우선순위
- Yield ↔ crowd 역할 동기화 — 좌표(어디로)와 회피 그룹(누굴 피함)이 따로 놀던 구멍 메움
- 회피 튜닝 — RangeMultiplier 1.2 최적. AvoidanceQuality::High는 역효과(함정 노트 참조)
- **상향 점프 트래버설** — UTacticalTraversalComponent. 벽 인지 → 도약점 계산 → 베지어 조향 → 포물선 발사

### 전투 진형 (BattleFormation) — ADR-0001~0004로 문서화
- **Manager 모드 결정** — EnterBattleDistance 700 / ExitBattleDistance 1000 히스테리시스. `GetPerceivedEnemies` 단일 소스 (모드결정·포메이션·추후 타겟팅 공유)
- **타겟 기준 슬롯 배치** — anchor = 타겟 transform (평시는 리더). CombatRole GameplayTag로 그룹 분리 (Melee/Ranged, Native 태그)
- **배정 정책 2갈래** — GroupHungarian(Arc/근접: 집합 생성 → 진입 시 헝가리안 1회 → 타겟 추종) / MemberSpecific(RangedSafe/원거리: 유닛별 커밋)
- **슬롯 생성 추상화** (→ ADR-0002) — "1인 → 1슬롯" 계약 통일. 멤버 순회는 컴포넌트, 집합형은 N·i, 개별형은 자기 사거리·적분포. 직업이 Strategy를 *선택*(귀속 아님). "배열로 넘기는 위화감" 직감이 집합형/개별형 본질 차이를 끌어냄
- **결정/실행 분리 커밋 게이트** (→ ADR-0003) — 영향맵을 매틱 컨트롤러 → 주기적 오라클로. FCommitSnapshot, 트리거 3종(첫배치/사거리이탈/위협회피), reluctance 연속 가중치. 생명주기 버그(Activate=진입이벤트 오해)를 며칠 헤매다 "증상 아니라 함수 계약을 봐라"로 잡음
- **RangedSafe 360도 영향맵** (→ ADR-0004) — 방향축(FrontlineDir) 폐기. sector 분산은 "군집의 반대 극단(억지 분산)"이라 기각. 360도 등각 후보 + 점수 위임(Threat soft-saturation/Occupancy/PullToAlly). 축이 없어 플레이어 궤도 회전 트위치 구조적 소멸
- **점프 결정권한 A/B 분리** (→ ADR-0001) — "할까(A=정책, 모드별 단일주체)"와 "가능한가(B=역학, 공용 Traversal)" 분리. 비대칭을 보고 "합치자" 직행하는 착각을 막은 사례

### 적 스폰 / 초기 배치 (CompositeFormation) — ADR-0007로 문서화
- **모드 2갈래** — `EEnemyFormationMode { ByEnemyClass, CompositeFormation }`. Details에서 `EditCondition`+`EditConditionHides`로 선택 모드 데이터만 노출. ByEnemyClass는 *입구만* (BuildFallbackSlots 임시 일렬, 전투 행동 아직 없음 — Targeting/StateTree 연결점으로 비워둠)
- **Spawner 인라인 복합 배치** — `FEnemyCompositeFormation`을 DataAsset 아니라 `AEnemySpawner` 안 인라인 UPROPERTY로. 디자이너가 `FEnemySubFormation` 배열을 `+/-`로 직접 조합. 재사용 Preset보다 *스폰 지점별 직접 편집*이 목표.
- **SubFormation 전략** (→ ADR-0002 재사용) — `UEnemySubFormationBase`(EditInlineNew+Blueprintable) + `_Line`/`_Circle`/`_Arc`/`_Scatter`. 계약은 `BuildSlots(기준Transform, SlotCount) → FEnemyFormationSlot[]` 하나(BlueprintNativeEvent라 BP 확장 가능). Scatter는 후보 N개 중 기존 슬롯과 최대거리(best-candidate sampling)
- **책임 분리 (이상위치 ↔ 보정)** — 전략은 *이상적 슬롯만*, 벽/장애물/NavMesh 보정은 `AEnemySpawner`가 공통. 새 SubFormation 추가가 보정 중복 없이 위치계산만으로 끝남. "한 곳이 너무 많이 안다"는 위화감이 이 분리를 끌어냄
- **스폰 보정 흐름** — `BuildSpawnResolveCandidates`(반경 step씩 ↑, 각 반경 방향 N개 링) → `ProjectPointToNavigation`(옵션) → `AdjustIfPossibleButDontSpawnIfColliding`. 안전위치 못 찾으면 *벽 안 강제 스폰 안 하고* Warning 로그 후 nullptr. 보정 옵션 전부 Spawner UPROPERTY
- **에디터 즉시 확인 루프** — `SpawnEnemies`/`ClearSpawnedEnemies` `CallInEditor`. 게임월드 아니면 `RF_Transient`로 스폰(레벨 안 더럽힘) + 슬롯 디버그 표시. 조합→즉시 확인→조정

### 프로젝트 운영 / 문서
- **ADR 문서 체계 구축** — 0000 개관(번호 밖) + 0001~0004 + 0006~0007, **한국어판(`ko/`, 코드 디테일 복기용) / 일본어판(`Docs/ADR/`, 코드 몰라도 읽히는 어필용)** 양판. 인덱스 README 양판. (0006은 구현 후 보충 추가 — 전투중 방침 vs 0007 스폰시점 도구 경계 명시)
- **ADR 작성 지침 정립** — 동결 아니라 근거 보존 / 기각 대안 필수 / 검증범위 넘는 단정 금지 / 안 한 고민은 어필용이라도 안 적음 / 구현 안 한 것 한 것처럼 안 씀
- 주석 규칙 (한국어 작업용 + 일본어 설계핵심만, 영어 제거 / 그룹박스 + 단계넘버링 / UPROPERTY 툴팁 한일 병기)
- 커밋 메시지 규칙 (영어, conventional prefix, ■ Problem/Changes/Preserved/Known limitations)
- README 3분할 (영/일 = 어필 랜딩, 한 = 이 문서 = 진행관리)

---

## 🚧 진행 중 / 바로 다음

- [ ] **영상 촬영 (포폴 자료)** — ⭐ 1순위.
- [ ] **내려가는 트래버설** — takeoff 계산이 올라가기 전제라 내려가기 깨짐. 점프 아니라 걸어 떨어지기로 기움 (아래 설계 고민)

---

## 🗺️ 로드맵 — 생각한 거 전부 (다 못 해도 적어두고 우선순위 새기기)

> 다 할 수 없다는 걸 안다. 그래도 다 적어둔다. 머릿속에만 있으면 까먹고 같은 고민 반복한다.
> 전체 지도를 펼쳐놓고, 매번 "지금 이 중 뭐가 제일 중요한가"를 다시 고른다.

### 🟢 기능 설계 (설계 기반은 있지만 시스템 전체 설계 후 폴리싱 예정)
- **내려가는 트래버설** — NavLink 안 쓰는 중. 올라가기=점프 완료. 내려가기는 walk-off-ledge(가장자리까지 MoveTo → 직접 이동입력으로 NavMesh 밖 → 자동 낙하 → 착지 후 MoveTo 복귀)로 기움. 가장자리 감지 복잡도가 고민
- **평지 갭 점프 판정** — 단차 없이 끊긴 절벽(ZDiff≈0). 수직 sweep으로 안 잡힘 → NavMesh 연속성/중간 ground trace로 감지해야. "ZDiff가 신호가 아니라 NavMesh 우회량이 신호"라는 결론과 같은 결. **나중에 고민** (ADR-0001 §8.2)
- `SlotCacheUpdateDistance` → FormationDataAsset 이동 (값 500이 I진형 군집 유발, 진형별 튜닝)
- Yielding 중 재평가 (현재 진입 후 플레이어 추가 접근 무반응)
- 리더 스왑 실제 구현 (Possess 핸들러 이미 깔림 → 부르기만)
- `AbortTraversal` APartyCharacter → ACharacter 캐스트 정정 (비-PartyCharacter에서 abort 동작 회복, ADR-0001 §9)
- Occupancy 선형 → 제곱 감쇠 (영향맵 "격자 느낌" 나면 한 줄 변경, ADR-0004 §6)
- 스폰 보정 슬롯별 독립 → 확정 위치 반영 (좁은 공간서 다수 슬롯 동시 막힐 때 후보가 서로 침범. 실측 후 거슬리면, ADR-0007 §6)
- **V/I 자동전환 정밀화** — 현재 리더 위치에서만 통로 폭 측정 (진형 실제 점유공간 무시). StateTree 복합조건으로

### 🟡 핵심 구조 설계 (필수 구현 목표 설계 시스템)
- **StateTree 도입** — 모드 선택을 컴포넌트 if/else에서 이관. **전투 다음** (지금은 State가 사실상 Follow/Battle 2개 — StateTree가 풀 문제가 이제 막 실재). 복합조건(좁음 AND 0.5초 AND 비전투), 히스테리시스/우선순위 빌트인. ⚠️ 과용 경계 — 매틱 슬롯계산/yield 세부는 C++ 핫패스
- **MovementIntent 조정 레이어** — 유닛별 커밋 모델을 단일 Resolver로 일반화. intent 우선순위(P0생존~P4홈) + 인터럽트 가능성 분리. 공격/회피/traversal/홈을 같은 언어(intent)로. 개관 문서가 전체 설계
- **Home + 슬롯-앵커 국소 흐름 (포텐셜 필드+그래디언트 D9)** — 닻(커밋 슬롯)은 이산 고정, 그 주변 좁은 반경만 작은 포텐셜로 흐르게. "물처럼 흐르는" 느낌. P4 HomeSlot intent의 일부 → "공격 중 안 흐름"이 우선순위로 자동. StateTree/MovementIntent 후
- **reluctance CommitTime 출발/도착 정밀화** — 현재 출발 시점에 찍혀 이동 길면 약해짐. 도착 이벤트(ReceiveMoveCompleted + FAIRequestID 대조)로 재찍기. 단 "출발 폴백 + 도착 덮어쓰기 + 도착대기 상태 안 만들기"로 프리징 방지. 적 이동 들어와야 검증 가능 (ADR-0003 §6)
- **타겟 선정 레이어** — Perception 기반 적 감지(현재 DebugPerceivedEnemies 수동). 존 앵커링("플레이어 교전 구역으로 후보 제약" — 거리 리쉬 대신). Threat/PullToAlly 체감이 이게 있어야 검증됨
- **전투 행동 (스킬 레이어, [3])** — 각 동료 자율 전투. 공격/스킬 모션, 사거리 기반 발동. 슬롯 Strategy가 사거리(캐릭터 아님) 받게 해둔 게 이걸 위한 포석
- **BossEvade Coordinator** — 보스 전체공격 회피. 파티 단위 안전위치 할당(개별 StateTree 단독 아님 — 같은 곳 몰림/입구 낑김 방지). 개관 D5
- **적 그룹 전투 — 전략을 종류에서 분리** (→ ADR-0006) — 그룹 전투 방침을 몬스터 종류에 고정 안 함. 개별 기질(교전거리/도망임계/공격성)은 몬스터, 그룹 진형(포위/산개/C)은 배치 데이터가. 스폰 초기배치(0007, 완료) *위에* 슬롯-닻 기억 → Target/Anchor 재배치 → Hold/Commit/Return을 쌓아 *전투중* 행동으로. **전투→StateTree→이것** 순. 핵심 계약: 그룹전략은 slot/target/token/posture "의도"만, GAS ability 직접 호출 금지
- **적 진형 (Flock 기반)** — 리더리스 집단 이동. `ATacticalCharacterBase`로 기반 재사용. 위 그룹전투와 별개 축(이건 *이동*, 위는 *전투 배치*)

### 🔴 추가 목표 (아이디어만. 다른 목표 모두 완료 시)
- **엄폐 평가 (cover/LoS)** — 영향맵에 "엄폐 될 수 있는 위치"가 후보로 살아남게는 됐지만, 엄폐 자체를 *평가*하진 않음. NavMesh cover / line-of-sight 점수축 추가
- 스킬 체이닝
- `UYieldStrategy_Narrow` — 통로용 flip (I진형 양보)
- NavLink-aware 점프

### ⚪ 낮은 우선순위
- NavMesh 가장자리 회피 (절벽/위험 — 현재 맵에선 잘 안 나와서 미룸. "플레이어가 밀면 동료가 절벽으로" 문제도 여기)


---

## 🧠 설계 결정 — 어필판(영/일)에 올린 핵심

> 영/일 README의 Key Design Decisions에 올린 것들. 여기선 목록만 — 풀이는 아래 + ADR.

1. PartyManager 별도 액터 분리 (시스템 수명을 캐릭터에 안 묶음)
2. Yield 진형 단위 (per-character 아님 — 공유 컨텍스트)
3. Yield Strategy + stateless (Asset Flyweight, OCP + 데이터 선택)
4. **전투 위치 = Home(기준점), 고정 슬롯 아님** (공격/회피가 삭제 아니라 일시 상회) → 개관
5. **결정/실행 분리 — "언제 옮길까"와 "어디로"** → ADR-0003
6. **방향 안 정하고 평가 위임 (360도 영향맵)** → ADR-0004
7. StateTree vs BT (commit형 모드엔 state machine)
8. (격하) Detour 회피를 컨트롤러가 결정 — 구현 기법이라 ADR/구현 영역으로
9. **적 스폰 인라인 조합 vs 재사용 Preset** (재사용≠직접편집, 그 자리 `+/-` 조합) + **이상위치/보정 책임 분리** → ADR-0007

---

## 🔑 핵심 의사결정 — 풀어서 (몇 주 뒤의 나에게)

> 키워드로만 적으면 까먹는다. 풀어쓴다. (ADR에 더 자세히 있는 건 링크만.)

**Yield 관련 (1)~(5)는 ADR-0002 + 아래 유지** — 자세한 건 `Docs/ADR/ko/0002...`

**(전투) Home 개념이 왜 중요한가**
전투 중 동료의 최종 위치 위엔 공격·스킬·회피·재배치가 다 겹친다. 슬롯 하나를 "여기 꼭 서라"로
두면 그 위에 예외가 쌓인다("공격 중이면 슬롯 추적 skip" 같은 if문이 진형 코드에 번짐). 그래서
슬롯을 *Home*(할 일 없을 때 수렴하는 기준점)으로 재정의. 공격/회피는 Home을 삭제 안 하고 우선순위만
일시 상회. 돌아갈 기준점이 항상 하나 유지됨. 이게 MovementIntent의 P4 HomeSlot으로 일반화됨.

**(전투) 왜 영향맵을 매틱 안 돌리고 게이트로 가뒀나**
영향맵은 "지금 어디가 좋은가" 답하는 *평가기(오라클)*다. 근데 매틱 돌려 슬롯을 재전달하면 두 개가
터진다 — ① 후보 0번이 "현재위치+보너스"라 이동 중 매틱 평가하면 ≈제자리 재선택 = 이동 중 정지
② (당시) FrontlineDir이 플레이어 기준이라 플레이어 돌면 평가 프레임 회전 = 정지 유닛 쫓겨남.
해결: 게이트가 "언제 옮길까"만 결정(트리거 시 1회), 그 사이 슬롯 잠금, 이동은 locomotion.
영향맵은 평가기 본분으로 복귀. 자세히 ADR-0003.

**(전투) sector 분산을 왜 버렸나 — 360도로 간 진짜 이유**
군집 풀려고 "동료끼리 각도 갈라주자(sector 분산)" 떠올림. 근데 이건 "가장 먼 빈 각도로 밀기" =
*최대 분산* = 군집의 반대 극단(양극단 찢어짐). 원한 건 "평소엔 서로 다른 위치"지 "최대한 멀리"가
아님. 한 발 물러서니 진짜 문제는 "방향을 하나 정해 좁은 부채꼴에 깐다"는 전제 자체 — 측면/후방이
후보에 아예 없고, 축이 뭘 기준하든(플레이어/적무게중심) 흔들림. 그래서 방향을 후보 생성에서 빼고
360도 + 점수 위임. 자세히 ADR-0004.

**(전투) 점프 결정권 — Battle에 점프 판단 없는 게 왜 결함 아닌가**
"Battle 컴포넌트에 점프 판단 없네 → 책임 위치 틀림 → 캐릭터로 통일하자"로 직행한 적 있음. 틀린
진단. 근거로 든 게 다 B(물리)였는데 그건 이미 캐릭터에 있음 — A(상황판단)를 캐릭터로 내릴 근거가
아님. "Battle에 A 없음"은 *미구현*이지 오배치 아님. 비대칭≠결함. 자세히 ADR-0001.

**(스폰) 왜 Preset 안 쓰고 Spawner 인라인으로 갔나**
처음엔 `Line+Line`/`Line+Circle` 조합을 Formation Preset 자산으로 만들어두고 스폰지점에서 *고르는*
구조가 자연스러워 보임. 근데 요구를 다시 보니 어긋남 — 원한 건 "C/D/E 만들어두고 선택"이 아니라
*이 스폰지점 안에서 직접 조합*. 어떤 지점은 전열만, 어떤 지점은 전열Line+후열Circle. Preset으로
하면 지점마다 조금씩 다른 배치 위해 복제·Override·Variant·동기화가 줄줄이 따라옴. 핵심 통찰:
"재사용"과 "직접편집"은 *다른 목표축*. Preset은 "같은 설정 여러 곳", 인라인은 "이 자리서 그때그때".
이번은 명백히 후자 → `FEnemyCompositeFormation`을 자산 아닌 Spawner 인라인으로. (자동 거리 그룹핑도
기각 — 디자이너가 "이 둘은 한 진형" 의도할 방법 없어짐. 0006의 "그룹경계는 배치가 만든다"와 같은 결.)
자세히 ADR-0007.

**(스폰) 왜 보정을 SubFormation 아니라 Spawner에 뒀나**
`Line`/`Circle`이 슬롯 만들면서 벽체크·NavMesh 투영까지 하면 두 개 터짐 — ① 배치형태가 월드충돌·
NavSystem까지 알아야 함(책임 비대) ② *모든* SubFormation에 같은 보정 코드 중복. 그래서 전략은
이상위치만, 보정은 Spawner 공통(링 후보 → Nav 투영 → AdjustIfPossible). 새 배치형태 추가가 *순수
위치계산 추가*로 줄고 보정은 공짜로 따라옴. 안전위치 못 찾으면 벽에 강제로 안 박고 포기+로그.
"시스템이 뭘 *안 하는가*를 정하는 것도 설계"의 또 다른 사례. 자세히 ADR-0007.

---

## ⚠️ 함정 노트 (실수 재발 방지)

### Detour / 컴포넌트 교체 (3주차)
- **SetDefaultSubobjectClass의 이름 인자는 "부모가 만드는 서브오브젝트의 고정 이름"** — `TEXT("PathFollowingComponent")`여야. 딴 이름은 무시되고 기본 컴포넌트 뜸
- **GetNameSafe(PFC)는 클래스명 아니라 인스턴스명** — 인스턴스명이 "PathFollowingComponent"라 교체 성공했는데 실패로 오인. 클래스 확인은 `GetClass()->GetName()`
- **GameMode에 C++ PlayerController 직접 꽂으면 spawn 실패** — IMC·위젯 같은 에셋 참조가 C++ CDO엔 빔. BP로 감싸야(BP_TacticalAIPlayerController). 버그 아니라 "BP에서 에셋 받아야 완성되는 클래스"
- **DefaultPawnClass를 BP_PartyCharacter로 지정 금지** — GameMode 자동 스폰이 PartyManager 리더 Possess랑 충돌. None으로 둬야 우리 흐름

### 회피 튜닝
- **AvoidanceQuality::High가 역효과** — 너무 정밀한 해를 찾아 "비켜서기"가 아니라 "딱 붙어 스치기"가 됨. RangeMultiplier 1.2가 자연스러움

### UE5 일반 (이번에 배움)
- **DataAsset에 Instanced UObject는 Outer 체인으로 GetWorld() 못 함** — UWorld* 명시적으로 넘겨야. 시그니처가 한계를 말해줌
- **DataAsset = Flyweight** — 같은 Asset 참조하는 두 Component는 Instanced 멤버(Strategy 포함) 공유. Strategy가 슬롯별 상태 들면 서로 박살 → stateless 필수, 상태는 Component
- **UPROPERTY `//` 주석이 UHT 툴팁 됨** (검증) — `///`나 `/** */` 불필요
- **`CallInEditor` 스폰은 게임월드 아니면 `RF_Transient` 줘야** — 안 주면 에디터 프리뷰로 스폰한 적이 레벨에 영구 오브젝트로 박혀 저장됨. `GetWorld()->IsGameWorld()` 체크 후 비게임월드면 `SpawnParams.ObjectFlags |= RF_Transient`. 조합→즉시확인 루프의 전제

---

## 🧗 내려가는 트래버설 — 설계 고민 (아직 결론 안 냄)

> NavLink 안 쓰는 중. 올라가기=점프 완료. 내려가기 선택지 박제.

**왜 어려운가**: 플레이어는 단차 내려갈 때 점프 안 하고 이동키로 걸어 떨어짐(올라갈 때만 점프).
동료가 점프로 내려가면 어색. 근데 "걸어 떨어지기"가 AI엔 어렵다 — MoveTo는 NavMesh 위에서만 도는데
NavMesh는 절벽 끝에서 끊김.

**선택지**:
- **A) NavLink로 연결** — 엔진 표준. 견고하지만 절벽마다 수동 배치, 동적 지형 안 됨
- **B) walk-off-ledge 상태** — 가장자리까지 MoveTo → MoveTo 끊고 AddMovementInput으로 NavMesh 밖 밀기 → 자동 낙하 → 착지 후 MoveTo 복귀. 동적·플레이어랑 동일하게 자연스러움. 단 가장자리 감지 직접 + NavMesh 밖 수동제어 구간 늘어남

**현재 기운 방향**: 내려가기는 점프 *아님*. 올라가기=점프, 내려가기=걸어 떨어지기, **의도적 비대칭**.
우리가 NavLink 안 쓰고 컴포넌트 직접 감지라 B가 결대로 맞는데, 가장자리 감지 복잡도가 고민. 맵 정적이면 A도 고려.

> ⚠️ 관찰: 엔드필드도 NavLink 드문드문 깔고 거기서 점프로 떨어지는 듯. 생각보다 정교하진 않음 —
> 연산 아끼고 유저가 유심히 안 본다 판단해 간소화했을지도.

---

## 🎬 영상 촬영 계획

- [x] V자 진형 평지 따라오기 (5초)
- [x] 좁은 통로 → I자 자동 전환 (5초)
- [x] 통로 나옴 → V자 복귀 (5초)
- [x] Yield 시연 — 플레이어가 동료 통과 시 비킴 (5초)
- [ ] 진형별 Yield 비교 — V (옆+뒤) vs I (없음 / 미래 Narrow) (10초)
- [ ] Detour 회피 시연 — 플레이어가 정지한 동료 들이밀면 비켜감 (5초)
- [ ] 상향 점프 트래버설 — 단차 올라가기 (5초)
- [ ] **전투 진형 시연** — Follow↔Battle 전환, 타겟 둘러싸기, 원거리 360도 배치 (신규, 우선)
- [ ] 헝가리안 매칭 비교 영상
- [ ] 환경 적응 종합 데모 (3분, 마일스톤 후)

---

## 🧭 일하는 방식 — 나에게 (작업 속도 못지않게 중요)

- **내 질문을 결정으로 추정하지 않게 — *탐색* 과 *결정* 분리. AI가 탐색에 같이 참여해야지 답 추정 안 함**
- **반쪽짜리 해결책 안 함. 트레이드오프 명확히 인지 후 *의식적 선택***
- ⭐ **메모는 키워드 말고 "왜"를 문장으로. 나중에 내가 기억안날 수 있다**
- **직감을 신뢰하고 끝까지 판다** — "배열 위화감"이 ADR-0002를, "Activate 정체"가 ADR-0003을 끌어냄. 코드 표면도 AI 확신도 진실 보장 안 함
- **ADR은 동결 아니라 근거 보존** — 근거 남기면 미래에 떳떳이 수정. 막을 건 "맥락 모른 채 되돌리기"뿐

---

## 🔗 영감

- 그랑블루 판타지 리링크 (4인 파티 액션)
- 아크나이츠 엔드필드 (동료 AI 자연스러움 기준선)
- Artificial Intelligence for Games(Ian Millington)

---

🇺🇸 [English](./README.en.md) | 🇯🇵 [日本語版](./README.md)