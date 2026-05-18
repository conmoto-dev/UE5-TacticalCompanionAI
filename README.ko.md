# 개인 로드맵 및 핵심 메모

> 외부용이 아닌, 개발자 본인의 진행 관리·사고 정리·아이디어 노트.

---

## 🎯 큰 그림

**최종 목표**: 일본 게임업계 언리얼 C++ 게임플레이 프로그래머 복귀

**나의 차별점**: 동료 AI 시스템 (대학생 때부터의 꿈, 이전 BT/네비 경험 기반)

**1차 마일스톤 (한 달 내)**: 진형 + 환경 적응 + Yield + 영상 자료 → 면접 지원 가능 수준

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

### 🚧 진행 중 / 다음

- [ ] 영상 촬영 (포폴 자료)
- [ ] `SlotCacheUpdateDistance` Asset 으로 이동 (I 진형 편향 해결)
- [ ] Yielding 중 재평가 (현재 진입 후 플레이어 추가 접근 무반응)
- [ ] 카메라/입력을 PlayerController로 분리 리팩토링

### 📅 한 달 로드맵

- **1주차**: ✅ 진형 시스템 + DataAsset + I자 자동 전환
- **2주차**: ✅ Yield 시스템 + Strategy 패턴 분리
- **3주차**: 헝가리안 매칭 정밀 + 카메라/입력 분리 리팩토링 + 영상
- **4주차**: StateTree 학습 + 적 시야 시스템 (UAIPerceptionComponent)

---

## 🧠 설계 결정 — 내부 메모

### 일본어/영어 README 에 올린 중요한 것

1. PartyManager를 별도 액터로 분리
2. Sphere Sweep 채택 (vs Line Trace)
3. NavMesh as primary truth (환경 보정 재구조화)
4. Manager가 모드 결정 / Component가 모드 내부 결정 (추상화 레벨 분리)
5. 히스테리시스로 진형 전환 깜빡임 방지
6. 비동기 LineTrace 보류 (YAGNI)
7. **Yield 가 진형 단위 책임 — 모든 캐릭터 서로 Yield 의 한계 인지하고 선택**
8. **Yield 의 Strategy 패턴 도입 — 진형별 알고리즘 다양성 표현**
9. **Strategy stateless / 상태는 Component — Asset Flyweight 패턴 인식 후 결정**

### 코드 레벨 결정 (여기서만)

- APartyCharacter 단일 클래스 → 컨트롤러로 역할 결정
- CurrentLeader Pull 방식 (이벤트 Push X) → YAGNI
- 추상 베이스 클래스 보류 → Rule of Three
- 인터페이스 미리 안 만듦 (IYieldContextProvider 가 첫 인터페이스 — *진짜 필요* 시점에 도입)
- ABP_Companion 분리 안 함 → GroundSpeed만으로 충분
- CalculateIdealLocation에서 CurrentLeader 인자 제거 → 방어 과다 코드 제거
- CachedSlotLocations 고정 배열 → TArray (확장성)
- ImpactNormal.Z 임계값 0.7 (≈45도) → UE NavMesh agent slope 기본값과 일치
- TryFindGroundZ 트레이스 범위 500 → 다른 층 오검출 방지 (2000 은 과함)
- **Yield Strategy 의 World 접근 불가 → TryProjectToNavMesh 시그니처에 World 명시** — 한계 자체를 코드로 표현
- **Strategy 가 APartyCharacter 직접 의존 → 미래 ATacticalCharacterBase 추상화 필요성 인지**

### Yield Strategy 도입 배경 (2주차 작업)

직접 부딪힌 한계가 *Strategy 패턴 도입의 진짜 동기*:

- Component 안에 Yield 로직 박혀있어서, 진형마다 다른 Yield (V vs I) 표현 못 함
- I 진형 의도 = "좁은 골목에서 뒤집기" — V 의 *옆+뒤 비킴* 으로 표현 불가. *알고리즘 자체* 가 다름
- 디자이너가 진형마다 Yield 선택 가능하게 하고 싶음

→ Strategy 분리. Component 의 알고리즘 → Strategy 클래스. DataAsset 에서 드롭다운 선택.

**중간 혼란 후 도달한 핵심 의사결정** (이거 면접 자료):
- *모든 캐릭터 서로 Yield* 의 더 자유로운 설계 가능 — 근데 *오버헤드 + 진형 의미 무너짐* 의 한계
- *플레이어 파티 안 Yield 만* 의 의도적 선택. *시스템이 *안 하는 거* 결정* 도 디자인.
- *Asset Flyweight 인식* — Strategy 가 상태 가지면 *공유 시 충돌*. 상태 Component 측.
- *UInterface 패턴* — Strategy 가 *구체 Component 모름*. 미래 BattleComponent 재사용.

### 시간 차원 디자인의 깨달음 (Yield 작업 중)

