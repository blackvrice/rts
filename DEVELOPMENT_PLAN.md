# Development Plan

현재 완료된 시스템을 기반으로 플레이 가능한 RTS 게임으로 발전시키기 위한 단계별 계획.

---

## Phase 1 — 전투 완성 (Combat Polish)

현재 전투는 동작하지만 데이터와 피드백이 빈약하다.

### 1-1. 유닛 정적 데이터 (UnitStaticData) 실용화
- `UnitStaticData`에 실제 스탯 필드 추가: `displayName`, `maxHp`, `attackDamage`, `attackRange`, `attackCooldown`, `moveSpeed`, `armor`
- `Unit` 생성 시 `UnitStaticData` 프로토타입에서 스탯을 읽어오도록 변경
- HUD 선택 패널에 실제 공격력·방어력·사거리 표시 (follow-up: Selected Unit HUD Details)
- 파일: `include/core/data/UnitStaticData.hpp`, `src/core/model/Unit.cpp`, `src/game/game/GameLogicManager.cpp`

### 1-2. 커서 상태 변형
- 공격 가능한 적 위에 마우스가 올라가면 `Cursor_02.png`로 교체
- 드래그 선택 중 `Cursor_03.png`, 이동 명령 중 `Cursor_04.png`
- 파일: `src/platform/sfml/SfmlRenderer.cpp` 또는 HUD 오버레이

### 1-3. Patrol 커맨드 구현
- `PatrolCommand` 로직 연결: A↔B 사이를 반복 이동
- 경로 상 적 발견 시 공격 후 복귀 (attack-move 패턴)
- 파일: `src/game/game/systems/MovementSystem.cpp`, `GameLogicManager.cpp`

### 1-4. Attack-Move 커맨드 구현
- 이동 중 경로 상의 적을 자동 공격 후 계속 이동
- 파일: `include/core/model/IGameElement.hpp`, `Unit.cpp`, `MovementSystem.cpp`

---

## Phase 2 — 유닛 다양화 (Unit Variety)

한 종류 유닛만으로는 전략이 없다.

### 2-1. 복수 유닛 타입
- `UnitType` enum 정의: `Warrior`, `Archer`, `Worker`
- 각 타입별 `UnitStaticData` 프로토타입 등록
- `UnitFactory`가 타입에 따라 스탯·스프라이트를 다르게 세팅
- 파일: `include/core/data/UnitStaticData.hpp`, `include/core/factory/UnitFactory.hpp`

### 2-2. Worker 유닛 기초
- `Worker` 타입: 채집·건설 명령 가능
- `gather(IGameElement*)` 실제 구현 (현재 empty override)
- 자원 노드로 이동 → 채집 → 마을회관으로 복귀 루프
- 파일: `src/core/model/Unit.cpp`, `GameLogicManager.cpp`

### 2-3. 원거리 공격 지원
- 사거리가 되면 이동 없이 원거리 공격 (투사체는 즉시 처리 or 간단한 projectile)
- `Archer` 타입에 긴 사거리 적용
- 파일: `src/core/model/Unit.cpp`, `updateAttack()`

---

## Phase 3 — 자원 & 경제 (Economy)

자원 없이는 생산·건설·승패 조건이 없다.

### 3-1. 자원 노드 엔티티
- `ResourceNode` 클래스: `IGameElement` 구현, 금광·나무 타입
- 세계에 자원 노드 배치 (초기에는 하드코딩, 이후 맵 로딩에서)
- 파일: 새 `include/core/model/ResourceNode.hpp`, `src/core/model/ResourceNode.cpp`

### 3-2. 채집 루프 완성
- Worker가 자원 노드 클릭 시 채집 명령 발행
- 채집량 누적 → 마을회관 인접 시 `GameWorld::setPlayerResources()`로 골드·나무 증가
- HUD 자원 수치 실시간 반영 (이미 `PlayerResourceState` 파이프라인 존재)
- 파일: `GameLogicManager.cpp`, `src/game/game/systems/` 신규 GatherSystem

### 3-3. 자원 소비 (생산·건설 비용)
- `UnitStaticData`에 `goldCost`, `woodCost`, `foodCost` 추가
- 유닛 훈련·건물 건설 시 `PlayerResourceState`에서 차감
- 잔액 부족 시 명령 거부

