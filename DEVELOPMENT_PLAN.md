# Development Plan

현재 체크아웃과 작업 트리 기준으로 이미 개발된 것과 추가 개발이 필요한 것을 함께 표시한 RTS 개발 계획.

## 상태 표기

- `[완료]`: 현재 코드 경로에서 기능이 연결되어 실제 런타임 동작으로 사용 가능.
- `[부분 완료]`: 모델, 명령, UI, 렌더, 또는 데이터 골격은 있으나 게임 루프까지 완성되지 않음.
- `[필요]`: 아직 구현이 없거나 TODO/placeholder 수준.

---

## 현재 이미 개발된 핵심 기반

- `[완료]` `LogicCommandBus`, `UICommandBus`, `GameplayInputCommand` 기반 UI 입력 -> 로직 명령 흐름.
- `[완료]` 드래그 선택, 컨트롤 그룹, 우클릭 이동/공격 명령, 스타식 명령 단축키.
- `[완료]` 카메라 매니저, 월드 렌더 뷰, screen/world 좌표 변환, 방향키/화면 가장자리 스크롤.
- `[완료]` 유닛 이동, pathfinding, 동적 충돌, 점유된 목적지 근처 접근/정지 처리.
- `[완료]` 기본 전투 틱: 공격 대상 추적, 사거리 진입 후 공격, 쿨다운 피해, 사망 처리.
- `[완료]` 팀 기반 우클릭 판정: 같은 팀은 이동, 상대 팀은 공격.
- `[완료]` 공격 추격 중 move 애니메이션, 사거리 진입 후 attack 애니메이션 전환.
- `[완료]` 플레이어 자원 상태 저장 모델과 HUD 자원 표시 파이프라인.
- `[완료]` 선택 유닛 HUD 기본 정보: 선택 수, 이름, 행동, HP, 위치.
- `[부분 완료]` 커서 상태 변형: 작업 트리 기준으로 attack/drag/move 커서 경로가 추가됨. 자동 빌드 검증과 커밋 여부는 별도 확인 필요.
- `[부분 완료]` `ResourceNode`, `Building`, `FogOfWar`, `BuildingViewModel`, `ResourceNodeViewModel` 골격 존재.

---

## Phase 1 - 전투 완성 (Combat Polish)

현재 전투는 기본 동작이 있지만 데이터 주도 스탯, 커맨드 다양성, 피드백이 더 필요하다.

### 1-1. `[부분 완료]` 유닛 정적 데이터 (`UnitStaticData`) 실용화

이미 개발된 것:
- `Unit`에 `maxHp`, `attackDamage`, `attackRange`, `attackCooldown`, `moveSpeed` 값이 존재하고 전투에 사용됨.
- `UnitStaticData` 골격 파일이 있음.
- HUD 선택 패널은 현재 선택 유닛의 HP, 행동, 위치를 표시함.

추가 개발 필요:
- `UnitStaticData`에 `displayName`, `maxHp`, `attackDamage`, `attackRange`, `attackCooldown`, `moveSpeed`, `armor`, 비용 필드를 정리.
- `Unit` 생성 시 하드코딩 스탯 대신 `UnitStaticData` 또는 타입별 프로토타입에서 스탯을 읽도록 변경.
- HUD 선택 패널에 실제 공격력, 방어력, 사거리, 유닛 이름 표시.

관련 파일:
- `include/core/data/UnitStaticData.hpp`
- `include/core/model/Unit.hpp`
- `src/core/model/Unit.cpp`
- `src/game/game/GameUIManager.cpp`

### 1-2. `[부분 완료]` 커서 상태 변형

이미 개발된 것:
- 작업 트리 기준 `UpdateHudCursor`, `Cursor_02.png`, `Cursor_03.png`, `Cursor_04.png` 연결 코드가 있음.
- `GameUIManager`가 드래그 상태, 이동 명령 모드, 적 hover 상태에 따라 커서 ID를 고르는 코드가 있음.

