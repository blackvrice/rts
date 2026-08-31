# RTS

C++23과 SFML로 고정 틱 시뮬레이션, 명령 리플레이, A* 이동을 구성한 실시간 전략 게임 Vertical Slice입니다.

**Gameplay GIF:** To be added

**Gameplay Video:** To be added

| 개발 | Engine / Framework | Language | 핵심 | 검증 |
|---|---|---|---|---|
| 개인 프로젝트 / 1인 개발 | SFML 3 · Dear ImGui · CMake | C++23 | Fixed Tick · Command/Replay · WorldHash · A* | CTest Headless Smoke · Manual QA Checklist |

## Overview

일꾼으로 자원을 모으고 건물을 지어 병력을 생산한 뒤, 유닛을 지휘해 적 기지를 파괴하는 RTS입니다. 렌더링 기능보다 게임 내부 시스템 설계에 초점을 두고 입력을 Command로 변환해 30Hz Logic Tick에서 처리하며, 결과를 렌더 스레드에 전달하는 구조로 구현했습니다.

## Core Gameplay

`Gather → Build → Produce → Control Units → Combat → Victory / Defeat`

선택·컨트롤 그룹·명령 큐, 채집·건설·생산, 근접/원거리 전투, 적 AI, Fog of War와 미니맵이 하나의 매치 흐름으로 연결됩니다. JSON 또는 Tiled `.tmx` 맵에서 유닛·건물·자원 배치를 읽어 같은 시스템으로 실행합니다.

## Technical Highlights

1. **Fixed-Tick Command Simulation** — 화면 입력을 `LogicCommand`로 변환하고 단일 Router를 거쳐 30Hz Logic Tick에 적용합니다. 렌더 프레임 시간과 게임 규칙의 시간 기준을 분리했습니다.
2. **Command Replay + WorldHash** — 명령을 Tick과 함께 기록·재생하고, 30 Tick마다 렌더 상태를 제외한 WorldHash를 비교해 divergence를 찾습니다. Save/Load 뒤에도 해시를 비교할 수 있습니다.
3. **A* Pathfinding / Unit Movement** — 정적 footprint와 동적 유닛 점유를 분리하고, A* 요청을 여러 Tick과 worker thread에 분산한 뒤 결과는 고정 순서로 적용합니다.
4. **Entity / Data-driven Architecture** — `EntityId(index + generation)`로 재사용 슬롯의 stale handle을 무효화하고, 유닛·건물·자원·애니메이션·맵 데이터는 JSON/TMX로 분리했습니다.

## Architecture

```text
Input / UI
    │
    ▼
LogicCommand Bus → Router
    │
    ▼
Logic Thread (Fixed 30Hz)
    ├─ Movement / A* / Collision
    ├─ Combat / Economy / Tech / AI
    └─ GameWorld / Entity / Fog / WorldHash
    │
    ▼
ViewModel / RenderQueue → SFML Render Thread
```

게임 모델은 SFML draw call을 직접 호출하지 않습니다. Logic은 World State를 변경하고 `RenderQueue`를 만들며, 렌더 계층이 이를 소비합니다. 상세 클래스와 데이터 흐름은 [Architecture](docs/ARCHITECTURE.md)에서 확인할 수 있습니다.

## Problem Solving

### 대형 TMX 맵의 A* Tick Stall

**Problem** 256×256 포트폴리오 맵에서 첫 적 웨이브가 길을 찾을 때 Logic Thread가 수 초 동안 World write lock을 점유했습니다.

**Cause** A*의 동적 장애물 질의마다 모든 Entity를 선형 탐색했고, 한 Tick에 여러 cross-map 경로를 함께 계산했습니다.

**Solution** 유닛 점유 grid를 캐시해 질의를 O(1)로 바꾸고, 요청 수를 Tick에 분산했습니다. 이후 독립 A* 계산은 thread pool에서 수행하되 적용 순서는 고정했습니다.

**Verification** `rts_headless_smoke`에 포트폴리오 맵 90 Tick 실행을 추가했습니다. 기록상 최대 Tick은 수정 전 약 3.1초에서 수정 후 30ms 미만으로 줄었고, 현재 smoke도 통과합니다.

### Fog of War 밖 정보 노출

**Problem** 시야 밖 전투의 소리와 월드 이펙트가 재생되면 보이지 않는 적의 위치를 추측할 수 있었습니다.

