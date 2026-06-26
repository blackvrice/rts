# RTS — C++ 실시간 전략 게임 (Vertical Slice)

> C++23 / SFML 로 직접 구현한 RTS 게임. 데이터 주도 설계, 고정 틱 결정론 시뮬레이션,
> 명령 로그 기반 리플레이, A* 패스파인딩, 전장의 안개, 적 AI까지 하나의 수직 슬라이스로 구현했습니다.

<!-- TODO: 데모 GIF/스크린샷 넣기 (gameplay.gif). 채집→생산→전투→안개 정찰 30초 클립 권장 -->
<!-- ![gameplay](docs/gameplay.gif) -->

---

## ✨ Features

**전투 (Combat)**
- 무기/장갑 상성 테이블 (Normal·Pierce·Siege·Magic × Unarmored·Light·Heavy·Fortified)
- 공격 FSM: 선딜(PreCast) → 발사(FirePoint) → 후딜(Cooldown), 무빙샷 취소 지원
- 원거리 투사체(호밍) + AoE/스플래시(거리별 감쇠), 공중/지상 타게팅 데이터

**경제 / 테크 (Economy & Tech)**
- 자원 채집 루프(채집 → 반납 → 재채집), Cost/canAfford/pay/refund 비용 처리
- 실시간 인구·병력 집계, 자원 부족 시 HUD 경고 표시
- `TechTreeValidator`: 선행 건물/업그레이드 요구 검증 (CanBuild / CanProduce)

**유닛 제어 (Control)**
- 드래그 박스 / Shift 추가 / Ctrl·더블클릭 동일 타입 선택, 컨트롤 그룹(1~9)
- 명령 큐(Shift 예약), Move·Attack·AttackMove·Patrol·Hold·Gather·Build

**UI / 시야 (UI & Fog of War)**
- 컨텍스트 명령 카드(선택 종류별 버튼, 조건/자원 미충족 시 회색 잠금)
- 워커 **빌드 메뉴** / 생산 건물 **생산 리스트** / 다중 선택 **초상화 그리드**(클릭 시 개별 선택)
- 전장의 안개(미탐색/탐색/시야 3단계, 시야 밖 적 은폐)
- **인터랙티브 미니맵**: 지형·유닛·자원 표시, 카메라 뷰포트, 좌클릭 이동 / 우클릭 명령

**맵 / 저장 / 리플레이 (Map · Save · Replay)**
- **Tiled `.tmx`** 맵 임포트(tmxlite) + 자체 JSON 맵 포맷
- 세이브/로드(JSON 스냅샷: 틱·경제·엔티티 상태·생산 큐)
- **WorldHash**(FNV-1a 결정론 해시) + 디버그 오버레이(F3)
- **리플레이**: 플레이어 명령 로그 기록/재생, 30틱마다 해시 체크포인트로 divergence 검출

**AI**
- 빌드 오더 상태머신(Opening → Gather → BuildBarracks → ProduceArmy → Attack), 워커 자동 채집, 병력 집결·공격 타이밍·방어 반응

---

## 🧱 Tech Stack

| 분류 | 사용 기술 |
|------|-----------|
| 언어 | **C++23** |
| 렌더/윈도우 | **SFML 3** + OpenGL |
| UI 오버레이 | **Dear ImGui** |
| 데이터 | **nlohmann/json** (units·buildings·resources·animations·maps) |
| 맵 | **tmxlite** (Tiled `.tmx`) |
| 빌드 | **CMake** (Ninja / CLion) |

핵심 설계: 고정 틱(30Hz) 결정론 시뮬레이션 · Logic/Render 스레드 분리 · Command Bus + Router ·
EntityId 핸들(index+generation) · 데이터 주도(콘텐츠 무재컴파일 교체) · DI 컨테이너.

---

## 🏗 Architecture

```
                 ┌────────────────────────────┐
   입력(SFML) ─▶ │  UICommandBus → UIManager   │  (화면 좌표, ImGui HUD)
                 └─────────────┬──────────────┘
                               │ LogicCommand (Move/Attack/Build/Train…)
                               ▼
   ┌──────────────────── LogicThread (고정 dt 30Hz) ─────────────────────┐
   │  CommandRouter.dispatch → GameLogicManager                          │
   │     ├ MovementSystem (A* PathManager, 충돌, 진형/회피)              │
   │     ├ CollisionSystem (유닛=원 / 건물·자원=사각 footprint)          │
   │     ├ Combat (상성·FSM·투사체·스플래시)                             │
   │     ├ Economy/Tech (Cost, TechTreeValidator)                        │
   │     ├ Fog of War · WorldRuntimeServices(이펙트/사운드/공간 인덱스)  │
   │     └ AI (빌드오더 상태머신)                                        │
   │  GameWorld (EntityManager, 점유 그리드, fog, worldHash)             │
   └────────────────────────────┬───────────────────────────────────────┘
                                 │ ViewModel 동기화
                                 ▼
                 ┌────────────────────────────┐
   화면 출력 ◀── │ RenderQueue → SfmlRenderMgr │  (월드 카메라 / UI 분리)
                 └────────────────────────────┘
```