추가 개발 필요:
- 현재 변경분의 빌드/실행 검증 및 커밋 상태 확정.
- 적 hover 판정을 유닛뿐 아니라 공격 가능한 건물까지 확장.
- HUD/월드 위 커서 우선순위와 UI hover 예외 처리 정리.

관련 파일:
- `include/core/render/RenderCommand.hpp`
- `src/game/game/GameUIManager.cpp`
- `src/platform/sfml/SfmlRenderManager.cpp`

### 1-3. `[필요]` Patrol 커맨드 구현

이미 개발된 것:
- `PatrolCommand`와 UI 입력/단축키 액션은 존재함.
- `GameLogicManager`에 `PatrolCommand` 라우터는 있으나 현재 TODO.

추가 개발 필요:
- `PatrolCommand` 로직 연결: A <-> B 사이 반복 이동.
- 경로상 적 발견 시 공격 후 순찰 복귀.
- 순찰 상태의 애니메이션/HUD 상태 표시.

관련 파일:
- `include/core/command/LogicCommand.hpp`
- `src/game/game/GameLogicManager.cpp`
- `src/game/game/systems/MovementSystem.cpp`

### 1-4. `[필요]` Attack-Move 커맨드 구현

이미 개발된 것:
- `AttackMoveCommand`와 `GameplayInputAction::AttackMove` 골격은 있음.
- 기본 우클릭 공격과 타겟 추격 전투는 구현됨.

추가 개발 필요:
- 이동 중 시야/탐색 범위 안 적을 자동 공격.
- 적 처치 후 원래 이동 목표로 복귀.
- 경로 탐색, 타겟 스캔, 공격 상태 전환 규칙 정리.

관련 파일:
- `include/core/model/IGameElement.hpp`
- `src/core/model/Unit.cpp`
- `src/game/game/systems/MovementSystem.cpp`

---

## Phase 2 - 유닛 다양화 (Unit Variety)

한 종류 유닛만으로는 전략이 부족하다.

### 2-1. `[부분 완료]` 복수 유닛 타입

이미 개발된 것:
- `UnitType`에 `Warrior`, `Archer`, `Worker`가 존재함.
- `UnitFactory`와 프로토타입 로더 골격이 존재함.

추가 개발 필요:
- `UnitType` 네임스페이스와 factory/prototype 코드 정리.
- 타입별 `UnitStaticData` 등록.
- `UnitFactory`가 실제 `core::model::Unit` 생성 경로에 연결되도록 수정.
- 타입별 스프라이트/애니메이션 클립 선택.

관련 파일:
- `include/core/model/UnitType.hpp`
- `include/core/factory/UnitFactory.hpp`
- `include/core/prototype/UnitPrototypeLoader.hpp`

### 2-2. `[필요]` Worker 유닛 기초

이미 개발된 것:
- `UnitType::Worker`와 `GatherCommand` 골격이 있음.
- `ResourceNode::tryGather(...)` 모델은 있음.

추가 개발 필요:
- `Unit::gather(IGameElement*)` 실제 구현.
- Worker가 자원 노드로 이동 -> 채집 -> 마을회관 복귀하는 루프.
- Worker 전용 HUD 명령 활성화.

관련 파일:
- `src/core/model/Unit.cpp`
- `src/core/model/ResourceNode.cpp`
- `src/game/game/GameLogicManager.cpp`

### 2-3. `[부분 완료]` 원거리 공격 지원

이미 개발된 것:
- `Unit`은 `attackRange` 기반으로 사거리 밖 추격, 사거리 안 공격을 수행함.

추가 개발 필요:
- `Archer` 타입에 긴 사거리와 별도 스프라이트/스탯 적용.
- 투사체 또는 원거리 피격 피드백.
- 근접/원거리 공격 타입 분리.

관련 파일:
- `src/core/model/Unit.cpp`
- `src/core/viewmodel/UnitViewModel.cpp`

---

## Phase 3 - 자원 & 경제 (Economy)

