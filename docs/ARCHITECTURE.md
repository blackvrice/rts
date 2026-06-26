# Architecture

이 문서는 RTS 프로젝트의 구조와 설계 의도를 설명합니다. 코드를 읽기 전에 "큰 그림"을
잡는 용도이며, 각 절은 실제 디렉터리/파일과 연결됩니다.

- 상위 소개: [`README.md`](../README.md)
- 진행 기록: [`DEVELOPMENT_PLAN.md`](../DEVELOPMENT_PLAN.md) · [`DEVELOPMENT_LOG.md`](../DEVELOPMENT_LOG.md)

---

## 1. 설계 목표

| 목표 | 구현 방식 |
|------|-----------|
| **결정론(Determinism)** | 고정 dt(30Hz) 시뮬레이션, RNG 미사용, 안정적 반복 순서 → 리플레이/해시 검증 가능 |
| **입력과 시뮬레이션 분리** | 모든 플레이어 행동을 `LogicCommand`로 추상화, 단일 Router에서 처리 |
| **로직과 렌더 분리** | 로직은 `RenderQueue`만 채우고 렌더러가 소비, 모델은 렌더를 모름 |
| **데이터 주도(Data-driven)** | 유닛/건물/자원/애니메이션/맵을 JSON·`.tmx`로 정의, 콘텐츠 변경 시 무재컴파일 |
| **레이어 경계** | `core`(엔진/도메인) → `game`(규칙) → `platform`(SFML/ImGui) 단방향 의존 |

---

## 2. 큰 그림 (런타임 데이터 흐름)

```
                        ┌──────────────────────────────────────────────┐
   OS 입력 (SFML) ─────▶│ SfmlWindow : 이벤트 → UICommand               │
                        │   (ImGui가 마우스를 점유하면 게이팅)          │
                        └───────────────┬──────────────────────────────┘
                                        │ UICommandBus
                                        ▼
                        ┌──────────────────────────────────────────────┐
                        │ GameUIManager (UICommandRouter)               │
                        │   - 화면 입력 해석, 카메라, 선택, 빌드/생산   │
                        │   - 결과를 LogicCommand 로 발행               │
                        └───────────────┬──────────────────────────────┘
                                        │ LogicCommandBus
   ════════════════════════════════════╪═══════════════ 스레드 경계 ═══
                                        ▼
   ┌──────────────────── LogicThread (고정 dt 30Hz) ───────────────────┐
   │ 1) bus 드레인 → LogicCommandRouter.dispatch                       │
   │ 2) GameLogicManager.tick(dt):                                     │
   │      MovementSystem  (PathManager A*, 진형, 지역 회피)            │
   │      CollisionSystem (유닛=원 / 구조물=사각 footprint)            │
   │      Combat          (상성·공격 FSM·투사체·스플래시)             │
   │      Economy/Tech     (Cost, TechTreeValidator, 인구 집계)        │
   │      FogOfWar / WorldRuntimeServices (이펙트·사운드·공간 인덱스)  │
   │      AI              (빌드오더 상태머신)                          │
   │    GameWorld: EntityManager, 점유 그리드, fog, worldHash          │
   └───────────────────────────────┬──────────────────────────────────┘
                                    │ syncWithWorld (모델 → ViewModel)
                                    ▼
                        ┌──────────────────────────────────────────────┐
                        │ ViewModel[] → RenderQueue (DrawSprite/Rect/   │
                        │   Circle/Text + HUD 갱신 커맨드)              │
                        └───────────────┬──────────────────────────────┘
                                        ▼
                        ┌──────────────────────────────────────────────┐
                        │ SfmlRenderManager : World(카메라) / UI 분리   │
                        │   + SfmlHudOverlay (Dear ImGui)               │
                        └──────────────────────────────────────────────┘
```

핵심: **왼쪽(입력/UI)은 자유 프레임, 가운데(시뮬레이션)는 고정 틱, 오른쪽(렌더)은 큐 소비.**
세 영역이 커맨드 버스와 렌더 큐로만 연결되어 서로의 내부를 모릅니다.

---

## 3. 레이어 & 디렉터리 맵

