# 개인 로드맵 및 핵심 메모

> 외부용이 아닌, 개발자 본인의 진행 관리·사고 정리·아이디어 노트.
> ⚠️ 압축해서 적으면 몇 주 뒤에 내가 못 읽는다. 결정은 "왜"를 문장으로 풀어쓸 것.

---

## 🎯 큰 그림

**최종 목표**: 일본 게임업계 언리얼 C++ 게임플레이 프로그래머 복귀

**나의 차별점**: 동료 AI 시스템 (대학생 때부터의 꿈, 이전 BT/네비 경험 기반)

**1차 마일스톤 (두 달 내)**: 진형 + 환경 적응 + Yield + 영상 자료

---

## 📊 진척표

### ✅ 1주차 완료

- [x] 3-레이어 아키텍처 (Manager/Controller/Character)
- [x] V자 진형 추종
- [x] Sphere Sweep + VectorPlaneProject 슬라이딩
- [x] 스프링 기반 GapScale 동적 변화
- [x] Quaternion 지연 회전
- [x] 거리/회전 기반 캐시 갱신
- [x] 카메라 블락 수정
- [x] UFormationDataAsset 도입 (UPrimaryDataAsset + FFormationSlotData struct)
- [x] 환경 보정 재구조화 (NavMesh as primary truth, 4단계 헬퍼 분리)
- [x] 경사면을 벽으로 오인하는 버그 수정 (ImpactNormal.Z 임계값)
- [x] NavMesh raycast 기반 V↔I 자동 진형 전환 (히스테리시스)

### ✅ 2주차 완료

- [x] **Yield 시스템 Character → Component 마이그레이션** (formation-level policy 통찰)
- [x] **Reaction Time delay 도입** — 동시 반응 방지, 시간 차원 디자인의 첫 사례
- [x] **Yield 자연성 폴리싱** — entry delay + projected backward offset (cross-product 순서 버그 수정 포함)
- [x] **Per-slot 거리 기반 슬롯 캐시 트리거** — 리더 이동 기반 → 슬롯-플레이어 거리 기반. 결과 좌표보다 *진입 시점* 통제의 중요성 통찰
- [x] **Hysteresis 안전망** (PostEditChangeProperty + BeginPlay) — YieldExitRadius ≥ YieldEnterRadius 코드로 강제
- [x] **Velocity → Facing 기반 cone 판정** — 카메라/캐릭터 facing 분리 게임에서 의도 명확화
- [x] **Yield Strategy 분리** — UYieldStrategy 추상 + _Standard / _None 구체 + IYieldContextProvider 인터페이스. 진형 단위 알고리즘 선택 + 미래 Component 재사용
- [x] **Hungarian-Yield 충돌 수정** — Yielding 중 매칭 skip

### ✅ 3주차 완료 — RVO 졸업, Detour Crowd로