자원 표시 기반은 있으나 실제 채집, 소비, 공급 루프가 필요하다.

### 3-1. `[부분 완료]` 자원 노드 엔티티

이미 개발된 것:
- `ResourceNode` 클래스가 `IGameElement`로 구현됨.
- 금/나무 타입, 잔량, `tryGather(...)`가 있음.
- `ResourceNodeViewModel` 골격이 있음.
- `GameLogicManager`가 디버그 자원 노드를 생성함.

추가 개발 필요:
- `GameUIManager::syncWithWorld()`에서 `ResourceNodeViewModel` 생성 연결.
- 맵 로딩 또는 배치 데이터로 자원 노드 배치.
- 자원 노드 선택 HUD 표시.

관련 파일:
- `include/core/model/ResourceNode.hpp`
- `src/core/model/ResourceNode.cpp`
- `src/core/viewmodel/ResourceNodeViewModel.cpp`
- `src/game/game/GameUIManager.cpp`

### 3-2. `[필요]` 채집 루프 완성

이미 개발된 것:
- `PlayerResourceState` 저장/조회와 HUD 표시 파이프라인이 있음.
- `GatherCommand`와 `ResourceNode::tryGather(...)` 골격이 있음.

추가 개발 필요:
- Worker가 자원 클릭 시 `GatherCommand`를 발행하고 처리.
- 채집량 누적, 운반량, 복귀 위치, 입금 조건 구현.
- `GameWorld::setPlayerResources(...)`를 통해 골드/나무 증가.
- `GatherSystem` 또는 Worker 상태 머신 도입.

관련 파일:
- `include/core/model/PlayerResourceState.hpp`
- `src/core/world/GameWorld.cpp`
- `src/game/game/GameLogicManager.cpp`
- `src/game/game/systems/`

### 3-3. `[필요]` 자원 소비 (생산/건설 비용)

이미 개발된 것:
- HUD 자원 표시와 `PlayerResourceState` 저장소가 있음.
- `TrainUnitCommand`, `BuildCommand`, `CancelProductionCommand` 골격이 있음.

추가 개발 필요:
- 유닛/건물 데이터에 `goldCost`, `woodCost`, `foodCost` 추가.
- 훈련/건설 시작 시 자원 차감.
- 잔액 부족 시 명령 거부와 HUD 피드백.
- 식량 사용량/최대치 갱신.

관련 파일:
- `include/core/data/UnitStaticData.hpp`
- `include/core/data/BuildingStaticData.hpp`
- `include/core/model/PlayerResourceState.hpp`

---

## Phase 4 - 건물 (Buildings)

건물 모델은 생겼지만 배치, 렌더 연결, 생산/건설 게임 루프가 더 필요하다.

### 4-1. `[부분 완료]` 건물 엔티티 기초

이미 개발된 것:
- `Building` 클래스가 `IGameElement`로 구현됨.
- `TownHall`, `Barracks`, HP, 팀 ID, 생산 큐 골격이 있음.
- `BuildingViewModel`과 건물 텍스처 ID 경로가 있음.
- 디버그 TownHall/Barracks 생성 코드가 있음.

추가 개발 필요:
- `GameUIManager::syncWithWorld()`에서 `BuildingViewModel` 생성 연결.
- NxM 타일 풋프린트와 건물 크기 기반 충돌.
- `BuildingStaticData` 실용화.

관련 파일:
- `include/core/model/Building.hpp`
- `src/core/model/Building.cpp`
- `src/core/viewmodel/BuildingViewModel.cpp`
- `src/game/game/systems/CollisionSystem.cpp`

### 4-2. `[필요]` 건물 배치 (Build 명령)

이미 개발된 것:
- `BuildCommand`와 HUD Build 버튼/단축키 골격은 있음.

추가 개발 필요:
- Worker `build(int buildingType, Vector2D pos)` 실제 구현.
- HUD Build 버튼 -> 배치 모드 -> 위치 클릭 -> 건물 생성.
- 자원 차감, 빈 타일 검증, 건설 시간 처리.