수학으로 안 풀리는 문제는 *시간 모델링* 으로:
- Reaction Time delay → 동시 반응 분산
- Per-slot 거리 트리거 → Yield 끝 시점 슬롯 위치 통제 (결과 좌표 < 진입 시점)

이 패턴 자체 = *디자인 도구*. 미래 작업에서 *Hungarian 매칭 발동 조건*, *진형 전환 타이밍* 등에 응용 가능.

---

## 💡 아이디어 노트

### 수학/알고리즘 어필

- [x] 헝가리안 알고리즘: 진형 전환 시 최단 거리 매칭, O(N³)
- [ ] 회전 예측 보간: 트레일러 효과
- [ ] 다중 후보 평가: 막힌 슬롯에 8방향 후보, 점수로 최적 선택

### 자연스러움 어필

- [ ] 위험 지대 회피: NavMesh edge 감지 (절벽에서 떨어지지 않게)
- [ ] 점프 가능 지점 인식: NavLink 활용
- [ ] 회전 종료 후 슬롯 복귀 보간
- [ ] Yielding 중 재평가 (현재 진입 후 무반응)

### 시스템 확장

- [ ] Detour Crowd Manager 학습 (한 주말 통째)
- [ ] UAIPerceptionComponent (시야 시스템)
- [ ] StateTree 도입 (UE 5.7 표준, 평시/전투/양보 의사결정)
- [ ] **`UYieldStrategy_Narrow`** — I 진형의 "골목에서 뒤집기" 알고리즘 (벽 raycast + 통과 후 Hungarian 트리거)
- [ ] **`USlotGeneratorStrategy`** — 슬롯 정적 vs 절차적 생성 분리 (Yield Strategy 와 같은 패턴)
- [ ] **`ATacticalCharacterBase` 추상화** — Strategy 가 적/NPC 도입 시 그대로 재사용 가능

### 미래 (큰 단위)

- [ ] AMonsterGroupManager + UFormationFlockComponent (적 집단 AI, Leader 없는 진형)
- [ ] 플레이어 스킬 → 동료 연계 (시간 제한 윈도우)
- [ ] 캐릭터 스왑 시스템 (컨트롤러 교체 + 카메라 트랜지션)
- [ ] 타겟팅 우선순위
- [ ] Leader 사망 처리 (현재 미정의 동작 — 시체 위치 기반 추종 가능성)

### StateTree 도입 결정 (4주차 작업)

지금 if/else 자동 전환의 한계를 *직접 경험*했음:
- 모서리 케이스에서 의도하지 않은 발동
- 진형 추가 시 분기 누적 우려
- 측정 + 결정 + 실행이 한 함수에 섞임

→ StateTree 로 *측정/결정/실행 분리*. 새 진형 = 새 노드 추가만.
→ 복합 조건 (좁음 + 지속시간 + 비전투) 을 명시적 트리로 표현 가능.

Yield Strategy 작업 후 — *Strategy 패턴 + StateTree 결합* 그림 명확. Strategy = *각 상태의 행동*, StateTree = *상태 간 전이*.

---

## ⚠️ 자기 경고

- AI가 주도하게 두지 말 것 → 내가 설명할 수 있을 때까지만 진행
- YAGNI → 미래에 필요할 수도 있는 기능 미리 도입 금지 (비동기 사례)
- 추상화 늦추기 → Rule of Three
- 의도 있는 복잡도 vs 없어도 되는 복잡도 구분
- *코드 리뷰 / 결론 정리 / 책임 분리* 같은 멈춤이 시니어 사고. 작업 속도 못지 않게 중요
- **내 질문을 결정으로 추정하지 않게 — *탐색* 과 *결정* 분리. AI 가 *탐색에 같이 참여* 해야지 *답 추정* 안 함** (Yield Strategy 도입 토론 중 패턴 발견)
- **반쪽짜리 해결책 안 함. 트레이드오프 명확히 인지 후 *의식적 선택*** (Yield 진형 단위 선택의 본질)

---

## 🎬 영상 촬영 계획

- [ ] V자 진형 평지 따라오기 (5초)
- [ ] 좁은 통로 → I자 자동 전환 (5초)
- [ ] 통로 안 진행 (10초)
- [ ] 통로 나옴 → V자 복귀 (5초)
- [ ] **Yield 시연** — 플레이어가 동료 통과 시 비킴 (5초)
- [ ] **진형별 Yield 비교** — V (옆+뒤) vs I (Yield 없음 / 미래 Narrow) (10초)
- [ ] 헝가리안 매칭 비교 영상
- [ ] 환경 적응 종합 데모 (3분, 한 달 마일스톤 후)

---

## 🔗 영감

- 그랑블루 판타지 리링크 (4인 파티 액션)
- 아크나이츠 엔드필드 (동료 AI 자연스러움 기준선)
- AI Game Programming Wisdom 시리즈