- [x] **RVO → Detour Crowd 마이그레이션** — RVO는 리액티브하고 그룹 인식이 없어서, 회전/진형 전환 후 동료들이 근접 슬롯을 두고 "비비적"댔음. Detour는 예측 기반 + 그룹 우선순위가 있어서 이걸 해결. (옛 Open Problem #3 해결)
- [x] **컨트롤러가 회피 역할을 결정하는 구조** — 아래 "3주차 핵심 의사결정"에 자세히. 한 줄 요약: 빙의(Possess)를 단일 진입점으로, 컨트롤러가 `ITacticalAvoidanceController` 인터페이스로 역할(Leader/Normal/Yielding)을 세팅.
- [x] **UPlayerCrowdAgentComponent** — 플레이어는 AIController가 없어서 Detour가 자동 인식 못 함 → crowd agent로 수동 등록하는 컴포넌트. Pawn에 영구 부착, 컨트롤러가 켜고 끔.
- [x] **UTacticalCrowdFollowingComponent** — 동료의 PathFollowing을 이걸로 교체(SetDefaultSubobjectClass). Detour 그룹 비트마스크(1=Leader/2=Normal/4=Yielding)로 우선순위 제어.
- [x] **Yield ↔ crowd 역할 동기화** — 슬롯이 Yielding 상태로 전이하는 그 시점에 crowd 그룹도 4로 바꿔줌. 좌표(어디로 갈지)랑 회피 그룹(누굴 피할지)이 따로 놀던 구멍 메움.
- [x] **회피 튜닝** — RangeMultiplier 1.2가 제일 자연스러움. AvoidanceQuality::High는 오히려 역효과(아래 함정 노트 참조).
- [x] **상향 점프 트래버설** — UTacticalTraversalComponent. 벽 인지 → 도약점 계산 → 베지어 조향 → 포물선 발사. **단 내려가기는 아직 미완** (아래 진행 중).

### 🚧 진행 중 / 다음

- [ ] **영상 촬영 (포폴 자료)** — ⭐ 이게 1순위. 코드 아무리 좋아도 면접관은 영상 5초를 README 100줄보다 먼저 본다. StateTree보다 먼저.
- [ ] **내려가는 트래버설** — 지금 takeoff 계산이 올라가기 전제라 내려가기가 깨짐. 점프로 할지 걸어 떨어지게 할지 설계 고민 중 (아래 자세히).
- [ ] `SlotCacheUpdateDistance` Asset 으로 이동 (I 진형 편향 해결)
- [ ] Yielding 중 재평가 (현재 진입 후 플레이어 추가 접근 무반응)
- [ ] 리더 스왑 실제 구현 (빙의 핸들러는 이미 깔아둠 → Possess만 부르면 됨)

### 📅 한 달 로드맵

- **1주차**: ✅ 진형 시스템 + DataAsset + I자 자동 전환
- **2주차**: ✅ Yield 시스템 + Strategy 패턴 분리
- **3주차**: ✅ RVO → Detour 마이그레이션 + 컨트롤러 기반 회피 + 상향 점프
- **4주차**: 영상 촬영 + 내려가기 트래버설 + (여유되면) StateTree 사전조사

> 원래 3주차 계획이 "헝가리안 정밀 + 카메라 분리 + 영상"이었는데, RVO→Detour 마이그레이션이 통째로 들어와서 밀림. 영상이 4주차로 넘어간 게 제일 아픔. **영상부터 했어야 했다 — 다음엔 "보여줄 수 있는 상태"를 먼저 만들고 기능 추가하자.**

---

## 🧠 설계 결정 — 내부 메모

### 일본어/영어 README 에 올린 중요한 것

1. PartyManager를 별도 액터로 분리
2. Sphere Sweep 채택 (vs Line Trace)
3. NavMesh as primary truth (환경 보정 재구조화)
4. Manager가 모드 결정 / Component가 모드 내부 결정 (추상화 레벨 분리)
5. 히스테리시스로 진형 전환 깜빡임 방지
6. 비동기 LineTrace 보류 (YAGNI)
7. **Yield 가 진형 단위 책임** — 모든 캐릭터 서로 Yield 의 한계 인지하고 선택
8. **Yield 의 Strategy 패턴 도입** — 진형별 알고리즘 다양성 표현
9. **Strategy stateless / 상태는 Component** — Asset Flyweight 패턴 인식 후 결정
10. **컨트롤러가 회피 역할 결정** — Hub 패턴 대신 인터페이스 (3주차, 아래 자세히)
11. **StateTree를 BT 대신 쓰는 이유** — 아직 미도입이지만 README에 근거 기록 (아래 자세히)

---

## 🔑 2주차 핵심 의사결정 — 풀어서 (몇 주 뒤의 나에게)

> 예전에 키워드로만 적어놨더니 "Asset Flyweight 인식 — Strategy 상태 가지면 충돌" 이런 게 뭔 소린지 까먹었다. 풀어쓴다.

**(1) "모든 캐릭터가 서로 Yield" 설계도 가능했는데 왜 안 했나**
각 캐릭터가 자기 주변 actor를 보고 알아서 비키는 방식. 이게 더 자유롭다 — 파티든 적이든 아무 두 캐릭터나 서로 비킬 수 있으니까. 근데 두 가지 문제로 버림. ① N×M 근접 판정이라 캐릭터 늘면 비용 폭발. ② 더 중요한 건 *공유 컨텍스트가 없음* — 같은 플레이어를 피하는 동료끼리 같은 자리로 몰리고, 같은 진형 안 캐릭터끼리 서로 Yield 해서 진형이 무너진다. 누가 "데드락 났네", "다들 다른 방향으로 비켜" 하고 조정할 중앙이 없음.

**(2) 그래서 Yield를 진형 단위로. 이게 무슨 트레이드오프냐**
Yield를 FormationFollowComponent에 둠 = **다른 진형끼리는 Yield 못 함**이라는 제약을 받아들인 것. 무관한 NPC가 지나가도 동료는 안 비킨다. 근데 이건 의도적 선택이고, 장르적으로도 맞다 — 엔드필드/리링크도 플레이어 파티만 Yield 하고 적은 안 비킨다. **"시스템이 안 하는 걸 정하는 것도 설계다"** 가 여기서 나온 깨달음. 다 표현하려는 설계는 각 기능이 약해진다.

**(3) "Asset Flyweight 인식" 이게 뭐였냐 — Strategy를 stateless로 만든 진짜 이유**
언리얼 DataAsset은 Flyweight 패턴이다 = 여러 Component가 같은 Asset을 참조하면 메모리상 인스턴스는 하나고, `Instanced` 멤버(Strategy 포함)도 공유된다. 그래서 만약 Strategy가 슬롯별 상태(Yielding 플래그, 타이머, 退避 좌표)를 들고 있으면 → 같은 진형 Asset 쓰는 두 Component가 그 상태까지 공유 = 서로 박살냄. 그래서 Strategy는 판단/계산만 하는 stateless로 두고, 슬롯별 상태(SlotYieldStates 등)는 전부 Component 측에 둠. Component마다 자기 복사본을 가지니까 Asset 공유해도 안전.

**(4) "UInterface 패턴" — IYieldContextProvider 왜 만들었나**
Strategy가 `UFormationFollowComponent`라는 구체 타입을 알면, 나중에 BattleFormationComponent 같은 게 생겼을 때 Strategy를 재사용 못 한다. 그래서 인터페이스(IYieldContextProvider)를 끼워서 Strategy는 "Context"만 받고 구체 Component 이름을 모르게 함. 미래에 다른 Component가 같은 인터페이스만 구현하면 기존 Strategy가 그대로 돌아간다.

---

## 🔑 3주차 핵심 의사결정 — 풀어서 (이게 제일 헷갈렸던 부분)

> 컨트롤러 vs Hub 논쟁으로 며칠 끌었다. 결론과 *왜*를 박제.

**(1) 문제가 뭐였나 — "플레이어만 처리가 따로 빠지는" 비대칭**
Detour는 AIController로 움직이는 애들만 자동으로 crowd 멤버로 본다. 근데 플레이어는 PlayerController가 빙의하지 AIController가 없다 → Detour가 플레이어를 모름 → 동료가 플레이어를 그냥 관통. 그래서 플레이어를 crowd agent로 *수동 등록*해야 하는데, "언제 등록을 켜고 끄냐"의 트리거가 빙의 이벤트(PossessedBy)다. 이게 Character 안으로 새서 "동료는 자동인데 플레이어만 Character가 특별 처리"하는 비대칭이 생김. 이게 싫었다.

**(2) 두 컴포넌트의 소유 주체가 다르다는 게 핵심**
- `UPlayerCrowdAgentComponent` → **Pawn**이 소유 (스왑해도 Pawn에 남음)
- `UTacticalCrowdFollowingComponent` → **AIController**가 소유 (PathFollowing이라 빙의 풀리면 같이 떨어짐)
  이 비대칭이 "분산"의 진짜 정체였다. 그래서 **스왑할 때 컴포넌트를 떼었다 붙였다 하면 안 된다** — 컴포넌트는 제자리 두고 역할(role)만 스위칭.

**(3) AI가 제안한 Hub 패턴 vs 내 인터페이스 — 왜 인터페이스로 갔나**
제미나이 안: Pawn에 "회피 Hub" 컴포넌트 하나 두고, 걔가 "지금 플레이어 빙의면 ObstacleComp 켜고, AI면 컨트롤러의 CrowdComp 만지고" 하고 라우팅.
이거 거부한 이유: Hub가 Pawn에 사는데 AI 케이스에서 *컨트롤러 내부 컴포넌트*를 건너가서 만진다 = **소유권 역행**. 그리고 Hub 안에 `if(플레이어) / else(AI)` 런타임 분기가 그대로 남는다 — 분기를 없앤 게 아니라 Pawn으로 옮겨 숨긴 것뿐.
내 안: 컨트롤러가 `ITacticalAvoidanceController` 인터페이스를 구현. 분기가 **컨트롤러 다형성**으로 사라짐(PlayerController는 자기 Pawn의 ObstacleComp 켜고, AIController는 자기 CrowdComp 만지고 — 각자 자기 것만). 빙의(Possess)가 단일 진입점이라 스왑 = 재빙의 = 역할 자동 정리. 추가 스왑 코드 0.

**(4) 제미나이가 든 Hub의 두 근거를 왜 기각했나** (이거 면접에서 물어보면 좋은 답)
- "멀티플레이어에서 AIController는 서버에만 있어서 클라에 상태 전파 못 한다 → Hub 필요" → **반박**: 클라가 알아야 하는 건 yield *애니/UI 같은 표현 상태*지 회피 그룹 마스크(시뮬레이션)가 아니다. 표현 상태는 그냥 Pawn에 Replicated 변수 하나 두면 됨. Hub 없이도 됨. 그리고 Hub도 그 변수 복제하는 작업량은 똑같다.
- "수레/굴림돌 같은 비-Pawn 장애물은 컨트롤러가 없어서 인터페이스 못 쓴다 → Hub 필요" → **반박**: 그건 이미 UPlayerCrowdAgentComponent가 하는 일(ICrowdAgentInterface 컴포넌트)과 같은 종류다. 수레에 그 컴포넌트 붙이면 됨. 게다가 Hub는 Pawn 전제라 비-Pawn은 애초에 못 담음 — Hub가 오히려 한계.
- 결론: 둘 다 인터페이스를 *버리는* 게 아니라 *위에 쌓는* 확장이다. 마이그레이션 아님.

**(5) 확장성 결정 — 인터페이스는 일찍, 베이스 클래스는 늦게**
Enemy 컨트롤러 나중에 생길 수도 있음. 그래서 `ITacticalAvoidanceController` 인터페이스는 지금 빼뒀다(거의 공짜, Enemy가 구현만 하면 기존 코드 수정 없이 확장). 근데 컨트롤러 공통 *베이스 클래스*는 안 만듦 — 아직 공유할 구현이 없어서 빈 껍데기가 된다. Enemy가 실제로 코드를 공유하게 될 때 추출해도 늦지 않음. **계약(인터페이스)은 일찍, 공유 구현(베이스)은 늦게.**

---

## ⚠️ 3주차 함정들 (다시 안 밟으려고 기록)

- **SetDefaultSubobjectClass의 이름 인자는 "내가 짓는 이름"이 아니라 "부모가 만드는 서브오브젝트의 고정 이름"이다.** `TEXT("PathFollowingComponent")` 여야 함. 딴 이름 넣으면 그냥 무시되고 기본 컴포넌트가 뜬다.
- **GetNameSafe(PFC)는 클래스명이 아니라 인스턴스명을 찍는다.** PathFollowing 인스턴스 이름이 "PathFollowingComponent"라서, 교체 성공했는데도 실패한 줄 알고 삽질. 클래스 확인은 `GetClass()->GetName()`.
- **GameMode에 C++ PlayerController 직접 꽂으면 spawn 실패** 날 수 있다. 입력 매핑(IMC)·위젯 클래스 같은 에셋 참조가 C++ CDO엔 비어서 그렇다. BP로 감싸서(BP_TacticalAIPlayerController) 꽂아야 함. 이건 버그가 아니라 "이 컨트롤러는 BP에서 에셋 받아야 완성되는 클래스"라서. → BP로 감싸는 게 정석.
- **DefaultPawnClass를 BP_PartyCharacter로 지정하면 안 됨.** GameMode가 폰 자동 스폰하려다 PartyManager의 리더 Possess랑 충돌. None으로 둬야 PartyManager가 리더 빙의하는 우리 흐름이 맞음.

---

## 🧗 내려가는 트래버설 — 설계 고민 (아직 결론 안 냄)

> 이거 결정하려다 멈춤. 선택지랑 각 트레이드오프 박제.

**왜 어려운가**: 플레이어는 단차 내려갈 때 점프 안 하고 그냥 이동키로 걸어 떨어진다(올라갈 때만 점프). 그러니 동료가 점프로 내려가면 어색함. 근데 "걸어 떨어지기"가 AI한텐 어렵다 — MoveTo는 NavMesh 위에서만 도는데, NavMesh는 절벽 끝에서 끊긴다. NavMesh 없는 구간을 걸어가야 하는데 MoveTo로는 못 감.

**선택지**:
- **A) NavLink로 내려가는 지점 연결** — 엔진 표준. 절벽 위/아래를 NavLink로 이으면 pathfinding이 인식하고 MoveTo가 경로에 포함시킴. 링크 통과 시 "걸어 떨어지기" 동작 넣음. 장점: 견고, NavMesh와 정합. 단점: 절벽마다 수동 배치, 동적 지형 안 됨.
- **B) walk-off-ledge 상태 추가** — 가장자리까지 MoveTo로 가고, 거기서 MoveTo 끊고 CharacterMovement에 직접 이동 입력(AddMovementInput)으로 NavMesh 밖으로 밀어내기 → 바닥 없으면 자동 낙하 → 착지하면 MoveTo 복귀. 장점: 동적, 플레이어랑 똑같이 자연스러움. 단점: 가장자리 감지 직접 해야 함(아래 트레이스), NavMesh 밖에서 잠깐 수동 제어 상태 늘어남.