관련 파일:
- `src/game/game/GameLogicManager.cpp`
- `src/game/game/GameUIManager.cpp`
- `src/platform/sfml/SfmlHudOverlay.cpp`

### 4-3. `[부분 완료]` 건물 선택 & HUD

이미 개발된 것:
- `SelectionSystem`은 모든 `IGameElement`를 선택 대상으로 볼 수 있음.
- `Building`은 `displayName()`, HP, selected 상태를 제공함.
- `BuildingViewModel`은 선택 링과 HP 바 렌더 명령을 만들 수 있음.

추가 개발 필요:
- `GameUIManager`가 건물 viewmodel을 생성하도록 연결.
- 선택 HUD가 유닛뿐 아니라 건물 정보를 표시하도록 확장.
- 건물 선택 시 Train/Cancel 등 건물 전용 버튼 표시.

관련 파일:
- `src/game/game/systems/SelectionSystem.cpp`
- `src/game/game/GameUIManager.cpp`
- `src/platform/sfml/SfmlHudOverlay.cpp`

### 4-4. `[부분 완료]` 유닛 생산 큐

이미 개발된 것:
- `Building`에 생산 큐, 생산 시간, progress, spawn callback 골격이 있음.
- `BuildingViewModel`에 생산 progress bar 렌더 코드가 있음.

추가 개발 필요:
- `TrainUnitCommand` 처리 연결.
- 건물 타입별 생산 가능 유닛 제한.
- 생산 완료 시 실제 `Unit` 스폰과 `GameWorld` 추가.
- HUD 생산 큐 아이콘 및 cancel 처리.

관련 파일:
- `src/core/model/Building.cpp`
- `src/game/game/GameLogicManager.cpp`
- `src/platform/sfml/SfmlHudOverlay.cpp`

---

## Phase 5 - 맵 & 시야 (Map & Vision)

맵/시야 기반 타입은 있으나 실제 게임 렌더링과 규칙에 더 깊게 연결해야 한다.

### 5-1. `[부분 완료]` 맵 로딩

이미 개발된 것:
- `tmxlite`가 CMake에 연결되어 있음.
- `TileMapSoA`, `GameWorldGridQuery`, pathfinding grid 기반이 있음.
- 월드 타일 렌더링용 타일셋 경로가 있음.

추가 개발 필요:
- `.tmx` 파일 로딩.
- 타일 레이어 렌더링 데이터 변환.
- 충돌 레이어를 이동 불가 타일로 반영.
- 자원/건물/시작 위치를 맵 데이터에서 생성.

관련 파일:
- `external/tmxlite/`
- `src/core/world/`
- `include/core/map/TileMapSoA.hpp`
- `src/platform/sfml/SfmlRenderManager.cpp`

### 5-2. `[부분 완료]` 카메라 이동

이미 개발된 것:
- `CameraManager`가 위치/뷰포트/world size와 `screenToWorld`/`worldToScreen`을 제공함.
- 방향키 이동, 화면 가장자리 스크롤, 월드 레이어 SFML view 적용이 있음.

추가 개발 필요:
- 미니맵 클릭 이동.
- 카메라 bounds/속도 설정 데이터화.
- 마우스 휠 줌이 필요하다면 별도 정책 결정.

관련 파일:
- `include/core/manager/CameraManager.hpp`
- `src/core/manager/CameraManager.cpp`
- `src/game/game/GameUIManager.cpp`
- `src/platform/sfml/SfmlRenderManager.cpp`

### 5-3. `[부분 완료]` 전장의 안개 (Fog of War)

이미 개발된 것:
- `FogOfWar` 클래스가 셀 상태(`Unexplored`, `Explored`, `Visible`)와 원형 reveal 기능을 제공함.

