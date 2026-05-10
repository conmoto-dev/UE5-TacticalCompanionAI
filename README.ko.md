# 개인 로드맵 및 핵심 메모

> 외부 어필용이 아닌, 개발자 본인의 진행 관리·사고 정리·아이디어 노트.

---

## 🎯 큰 그림

**최종 목표**: 일본 게임업계 언리얼 C++ 게임플레이 프로그래머 복귀

**나의 차별점**: 동료 AI 시스템 (대학생 때부터의 꿈, 4년 전 BT/네비 경험 기반)

**1차 마일스톤 (두 달 내)**: 진형 + 캐릭터 스왑 + 환경 적응 + 영상 자료

---

## 📊 현재 상태

### ✅ 구현된 것
- 3-레이어 아키텍처 (Manager/Controller/Character)
- V자 진형 추종, Sphere Sweep + 슬라이딩 보정
- 스프링 기반 GapScale 동적 변화
- Quaternion 지연 회전
- NavMesh 투영 + 리더 견인 fallback
- 거리/회전 기반 캐시 갱신
- 자동 생성 클래스 활용 (TacticalAIGameMode/Character/PlayerController)

### 🚧 다음 우선순위
1. 카메라 블락 수정 (5분, 채널 정리)
2. UFormationDataAsset 만들기
3. I자 진형 추가
4. 환경 폭 측정 → 자동 진형 전환
5. 영상 촬영 (포폴 자료)

---

## 🧠 설계 결정 (왜 이렇게 만들었나) — 외부용 README엔 굵직한 것만 노출

### 1. PartyManager를 별도 액터로 분리
- 캐릭터 수명과 시스템 수명 분리
- 캐릭터 죽거나 스왑되어도 진형 유지
- 미래의 MonsterGroupManager와 같은 패턴

### 2. Line Trace → Sphere Sweep
- Line Trace는 두께 0이라 좁은 틈을 통과 가능으로 오판
- Sphere Sweep은 캐릭터 캡슐 반경과 동일하게 판정
- 실제로 캐릭터가 갈 수 있는지를 검증

### 3. 슬롯 막힘 처리: 진형 압축 vs 슬라이딩
- 진형 전체 압축: 동료 한 명만 막혀도 전체가 좁아져 부자연스러움
- 막힌 슬롯만 슬라이딩: 다른 동료는 자연스러움 유지
- 후자 선택 → VectorPlaneProject로 벽 법선에 수직 투영

### 4. 비동기 LineTrace 도입 보류
- 현재 동료 3명, 50cm 이동 임계값 → 성능 부담 거의 없음
- 비동기는 측정된 병목이 있을 때 도입 (YAGNI)
- 콜백 시점 처리 같은 방어 코드가 본 시스템 복잡도를 압도

### 5. ATacticalCharacterBase 추상 클래스 보류
- Rule of Three: 두 번째 구현체 (몬스터) 생길 때까지 보류
- 추측 기반 추상화는 잘못된 추상화로 이어짐

### 코드 레벨 결정 (README 미노출, 메모용)
- APartyCharacter 단일 클래스 (Player/Companion 분리 X) → 컨트롤러로 역할 결정
- CurrentLeader Pull 방식 (이벤트 Push X) → YAGNI
- 추상 베이스 클래스 보류 → Rule of Three
- 인터페이스 미리 안 만듦
- ABP_Companion 분리 안 함 → GroundSpeed만으로 충분
- CalculateIdealLocation에서 CurrentLeader 인자 제거 → 방어 과다 코드 제거
- CachedSlotLocations 고정 배열 → TArray (확장성)
- UFormationDataAsset 보류 + 미래 도입 (점진적 발전)

---

## 💡 아이디어 노트 (시도하고 싶은 것)

### 수학/알고리즘 어필
- [ ] 헝가리안 알고리즘: 진형 전환 시 최단 거리 매칭, O(N³)
- [ ] 회전 예측 보간: 트레일러 효과
- [ ] 다중 후보 평가: 막힌 슬롯에 8방향 후보, 점수로 최적 선택

### 자연스러움 어필
- [ ] 위험 지대 회피: NavMesh edge 감지
- [ ] 점프 가능 지점 인식: NavLink 활용
- [ ] 회전 종료 후 슬롯 복귀 보간

### 시스템 확장
- [ ] Detour Crowd Manager 학습 (한 주말 통째)
- [ ] UAIPerceptionComponent (시야 시스템)
- [ ] StateTree 도입 (UE 5.7 표준)

### 미래 (큰 단위)
- [ ] AMonsterGroupManager + UFormationFlockComponent
- [ ] 플레이어 스킬 → 동료 연계 (시간 제한 윈도우)
- [ ] 캐릭터 스왑 시스템 (컨트롤러 교체 + 카메라 트랜지션)
- [ ] 타겟팅 우선순위

---

## 📅 한 달 로드맵 (대략)

- **1주차 (지금)**: 진형 시스템 + DataAsset + I자 진형
- **2주차**: 헝가리안 매칭 + 카메라/입력 분리 리팩토링 + 영상
- **3주차**: 위험 지대 회피 + 회전 예측 + 영상
- **4주차**: BT/StateTree 학습 + 적 시야 시스템

**한 달 후**: 면접 지원 가능 수준의 1차 포폴 완성

---

## ⚠️ 자기 경고

- AI 복붙 기계 되지 말 것 → 내가 설명할 수 있을 때까지만 진행
- YAGNI → 미래에 필요할 수도 있는 기능 미리 도입 금지 (비동기 사례)
- 추상화 늦추기 → Rule of Three
- 의도 있는 복잡도 vs 없어도 되는 복잡도 구분

---

## 🎬 영상 촬영 계획

- [ ] V자 진형 따라오기 (기본)
- [ ] 좁은 통로 슬라이딩 보정
- [ ] 진형 자동 전환 (V → I)
- [ ] 환경 적응 종합 (3분 데모)
- [ ] 헝가리안 매칭 비교

---

## 🔗 영감

- 그랑블루 판타지 리링크
- 아크나이츠 엔드필드
- UE5 Detour Crowd Manager 학습 예정
- StateTree (UE 5.7+) 학습 예정
- AI Game Programming Wisdom 시리즈