- **입력과 시뮬레이션 분리**: 모든 플레이어 행동은 `LogicCommand`로 추상화되어 한 곳(Router)에서 처리 → 리플레이/세이브의 기반.
- **로직과 렌더 분리**: 로직은 `RenderQueue`(DrawSprite/Rect/Circle/Text + HUD 갱신)만 채우고, 렌더러가 소비. 모델은 렌더를 모름.
- **데이터 주도**: `data/*.json`, `data/maps/*.tmx` 만 바꾸면 유닛 스탯·초상화·맵·애니메이션 변경(재컴파일 불필요).

---

## 🔬 Technical Highlights

엔지니어링 관점에서 특히 공들인 부분입니다.

1. **결정론 시뮬레이션 + 리플레이 + WorldHash**
   고정 dt·RNG 미사용·고정 반복 순서로 결정론을 확보하고, 플레이어 명령을 틱과 함께 로그로 남겨
   `명령 로그만으로 경기를 재생`합니다. 매 N틱 `WorldHash`(엔티티 id/위치/HP/상태 + 경제 해시)를 비교해
   재생 중 divergence를 즉시 검출 — 디버깅/네트워킹 동기화의 토대.
   → [`core/replay/ReplayLog`](src/core/replay/ReplayLog.cpp), [`GameWorld::worldHash`](src/core/world/GameWorld.cpp)

2. **데이터 주도 콘텐츠 파이프라인**
   유닛/건물/자원/애니메이션/초상화/맵을 코드에서 분리. 팩토리 기본값을 시드한 뒤 JSON으로 오버레이하고,
   Tiled `.tmx`(오브젝트 레이어 = 스폰, 충돌 타일 레이어, 맵 프로퍼티 = 경제)를 그대로 임포트.
   → [`core/data/DataRegistry`](src/core/data/DataRegistry.cpp), [`core/map/MapLoader`](src/core/map/MapLoader.cpp)

3. **패스파인딩 + 사각 footprint 충돌의 일관성**
   A*(캐시·동적 점유)로 경로를 찾고, 건물/자원은 **타일 footprint 사각형**으로 점유·충돌을 통일.
   유닛은 원형, 구조물은 원-AABB 판정 + 점유 그리드 O(1) 조회. 건물에 박힌 유닛은 최소 침투 축으로 탈출.
   → [`MovementSystem`](src/game/game/systems/MovementSystem.cpp), [`CollisionSystem`](src/game/game/systems/CollisionSystem.cpp)

---

## 🚀 Build & Run

```bash
cmake -S . -B build
cmake --build build --target RTS
```

- 요구: C++23 컴파일러, SFML 3, CMake. (의존성은 `external/`에 vendoring: imgui, tmxlite, json)
- 실행: 빌드 후 산출된 `RTS` 실행. 게임 데이터는 `data/`, 아트는 Tiny Swords 에셋을 사용.

**조작 / 디버그 키**

| 키 | 동작 |
|----|------|
| 좌클릭 드래그 | 선택 / A | 어택무브 / 우클릭 | 이동·공격·채집 |
| 1~9 (+Ctrl/Shift) | 컨트롤 그룹 | F3 | 틱·WorldHash 오버레이 |
| F5 / F9 | 퀵세이브 / 로드 | F6 / F7 | 리플레이 기록 토글 / 재생 |

---

## 📑 개발 기록

- [`DEVELOPMENT_PLAN.md`](DEVELOPMENT_PLAN.md) — Phase/Epic 단위 로드맵과 완료 기준
- [`DEVELOPMENT_LOG.md`](DEVELOPMENT_LOG.md) — 에픽별 구현 내역·결정·검증 기록

> 본 프로젝트는 상용 출시가 아닌 **학습/포트폴리오용 수직 슬라이스**입니다.
> 아트는 [Tiny Swords](https://pixelfrog-assets.itch.io/tiny-swords) (Pixel Frog) 에셋을 사용했습니다.