**현재 기운 방향**: 내려가기는 점프 *아님*. 올라가기=점프, 내려가기=걸어 떨어지기, **의도적 비대칭**. 우리 시스템이 NavLink 안 쓰고 컴포넌트 직접 감지 방식이라 B가 결대로 맞는데, 가장자리 감지 복잡도 때문에 고민 중. 맵이 정적이면 A도 고려할 만함.

> ⚠️ 관찰: 엔드필드 플레이 관찰 결과 NavLink를 드문드문설치해 그 부분에서 점프로 떨어지고있다. 생각보다 정교하게 만들진 않은 듯 한데 연산 비용을 아끼고 플레이중 유저들이 유심히 관찰하지 않는다 판단하고 로직을 간소화한걸지도.

---

## 🎮 다음 큰 단계 — StateTree & 전투 (순서 주의)

> UE5 출시부터 때부터 StateTree 관심 있었음. 근데 순서가 중요하다.

**순서: 전투 진형 먼저 → 그다음 StateTree → 그다음 Flock.**

**왜 전투를 먼저?**
지금 State가 Idle(Follow) 하나뿐이다. StateTree를 지금 도입하면 노드 하나짜리 상태머신 = 의미 없는 ceremony. **전투 진형을 먼저 만들어서 "평시 ↔ 전투" 2개 상태 + 실제 전이 조건이 생긴 다음에야** StateTree가 풀 문제가 실재한다. 이게 내 기존 철학(if/else는 의도적 stepping stone, 실제 형태 드러낸 뒤 리팩터)과 맞물림. 전투가 그 "실제 형태 드러내기" 단계.