```
include/ , src/
├─ core/                  엔진·도메인 (플랫폼 비의존)
│  ├─ command/            Command, LogicCommand/UICommand, Bus, RouterBase
│  ├─ model/              IGameElement, Unit, Building, ResourceNode, Projectile …
│  ├─ world/              GameWorld, GameWorldGridQuery, WorldRuntimeServices
│  ├─ ecs/                EntityId(index+generation), EntityManager
│  ├─ sim/                SimClock(고정 dt), Fixed(고정소수 유틸)
│  ├─ data/               DataRegistry, *StaticData, TechTree, DataPaths
│  ├─ map/                MapData, MapLoader(JSON/.tmx), FogOfWar
│  ├─ tech/               TechTreeValidator
│  ├─ replay/             ReplayLog (명령 직렬화 + 해시 체크포인트)
│  ├─ path/               IGridQuery, GridTypes  (+ manager/PathManager : A*)
│  ├─ render/             RenderCommand(variant), RenderQueue, IRenderManager
│  ├─ viewmodel/          Unit/Building/ResourceNode/ProjectileViewModel
│  ├─ manager/            CameraManager, SceneManager, ILogicManager …
│  ├─ thread/             LogicThread (고정 틱 루프)
│  ├─ di/                 DIContainer (스코프/싱글톤 등록·resolve)
│  └─ ui/                 SelectBox, TextBox …
├─ game/
│  ├─ game/               GameScene, GameLogicManager, GameUIManager
│  │  └─ systems/         Movement, Collision, Selection, ControlGroup
│  ├─ login/ , lobby/     씬
└─ platform/
   └─ sfml/               SfmlWindow, SfmlRenderManager, SfmlHudOverlay
data/                     units.json, buildings.json, resources.json, animations.json
data/maps/                skirmish.json, tiled_skirmish.tmx
external/                 imgui, tmxlite, json (vendoring)
```

의존 방향: `platform → game → core`. `core`는 SFML/ImGui를 모릅니다(렌더는 추상 `RenderQueue`로만 표현).

---

## 4. 커맨드 흐름 (입력 추상화)

- **두 종류의 커맨드**: `UICommand`(마우스/키/HUD 의도)와 `LogicCommand`(Move/Attack/Build/Train/Select…).
- `CommandRouterBase<T>`는 타입별 핸들러를 등록(`on<T>`)하고 `dispatch(cmd)`로 분배합니다.
- 흐름: **SFML 이벤트 → UICommand → GameUIManager(해석) → LogicCommand → LogicThread에서 dispatch → 시스템 실행.**
- 이렇게 "플레이어 의도 = 직렬화 가능한 커맨드"로 모으면 **세이브/리플레이/(미래의)네트워킹**이 한 지점에서 가능해집니다.

> 참고: `core/command/CommandRouterBase.hpp`, `LogicCommand.hpp`, `UICommand.hpp`

---

## 5. 시뮬레이션 코어

### 5.1 고정 틱 루프 — `core/thread/LogicThread.cpp`
```
loop:
  while bus.tryPop(cmd): router.dispatch(*cmd)   // 이번 틱에 적용할 명령
  logic->tick(dt)                                 // dt = 1/30 고정
  sleep_until(nextTick)                            // 드리프트 보정
```
명령은 **틱 직전에** 적용되고, `tick()` 시작에서 `advanceTick()`로 카운터를 올립니다. 이 순서가 리플레이 재현의 핵심입니다.

### 5.2 GameWorld — `core/world/GameWorld.cpp`
시뮬레이션 상태의 단일 소유자:
- `EntityManager` + `EntityId(index+generation)` 핸들 → 슬롯 재사용 시 stale 핸들 무효화
- 구조물 **점유 그리드**(footprint 캐시, 충돌 변경 시 재구성) → `isCellOccupied` O(1)
- `FogOfWar`(로컬 플레이어 시야), 플레이어 자원, 투사체 풀
- `worldHash()` : 결정론 상태 다이제스트(아래 8절)

### 5.3 모델 — `core/model/`
`IGameElement`(tick/상태/팀/액션) 인터페이스 아래 `Unit`·`Building`·`ResourceNode`·`Projectile`.
유닛은 이동/공격/채집/건설 상태머신을 자체 보유(`Unit::tick`이 액션별로 분기).

---

## 6. 이동 · 패스파인딩 · 충돌

- **PathManager (A*)** — `IGridQuery`(정적 벽 / 동적 점유 / 이동 비용) 위에서 경로 탐색, `collisionVersion`으로 캐시 무효화.
- **MovementSystem** — 경로 추종, 진형(formation) 목표, 지역 회피(local avoidance), 도착/우회 처리. 이동은 충돌 수용 전까지 *speculative*.
- **CollisionSystem** — 형태 이원화:
  - **유닛 = 원**(collisionRadius) → 유닛끼리 분리(separation)
  - **건물·자원 = 사각 footprint**(원-AABB 판정) → 패스파인딩 점유와 동일 형태로 일관
  - 구조물에 **중심이 박힌** 유닛은 최소 침투 축으로 밀어 탈출. 채집·건설 워커는 가장자리 접근이 허용됨.

> `game/game/systems/MovementSystem.cpp`, `CollisionSystem.cpp`, `core/manager/PathManager.hpp`

---

## 7. 규칙 시스템 (Combat · Economy · Tech · AI)

