# 개인 로드맵 및 핵심 메모

> 외부용이 아닌, 개발자 본인의 진행 관리·사고 정리·아이디어 노트.

---

## 🎯 큰 그림

**최종 목표**: 일본 게임업계 언리얼 C++ 게임플레이 프로그래머 복귀

**나의 차별점**: 동료 AI 시스템 (대학생 때부터의 꿈, 4년 전 BT/네비 경험 기반)

**1차 마일스톤 (한 달 내)**: 진형 + 환경 적응 + 영상 자료 → 면접 지원 가능 수준

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
- [x] **UFormationDataAsset 도입** (UPrimaryDataAsset + FFormationSlotData struct)
- [x] **환경 보정 재구조화** (NavMesh as primary truth, 4단계 헬퍼 분리)
- [x] **경사면을 벽으로 오인하는 버그 수정** (ImpactNormal.Z 임계값)
- [x] **NavMesh raycast 기반 V↔I 자동 진형 전환** (히스테리시스)

### 🚧 진행 중 / 다음

- [ ] 영상 촬영 (포폴 자료) ← *오늘 중*
- [ ] 헝가리안 알고리즘 (2주차)
- [ ] 카메라/입력을 PlayerController로 분리 리팩토링 (2주차)

### 📅 한 달 로드맵

- **1주차**: ✅ 진형 시스템 + DataAsset + I자 자동 전환
- **2주차**: 헝가리안 매칭 + 카메라/입력 분리 리팩토링 + 영상
- **3주차**: NavMesh edge 감지 + 회전 예측 폴리싱 + 영상
- **4주차**: BT/StateTree 학습 + 적 시야 시스템 (UAIPerceptionComponent)

---

## 🧠 설계 결정 — 내부 메모

### 외부 README 에 노출된 굵직한 것

1. PartyManager를 별도 액터로 분리
2. Sphere Sweep 채택 (vs Line Trace)
3. NavMesh as primary truth (환경 보정 재구조화)
4. Manager가 모드 결정 / Component가 모드 내부 결정 (추상화 레벨 분리)
5. 히스테리시스로 진형 전환 깜빡임 방지
6. 비동기 LineTrace 보류 (YAGNI)

### 코드 레벨 결정 (외부 미노출)

- APartyCharacter 단일 클래스 → 컨트롤러로 역할 결정
- CurrentLeader Pull 방식 (이벤트 Push X) → YAGNI
- 추상 베이스 클래스 보류 → Rule of Three
- 인터페이스 미리 안 만듦
- ABP_Companion 분리 안 함 → GroundSpeed만으로 충분
- CalculateIdealLocation에서 CurrentLeader 인자 제거 → 방어 과다 코드 제거
- CachedSlotLocations 고정 배열 → TArray (확장성)
- ImpactNormal.Z 임계값 0.7 (≈45도) → UE NavMesh agent slope 기본값과 일치
- TryFindGroundZ 트레이스 범위 500 → 다른 층 오검출 방지 (2000 은 과함)

---

## 💡 아이디어 노트

### 수학/알고리즘 어필

- [ ] 헝가리안 알고리즘: 진형 전환 시 최단 거리 매칭, O(N³)
- [ ] 회전 예측 보간: 트레일러 효과
- [ ] 다중 후보 평가: 막힌 슬롯에 8방향 후보, 점수로 최적 선택

### 자연스러움 어필

- [ ] 위험 지대 회피: NavMesh edge 감지 (절벽에서 떨어지지 않게)
- [ ] 점프 가능 지점 인식: NavLink 활용
- [ ] 회전 종료 후 슬롯 복귀 보간

### 시스템 확장

- [ ] Detour Crowd Manager 학습 (한 주말 통째)
- [ ] UAIPerceptionComponent (시야 시스템)
- [ ] StateTree 도입 (UE 5.7 표준, 평시/전투/양보 의사결정)

### 미래 (큰 단위)

- [ ] AMonsterGroupManager + UFormationFlockComponent (적 집단 AI)
- [ ] 플레이어 스킬 → 동료 연계 (시간 제한 윈도우)
- [ ] 캐릭터 스왑 시스템 (컨트롤러 교체 + 카메라 트랜지션)
- [ ] 타겟팅 우선순위

### StateTree 도입 결정 (4주차 작업)

지금 if/else 자동 전환의 한계를 *직접 경험*했음:
- 모서리 케이스에서 의도하지 않은 발동
- 진형 추가 시 분기 누적 우려
- 측정 + 결정 + 실행이 한 함수에 섞임

→ StateTree 로 *측정/결정/실행 분리*. 새 진형 = 새 노드 추가만.
→ 복합 조건 (좁음 + 지속시간 + 비전투) 을 명시적 트리로 표현 가능.

---

## ⚠️ 자기 경고

- AI가 주도하게 두지 말 것 → 내가 설명할 수 있을 때까지만 진행
- YAGNI → 미래에 필요할 수도 있는 기능 미리 도입 금지 (비동기 사례)
- 추상화 늦추기 → Rule of Three
- 의도 있는 복잡도 vs 없어도 되는 복잡도 구분
- *코드 리뷰 / 결론 정리 / 책임 분리* 같은 멈춤이 시니어 사고. 작업 속도 못지 않게 중요

---

## 🎬 영상 촬영 계획

- [ ] V자 진형 평지 따라오기 (5초)
- [ ] 좁은 통로 → I자 자동 전환 (5초)
- [ ] 통로 안 진행 (10초)
- [ ] 통로 나옴 → V자 복귀 (5초)
- [ ] 헝가리안 매칭 비교 영상 (구현 후)
- [ ] 환경 적응 종합 데모 (3분, 한 달 마일스톤 후)

---

## 🔗 영감

- 그랑블루 판타지 리링크 (4인 파티 액션)
- 아크나이츠 엔드필드 (동료 AI 자연스러움 기준선)
- AI Game Programming Wisdom 시리즈