**왜 BT 아니고 StateTree? (README에도 적은 핵심)**
- 모드 선택 = "상태에 commit하는" 문제. 평시→전투 전이하면 거기 머물고 조건 바뀔 때만 전이. 이게 state machine 모델.
- BT는 매 틱 루트부터 재走査해서 leaf 찾음 = "지금 이 순간 뭘 할까" 리액티브 선택엔 맞지만 commit형 모드엔 안 맞음. BT에 모드 넣으면 blackboard 플래그로 상태 흉내내고 재진입 막느라 분기마다 가드 = 암묵적·취약한 상태머신.
- StateTree = BT의 Selector + state machine의 State/Transition 결합. 각 모드가 자기 진입조건/전이 소유, 복합 조건(좁음 AND 0.5초 AND 비전투) 명시적 노드, 히스테리시스/쿨다운/우선순위 빌트인, 유틸리티 선택(체력 따라 fight/flee 가중치) 기본 제공.
- ⚠️ **함정 경계: StateTree 과용하지 말 것.** 모드 전환 같은 상위 상태엔 맞지만, 매 틱 도는 슬롯 계산이나 yield 세부는 C++ 핫패스로 둬야 함. "어디 쓰고 어디 안 쓰나"를 아는 게 도구 쓸 줄 아는 것보다 세다.