**Cause** Entity 렌더는 Fog 상태를 확인했지만 전투 feedback 경로는 같은 visibility 규칙을 사용하지 않았습니다.

**Solution** 위치가 있는 공격·피격·사망·폭발 feedback만 `FogOfWar::Visible`일 때 출력하고, 선택·생산·자원·승패 같은 UI cue는 유지했습니다.

**Verification** RTS와 headless target을 다시 빌드하고 전체 smoke를 통과시켰습니다. 시각·청각 결과는 [QA Checklist](docs/QA_CHECKLIST.md)의 수동 항목으로 분리했습니다.

## Testing / Verification

2026-08-31 현재 `master`에서 CTest `rts_headless_smoke` **1/1 통과**와 `RTS.exe` 5초 실행 유지를 확인했습니다. 이 테스트는 다음 범위를 다룹니다.

- Runtime JSON과 JSON/TMX 맵 로딩
- Tech Tree 선행 조건과 데이터 ID 해석
- Replay 명령 직렬화, metadata와 hash checkpoint 저장/복원
- Fixed-point 이동 kernel
- 포트폴리오 맵 60 Tick 실행, 카메라·맵 경계·구조물 clearance

```powershell
& 'C:\Program Files\JetBrains\CLion 2026.1.2\bin\cmake\win\x64\bin\cmake.exe' --build cmake-build-debug --target RTS rts_headless_smoke -- -j 4
& 'C:\Program Files\JetBrains\CLion 2026.1.2\bin\cmake\win\x64\bin\cmake.exe' --build cmake-build-debug --target test
```

Headless Smoke는 실제 화면 조작을 대신하지 않습니다. 선택, 채집, 건설, 전투, Fog, Save/Load, Replay는 [Manual QA Checklist](docs/QA_CHECKLIST.md)로 별도 확인합니다.

## AI-assisted Development

생성형 AI는 코드 탐색, 구현 초안, 반복 코드와 테스트 작성 보조에 활용했습니다. 요구사항, 시스템 경계, 게임 규칙과 검증 기준은 개발자가 정의하고 생성 코드를 직접 검토·수정했습니다. 최종 판단은 빌드, headless test, 짧은 실행과 수동 게임플레이 결과를 기준으로 했습니다.

## Technical Documentation

- [Architecture](docs/ARCHITECTURE.md) — Thread, Command, World, Replay, Data 흐름
- [Source Structure](SOURCE_STRUCTURE.md) — 폴더와 주요 entry point
- [Development Plan](DEVELOPMENT_PLAN.md) — 구현 범위와 남은 작업
- [Development Log](DEVELOPMENT_LOG.md) — 문제 원인, 변경, 검증 기록
- [QA Checklist](docs/QA_CHECKLIST.md) — 실제 플레이 수동 검증 항목

## Build & Run

현재 CMake 설정은 Windows에서 SFML 3.0.2와 Dear ImGui의 로컬 경로를 사용합니다. `tmxlite`와 `nlohmann/json`은 저장소에 포함되어 있습니다.

```powershell
& 'C:\Program Files\JetBrains\CLion 2026.1.2\bin\cmake\win\x64\bin\cmake.exe' -S . -B cmake-build-debug -G Ninja
& 'C:\Program Files\JetBrains\CLion 2026.1.2\bin\cmake\win\x64\bin\cmake.exe' --build cmake-build-debug --target RTS -- -j 4
.\cmake-build-debug\RTS.exe
```

| 입력 | 동작 |
|---|---|
| 좌클릭 / Drag / Shift | 선택 / 다중 선택 |
| 우클릭 / A | 상황 명령 / Attack Move |
| 1–9 + Ctrl/Shift | Control Group |
| F3 | Tick / WorldHash Overlay |
| F5 / F9 | Quick Save / Load |
| F6 / F7 | Replay Record / Play |

## Current Scope

- 동일 실행 환경의 고정 Tick·안정적 순서와 WorldHash 검출은 구현했지만, float 기반 충돌·사거리·투사체가 남아 있어 **cross-platform bit-level determinism 완료를 주장하지 않습니다**.
- Multiplayer networking과 실제 Upgrade 콘텐츠는 제출 기능에 포함하지 않습니다.
- Gameplay GIF와 Video, 전체 Manual QA 재실행은 아직 필요합니다.
- 아트는 [Tiny Swords](https://pixelfrog-assets.itch.io/tiny-swords) (Pixel Frog) 에셋을 사용했습니다.