---

## Phase 4 — 건물 (Buildings)

건물 없이는 유닛 생산·베이스 개념이 없다.

### 4-1. 건물 엔티티 기초
- `Building` 클래스: `IGameElement` 구현, 고정 위치, 타일 풋프린트(NxM)
- `BuildingStaticData`: 체력, 크기, 종류 (TownHall, Barracks, Farm)
- 건물은 이동 불가, 충돌 시스템에 풋프린트로 등록
- 파일: 새 `include/core/model/Building.hpp`

### 4-2. 건물 배치 (Build 명령)
- Worker `build(int buildingType, Vector2D pos)` 실제 구현
- HUD Build 버튼 클릭 → 배치 모드 → 클릭한 위치에 건물 생성
- 자원 차감, 빈 타일 검증
- 파일: `GameLogicManager.cpp`, `GameUIManager`, `SfmlHudOverlay`

### 4-3. 건물 선택 & HUD
- 건물 선택 시 HUD 커맨드 패널에 건물 전용 버튼 (Train, Research 등)
- 건물 체력 표시, 건물 포트레이트

### 4-4. 유닛 생산 큐
- `Building`이 유닛 생산 큐(`std::deque<UnitType>`)를 가짐
- Train 명령으로 큐에 추가, 생산 시간 틱다운 후 건물 밖에 유닛 스폰
- HUD에 생산 큐 아이콘 표시 (StarCraft 스타일)
- 파일: `Building.cpp`, `GameLogicManager.cpp`

---

## Phase 5 — 맵 & 시야 (Map & Vision)

### 5-1. 맵 로딩
- tmxlite 이미 연결됨 — `.tmx` 파일에서 타일맵 로딩
- 타일 레이어 렌더링, 충돌 레이어에서 이동 불가 타일 설정
- 파일: `src/core/world/`, `src/platform/sfml/SfmlRenderer.cpp`

### 5-2. 카메라 이동
- 화면 가장자리 스크롤, 미니맵 클릭 이동
- 현재 카메라 시스템 활용 (`include/core/manager`)

### 5-3. 전장의 안개 (Fog of War)
- 탐색한 타일과 현재 시야 타일 구분
- 미탐색: 완전 검정 / 탐색 후 시야 밖: 어둡게 / 시야 내: 정상
- 적 유닛은 시야 내에만 렌더링

---

## Phase 6 — 게임 규칙 & AI

### 6-1. 승패 조건
- 상대 마을회관(TownHall) 파괴 시 승리
- 내 마을회관 파괴 시 패배
- 게임 종료 화면 (재시작 / 메인메뉴)

### 6-2. 간단한 AI 플레이어
- Enemy 팀 AI: 초기 유닛으로 플레이어 마을회관 공격
- Worker AI: 자원 채집 루프
- 이후 생산 AI로 확장

### 6-3. 씬 전환 완성
- Login → Lobby → Game → Game Over 흐름 완성
- 현재 Login/Lobby 씬은 골격만 존재

---

## Phase 7 — 폴리싱

- 유닛 피격·사망 애니메이션
- 공격 효과음, 이동 효과음
- 선택 링 / 체력바 개선
- Repair 명령 실제 구현 (Worker가 건물 수리)
- HUD/world 스냅샷 API 정리 (렌더 스레드 lock 아키텍처 개선)

---

## 현재 미완성 Follow-up 요약

| 항목 | 출처 | 해당 Phase |
|------|------|------------|
| 커서 상태 변형 (Cursor_02~04) | Sprite Mouse Cursor | Phase 1-2 |
| 유닛 실제 스탯 (armor, damage 등) HUD 표시 | Selected Unit HUD Details | Phase 1-1 |
| Patrol 커맨드 로직 | StarCraft Hotkeys | Phase 1-3 |
| 경제 명령 (채집·지출·공급) | Player Resource HUD | Phase 3 |
| 건물 이동 차단 풋프린트 | Movement Stops | Phase 4-1 |
| Patrol·Train·Build·Gather·Repair UI → LogicCommand 완성 | HUD Gameplay Input | Phase 2-2, 3-2, 4-2 |
| HUD/World 스냅샷 API 정리 | Render Lock Fix | Phase 7 |