**전투 진형 자체의 도전 (난이도순)**:
1. **타겟 기준 배치** — 평시는 리더 뒤에 줄 서기(leader-relative). 전투는 적 기준(target-relative)으로 둘러싸기/라인. CalculateIdealLocation의 기준을 "기준 actor + 방향"으로 추상화하면 평시/전투 같은 코드로. **OCP 실증 좋은 사례** — 전투 추가가 기존 코드 수정 없이 되는지.
2. **역할 기반 슬롯** — 탱커 앞, 딜러 뒤, 힐러 보호. Hungarian matching에 역할 가중치/제약 추가. 기존 알고리즘을 제약 하에 일반화.
3. **위협 기반 재배치** — 적이 힐러 붙으면 탱커 인터셉트, AOE 예고 회피. 야심차고 엣지케이스 많음 → **마지막에.**

> 자기경고: 다 만들려다 다 어중간 되는 게 제일 위험. 전투 진형 하나를 "평시→전투 확장" 완결 스토리로 깊게. Flock/스킬연계는 그 다음.

---

## 💡 아이디어 노트

### 수학/알고리즘 어필

- [x] 헝가리안 알고리즘: 진형 전환 시 최단 거리 매칭, O(N³)
- [ ] 회전 예측 보간: 트레일러 효과
- [ ] 다중 후보 평가: 막힌 슬롯에 8방향 후보, 점수로 최적 선택