- **Combat** — `damageMultiplier(weapon, armor)` 상성 + 공격 FSM(PreCast→FirePoint→Cooldown) + 원거리 투사체(호밍)와 스플래시 콜백.
- **Economy** — `PlayerResourceState`(gold/wood/food) + `Cost`/canAfford/pay/refund. 인구·병력은 매 틱 생존 엔티티에서 재집계.
- **Tech** — `TechTreeValidator`가 선행 건물/업그레이드 요구를 검증(`canBuild`/`canProduce`). 데이터의 `requirements`로 주도.
- **AI** — 빌드오더 상태머신(Opening→Gather→BuildBarracks→ProduceArmy→Attack→Rebuild) + 워커 채집/병력 집결/공격·방어 판단.
- **WorldRuntimeServices** — 시뮬레이션이 만들어내는 *과도기적* 산출물(이펙트, 사운드 큐, 공간 인덱스)을 모아둠. 렌더/오디오가 읽어 소비.

---

## 8. 렌더링

- **모델은 그리지 않는다.** `ViewModel`이 모델을 관찰해 `RenderQueue`에 `RenderCommand`(DrawSprite/DrawRect/DrawCircle/DrawText + HUD 갱신)를 쌓습니다.
- `SfmlRenderManager`가 큐를 두 번 순회: **World 레이어**(카메라 뷰, z-정렬)와 **UI 레이어**(스크린). 스프라이트시트 프레임 애니메이션·좌우 반전(flipX) 지원.
- **HUD**는 `SfmlHudOverlay`(Dear ImGui): 자원 패널, 컨텍스트 명령 카드, 워커 빌드 메뉴/생산 리스트, 다중 선택 초상화 그리드, 인터랙티브 미니맵. 데이터는 `UpdateHudSelection`/`UpdateMinimap` 커맨드로 전달.
- **전장의 안개**: 시야 밖 적은 ViewModel 단계에서 제외하고, 미탐색/탐색 타일은 World 레이어 위에 shroud 사각형으로 덮음.

> `core/render/RenderCommand.hpp`, `platform/sfml/SfmlRenderManager.cpp`, `SfmlHudOverlay.cpp`

---

## 9. 결정론 · 저장 · 리플레이

```
플레이어 명령 ─(틱 스탬프)─▶ ReplayLog
                                  │ save/load (JSON)
세이브: GameWorld 스냅샷(틱·경제·엔티티·생산큐) ─▶ JSON
리플레이: 초기 상태로 리셋 → 매 틱 기록 명령 재투입 → worldHash 비교
```
- **WorldHash** — FNV-1a 64bit. tick + 팀 경제 + (EntityId 정렬) 엔티티의 타입/정수화 위치/HP/액션/팀을 해시. 렌더/UI 제외. **같은 상태 = 같은 해시.**
- **Replay** — `acceptPlayerCommand` 게이트가 기록(Record)·라이브 입력 차단(Play)을 담당. 재생 시 30틱마다 해시를 비교해 divergence를 로깅.
- 결정론 전제: 고정 dt, RNG 없음(AI는 타이머 기반), 부동소수 정수화(해시), 안정적 엔티티 순회.

> `core/replay/ReplayLog.cpp`, `GameWorld::worldHash` / `saveGame` / `loadGame`

---

## 10. 데이터 주도 파이프라인

- `DataRegistry`가 부팅 시 **팩토리 기본값을 시드**한 뒤 `data/*.json`으로 **오버레이**(부분 정의 허용, 누락 시 기본값 유지).
- 맵: `MapLoader`가 확장자로 분기 — 자체 JSON 또는 **Tiled `.tmx`**(tmxlite). `.tmx`의 오브젝트 레이어 = 스폰(class=building/unit/resource, `kind`/`team` 프로퍼티), "collision" 타일 레이어 = 막힘 타일, 맵 프로퍼티 = 시작 자원.
- 효과: 스탯·초상화·맵·애니메이션을 **재컴파일 없이** 교체. (id 규칙 예: `town_hall`, `barracks`, `worker`, `gold` …)

---

## 11. 새 콘텐츠 추가 (확장 가이드)

| 추가 대상 | 방법 |
|-----------|------|
| 유닛 스탯/초상화 | `data/units.json` 항목 수정 (무재컴파일) |
| 새 유닛/건물 **타입** | `UnitType`/`BuildingType` enum + 팩토리 기본값 + JSON id 매핑 |
| 맵 | Tiled로 `.tmx` 제작(오브젝트=스폰) → 시나리오 경로 지정 |
| 새 명령 | `LogicCommand` 파생 + `GameLogicManager`에서 `router.on<T>` 등록 |
| 새 이펙트 | `EffectType` + `emitEffect` 호출 + HUD의 스프라이트 매핑 |

---

## 12. 알려진 한계 / 향후

- 멀티플레이어 네트워킹은 미구현이나, **커맨드 추상화 + 결정론 + WorldHash**가 lockstep 동기화의 토대.
- 리플레이 **재생 속도 조절**은 LogicThread 고정 틱 특성상 후속 과제.
- 업그레이드/연구 콘텐츠는 미존재(검증기·요구 구조는 준비됨).
- 충돌 경계 일부 상수(맵 한계 등)는 대형 맵을 위해 동적화 여지가 있음.