추가 개발 필요:
- `GameWorld` 또는 별도 vision system에 Fog 상태 보관.
- 유닛/건물 시야 반경으로 매 틱 reveal.
- 렌더러에서 미탐색/탐색됨/시야 안 상태를 시각화.
- 적 유닛/건물을 시야 안에서만 렌더링.

관련 파일:
- `include/core/map/FogOfWar.hpp`
- `src/core/map/FogOfWar.cpp`
- `src/platform/sfml/SfmlRenderManager.cpp`

---

## Phase 6 - 게임 규칙 & AI

### 6-1. `[필요]` 승패 조건

이미 개발된 것:
- 팀 ID와 `TownHall` 건물 타입은 존재함.

추가 개발 필요:
- 상대 TownHall 파괴 시 승리.
- 내 TownHall 파괴 시 패배.
- 게임 종료 화면과 재시작/메인메뉴 흐름.

### 6-2. `[필요]` 간단한 AI 플레이어

이미 개발된 것:
- Enemy 팀 유닛/건물 디버그 배치가 가능함.

추가 개발 필요:
- Enemy 팀 AI: 초기 유닛으로 플레이어 TownHall 공격.
- Worker AI: 자원 채집 루프.
- 생산 AI와 공격 타이밍 확장.

### 6-3. `[부분 완료]` 씬 전환 완성

이미 개발된 것:
- Login/Lobby/Game 씬 매니저 골격이 있음.

추가 개발 필요:
- Login -> Lobby -> Game -> Game Over 흐름 완성.
- Game Over scene 또는 overlay 구현.
- 재시작/메인메뉴 버튼 처리.

---

## Phase 7 - 폴리싱

- `[필요]` 유닛 피격/사망 애니메이션.
- `[필요]` 공격 효과음, 이동 효과음.
- `[부분 완료]` 선택 링: 유닛 선택 링은 있음. 건물/자원 선택 링은 viewmodel 연결 후 확인 필요.
- `[부분 완료]` 체력바: 건물 viewmodel에는 HP bar 코드가 있음. 유닛 HP bar와 HUD 확장은 필요.
- `[필요]` Repair 명령 실제 구현: Worker가 건물 수리.
- `[필요]` HUD/world 스냅샷 API 정리: 렌더 스레드 lock 아키텍처 개선.

---

## 현재 미완성 Follow-up 요약

| 상태 | 항목 | 출처 | 해당 Phase |
|------|------|------|------------|
| `[부분 완료]` | 커서 상태 변형 검증/확정 및 공격 가능 건물 hover 확장 | Cursor State Variation | Phase 1-2 |
| `[부분 완료]` | 유닛 실제 스탯 데이터화 및 HUD 공격력/방어력/사거리 표시 | Selected Unit HUD Details | Phase 1-1 |
| `[필요]` | Patrol 커맨드 로직 | StarCraft Hotkeys | Phase 1-3 |
| `[필요]` | Attack-Move 커맨드 로직 | LogicCommand set | Phase 1-4 |
| `[필요]` | Worker 채집 루프와 경제 명령 | Player Resource HUD, ResourceNode | Phase 2-2, 3-2 |
| `[필요]` | 자원 소비와 공급 갱신 | Player Resource HUD | Phase 3-3 |
| `[부분 완료]` | 건물 엔티티 렌더/선택/HUD 연결과 풋프린트 충돌 | Building model, Movement Stops | Phase 4-1, 4-3 |
| `[필요]` | Build/Train UI -> LogicCommand 완성 | HUD Gameplay Input | Phase 4-2, 4-4 |
| `[부분 완료]` | tmxlite 기반 실제 `.tmx` 맵 로딩 | tmxlite linkage | Phase 5-1 |
| `[부분 완료]` | FogOfWar를 GameWorld/렌더/시야 규칙에 연결 | FogOfWar core | Phase 5-3 |
| `[필요]` | 승패 조건과 AI 플레이어 | Game rules | Phase 6 |
| `[필요]` | HUD/World 스냅샷 API 정리 | Render Lock Fix | Phase 7 |