### 자연스러움 어필

- [x] Detour Crowd 그룹 우선순위 (Leader/Normal/Yielding) — 3주차 완료
- [ ] 위험 지대 회피: NavMesh edge 감지 (절벽에서 떨어지지 않게) — 우선순위 낮춤(현 테스트맵에서 잘 안 나옴)
- [ ] 점프 가능 지점 인식: NavLink 활용
- [ ] 회전 종료 후 슬롯 복귀 보간
- [ ] Yielding 중 재평가 (현재 진입 후 무반응/자연스러움 올리기 위해 구현 우선순위 높음)

### 시스템 확장

- [ ] UAIPerceptionComponent (시야 시스템)
- [ ] StateTree 도입 (전투 진형 먼저 만든 뒤)
- [ ] **`UYieldStrategy_Narrow`** — I 진형의 "골목에서 뒤집기" 알고리즘 (벽 raycast + 통과 후 Hungarian 트리거)
- [ ] **`USlotGeneratorStrategy`** — 슬롯 정적 vs 절차적 생성 분리 (Yield Strategy 와 같은 패턴)
- [ ] **`ATacticalCharacterBase` 추상화** — Strategy 가 적/NPC 도입 시 그대로 재사용 가능

### 미래 (큰 단위)

- [ ] AMonsterGroupManager + UFormationFlockComponent (적 집단 AI, Leader 없는 진형)
- [ ] 플레이어 스킬 → 동료 연계 (시간 제한 윈도우)
- [ ] 캐릭터 스왑 시스템 (컨트롤러 교체 + 카메라 트랜지션) — 빙의 핸들러 이미 깔아둠
- [ ] 타겟팅 우선순위
- [ ] Leader 사망 처리 (현재 미정의 동작 — 시체 위치 기반 추종 가능성)

---

## ⚠️ 자기 경고

- AI가 주도하게 두지 말 것 → 내가 설명할 수 있을 때까지만 진행
- YAGNI → 미래에 필요할 수도 있는 기능 미리 도입 금지 (비동기 사례)
- 추상화 늦추기 → Rule of Three / 인터페이스는 일찍 베이스는 늦게
- 의도 있는 복잡도 vs 없어도 되는 복잡도 구분
- *코드 리뷰 / 결론 정리 / 책임 분리* 같은 멈춤이 시니어 사고. 작업 속도 못지 않게 중요
- **내 질문을 결정으로 추정하지 않게 — *탐색* 과 *결정* 분리. AI 가 *탐색에 같이 참여* 해야지 *답 추정* 안 함**
- **반쪽짜리 해결책 안 함. 트레이드오프 명확히 인지 후 *의식적 선택***
- ⭐ **메모는 키워드로 적지 말고 "왜"를 문장으로. 3주 뒤의 내가 못 읽는다** (이번에 2주차 메모 까먹고 다시 풀어쓴 교훈)
- ⭐ **보여줄 수 있는 상태(영상)를 먼저. 기능 추가는 그 다음.** (영상이 자꾸 뒤로 밀림)

---

## 🎬 영상 촬영 계획

- [x] V자 진형 평지 따라오기 (5초)
- [x] 좁은 통로 → I자 자동 전환 (5초)
- [x] 통로 나옴 → V자 복귀 (5초)
- [x] **Yield 시연** — 플레이어가 동료 통과 시 비킴 (5초)
- [ ] **진형별 Yield 비교** — V (옆+뒤) vs I (Yield 없음 / 미래 Narrow) (10초)
- [ ] **Detour 회피 시연** — 플레이어가 정지한 동료 들이밀면 비켜감 (5초) ← 3주차 신규
- [ ] **상향 점프 트래버설** — 단차 올라가기 (5초) ← 3주차 신규
- [ ] 헝가리안 매칭 비교 영상
- [ ] 환경 적응 종합 데모 (3분, 한 달 마일스톤 후)

---

## 🔗 영감

- 그랑블루 판타지 리링크 (4인 파티 액션)
- 아크나이츠 엔드필드 (동료 AI 자연스러움 기준선)
- AI Game Programming Wisdom 시리즈

---

🇺🇸 [English](./README.md) | 🇯🇵 [日本語版](./README.ja.md)