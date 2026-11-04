# Source Structure

This document is a quick orientation map for future AI agents working in this repository. Keep it current when source ownership, folders, entry points, or build/runtime flow changes.

## Build And Entry Points

- `CMakeLists.txt` defines the `RTS` executable target and, with `BUILD_TESTING`, the `rts_sim` static library (every SFML-free simulation source, listed in `RTS_SIM_SOURCES`) plus the `rts_headless_smoke` and `rts_sim_scenarios` CTest executables that link it.
- `RTS_HEADLESS_ONLY` (CMake option, default OFF) skips SFML, OpenGL, ImGui and the `RTS` target so the headless tests configure and run on any machine, including Linux (`cmake -S . -B build-headless -G Ninja -DRTS_HEADLESS_ONLY=ON`). It needs the `external/tmxlite` submodule checked out.
- Core and game code (`include/core`, `include/game`, `src/core`, `src/game`) must stay SFML-free: only `src/platform/sfml` and `src/core/thread/LogicThread.cpp` include SFML. `core/command/UICommand.hpp` uses engine-neutral key/event types for this reason.
- `src/main.cpp` is the application entry point.
- The project currently targets C++23 in CMake and links SFML 3, SFML Audio, OpenGL, tmxlite, and ImGui.
- `include/platform/sfml/SfmlAssetPaths.hpp.in` is configured into the build directory so SFML platform code can locate packaged assets.
- Typical build commands are:

```powershell
& "C:\Program Files\JetBrains\CLion 2026.1.2\bin\cmake\win\x64\bin\cmake.exe" -S . -B cmake-build-debug
& "C:\Program Files\JetBrains\CLion 2026.1.2\bin\cmake\win\x64\bin\cmake.exe" --build cmake-build-debug --target RTS rts_headless_smoke rts_sim_scenarios -- -j 4
& "C:\Program Files\JetBrains\CLion 2026.1.2\bin\cmake\win\x64\bin\ctest.exe" --test-dir cmake-build-debug --output-on-failure
```

## Top-Level Layout

- `include/` contains project headers. Most implementation files under `src/` mirror these namespaces and folders.
- `src/` contains application, core, game, and platform implementations.
- `external/tmxlite/` is the tmxlite dependency used for map loading. Treat it as third-party code unless the task explicitly targets it.
- `external/json/` vendors the nlohmann/json single header (`nlohmann/json.hpp`) used by the DataRegistry. Third-party code.
- `data/` holds runtime JSON design data (`units.json`, `buildings.json`, `resources.json`, `animations.json`) loaded by `DataRegistry`, plus `data/maps/*.json` scenario maps loaded by `core/map/MapLoader`; edit these to retune stats, change sprites/animations, or lay out the starting map without recompiling.
- `tests/` contains headless test executables, all wired through CTest:
  - `tests/rts_headless_smoke.cpp` checks graphics-free runtime contracts such as data loading, map loading, tech-tree prerequisites, replay log round-trip, fixed math, and the default `portfolio.tmx` logic tick budget (passive-AI budget plus an aggressive-AI freeze guard).
  - `tests/sim/SimHarness.hpp` builds a `GameWorld` + `GameLogicManager` from a scenario map, sends `LogicCommand`s straight into the router, and ticks the simulation. Never call its `send`/`tick` while holding a world lock (handlers take the write lock).
  - `tests/rts_sim_scenarios.cpp` runs gameplay scenarios through `SimHarness`: gathering, training, construction, attack-move combat to victory, same-input WorldHash determinism, quick-save/load WorldHash round-trip, F6/F7 replay playback vs recording, plus report-only checks for known bugs (melee vs building reach, save/load continuation).
- `core/app/MatchConfig::saveDirectory` redirects the F5/F9 quick-save and F6/F7 replay slot (default `data/saves`, gitignored); tests point it at a temp folder.
  - `tests/data/maps/*.json` are small test-only scenario maps (not shown in the game).
- `scripts/` contains offline helper scripts. `scripts/gen_portfolio_map.py` authors walls in tile space and emits the showcase scenario as both `data/maps/portfolio.json` and `data/maps/portfolio.tmx` (the latter with a CSV "collision" tile layer + point-object group), validating entity/footprint placement against the blocked tiles before writing.
- `GAME_COMPLETION_ROADMAP.md` (root) is the current development plan toward a complete single-player RTS v1.0 (milestones, epics, tickets, sprint order). `DEVELOPMENT_PLAN.md` is the earlier Phase 0-8 implementation history.
- `assets/LICENSES.md` lists third-party asset sources and their license status; add a row for every new asset.
- `docs/` contains human-facing project notes such as architecture orientation and the manual QA checklist.
- `Tiny Swords (Free Pack)/` contains art assets used by the game.
- `cmake-build-debug/` is a local build output folder and should not be treated as source.

## Core Layer

- `include/core/app` and `src/core/di` hold application startup and dependency wiring, including `GameApp`. `core/app/MatchConfig.hpp` carries per-match rules (map path, `enemyAiPassive` debug switch, replay recording); `SessionContext::matchConfig()` holds the value for the next Game scene (env overrides `RTS_PASSIVE_AI=1`, `RTS_MAP=<path>`), and `GameApp` passes it to `GameLogicManager`.
- `include/core/command` defines command types and command buses used between UI and logic. Each domain follows a two-part pattern: a `*CommandBus` (thread-safe queue for cross-thread hand-off) plus a `*CommandRouter` (type-indexed dispatcher from `CommandRouterBase`). Logic, UI, and Audio each have their own bus/router pair.
- `include/core/thread` and `src/core/thread` contain off-thread workers: `LogicThread` drives the fixed-step simulation and `AudioThread` drains `AudioCommandBus` to drive an `IAudioManager` on its own ~60Hz poll. Both swap their managed manager via a `Change*ManagerCommand`.
- `include/core/manager` and `src/core/manager` contain cross-scene managers such as scene, camera, and path management.
- `include/core/scene` defines scene interfaces.
- `include/core/map` + `src/core/map`: terrain (`TileMapSoA`), fog of war, and `MapData`/`MapLoader` — a JSON scenario (`data/maps/*.json`) defining grid size, starting resources, and building/unit/resource placement. `GameLogicManager::setupInitialWorld` loads it (falling back to a built-in default) instead of hard-coding the starting layout; type ids resolve through `DataRegistry`.
- `include/core/sim` holds the simulation timing/determinism foundation: `SimClock.hpp` (fixed logic tick rate constants + `TickCount`) and `Fixed.hpp` (16.16 fixed-point `Fixed`/`FixedVec2`, int64-safe `length()`, the `stepToward` movement integrator, and grid/world conversions, compile-time tested via `src/core/sim/Fixed.cpp`). The logic thread steps at `SimClock`'s fixed delta; `GameWorld::currentTick()` is the monotonic tick index. The live path-following mover (`Unit::updateMove(dt, GridTransform)`) integrates via `stepToward`; full bit-determinism is pending the position-storage, collision, range, and projectile migrations (Epic 1.4.3).
- `include/core/ecs` contains entity primitives: the legacy `Registry`/`Entity` (uint32 component registry) and the `EntityId` (index+generation handle) + `EntityManager` (slot allocator with generation-based recycling). `GameWorld` owns an `EntityManager`, assigns an `EntityId` to each game element in `addElement`, and exposes `isAlive`/`resolve`/`pruneDeadEntities`. Units reference their targets (attack, gather resource, drop-off, build site) by `EntityId` resolved through an injected resolver, so a dead/recycled target stops resolving instead of dangling. `AttackCommand` also carries the clicked target's `EntityId`, which the logic prefers (when live/valid/near the click) over positional resolution.
- `include/core/entity`, `include/core/model`, and `include/core/data` define gameplay data, entities, and model types.
- `include/core/data` + `src/core/data/DataRegistry.cpp`: `DataRegistry` is the central authority for static unit/building/resource data and sprite/animation clips. It seeds built-in defaults, then `loadFromDirectory()` overlays JSON from `data/`. `unitStaticDataFor`/`buildingStaticDataFor`/`resourceStaticDataFor` delegate to the global registry; the `default*StaticDataFor` helpers in the `*StaticData.hpp` headers are the built-in fallbacks. `DataPaths.hpp` (generated from `.hpp.in`) provides the configured `data/` path.
- Sprite/animation data: `SpriteData.hpp` defines `SpriteClip`; `DataRegistry` loads `data/animations.json` into a keyed clip map (`sprite(key)`) plus a unit→sprite-set map (`unitSpriteSet`). The view models look up clips by key and fill `DrawSprite.texturePath`; `SfmlRenderManager` loads textures by path (cached). A readable `animations.json` is authoritative (replaces seeds), so clips can be added/removed/changed via data alone.
- `include/core/model/PlayerResourceState.hpp` defines the player resource snapshot stored by `GameWorld` and forwarded to the HUD.
- `include/core/factory` and `include/core/prototype` create game objects from static/prototype data.
- `include/core/world`, `include/core/path`, and `src/core/world` contain grid/world representation, coordinate transforms, path queries, and map-facing logic. `WorldRuntimeServices` owns transient runtime feedback events, active effect lifetimes, and the per-tick spatial grid used for optimized radius queries.
- `include/core/render` defines render commands, queues, context, and render-manager interfaces, including the `PlaySound` command used to forward simulation feedback to the platform renderer.
- `include/core/ui` and `src/core/ui` contain reusable UI elements such as text boxes and select boxes.
- `include/core/font` and `src/core/font` contain font loading and font metric abstractions.
- `include/core/viewmodel` and `src/core/viewmodel` build render/UI-facing view models.

## Game Layer

- `include/game/login` and `src/game/login` contain the login scene, UI manager, and logic manager.
- `include/game/lobby` and `src/game/lobby` contain the lobby scene, UI manager, and logic manager.
- `include/game/game` and `src/game/game` contain the main gameplay scene, UI manager, and logic manager.
- `include/game/game/systems` and `src/game/game/systems` contain the gameplay systems. `GameLogicManager` only coordinates them: it routes `LogicCommand`s (`registerCommandHandlers`), runs them in a fixed per-tick order (`tick`), and owns match lifecycle, save/load orchestration and replay. Reordering systems in `tick` changes replay hashes.
  - `SelectionSystem`, `ControlGroupSystem`, `CollisionSystem`, `MovementSystem`: selection, control groups, unit/structure collision, path orders.
  - `GameplayQueries`: shared read-only lookups (command target under the cursor, closest drop-off/resource, buildings by type, tech state, first selected building/worker).
  - `UnitOrderSystem`: player orders for selected units (move/rally, smart right-click, attack-move, patrol, gather, stop, hold) and Shift-queued order chains.
  - `CombatSystem`: automatic engagement passes (post-kill retarget, attack-move, patrol, hold, idle auto-acquire).
  - `EconomySystem`: gather orders/redirects, resource deliveries, per-tick supply recount.
  - `ProductionSystem`: train/cancel, buffered unit spawns, rally handling.
  - `ConstructionSystem`: footprint placement checks, AI site search, player build orders.
  - `EnemyAiController`: the scripted enemy (build order, production, workers, defense, waves) and its save state.
  - `MatchRules`: victory/defeat. `FeedbackSystem`: hit/death/completion sound and effect events. `MatchSnapshot`: save-file JSON format (capture/restore).
- `MovementSystem` throttles A* (fresh orders and periodic replans each have their own per-tick budget) and periodically re-plans traveling units to their current final target (`kRepathInterval`) so paths adapt to the changing world.

## Platform Layer

- `include/platform/IWindow.hpp` defines the platform window abstraction.
- `include/platform/sfml` and `src/platform/sfml` contain SFML-backed windowing, rendering, HUD overlay, font metrics, audio playback, and asset-path integration.
- `SfmlAudioManager` is the SFML-backed `IAudioManager` driven by `AudioThread`: it registers `PlayCueCommand`/`PlaySoundCommand`/`PlayMusicCommand`/`StopMusicCommand`/`SetMasterVolumeCommand` handlers on the `AudioCommandRouter`, lazily caches `sf::SoundBuffer`s, pools one-shot voices, and streams one `sf::Music`. It loads files from the configured `AudioRoot` (`assets/audio`) and synthesizes one-shot tones per gameplay `SoundCue`. All playback runs on the audio thread.
- Gameplay `SoundCue` feedback flows logic → `RenderQueue` (`render::PlaySound`) → `SfmlRenderManager`, which forwards a `PlayCueCommand` to the `AudioCommandBus` rather than playing inline; tone synthesis lives in `SfmlAudioManager`, not the render thread.
- `SfmlHudOverlay` draws the ImGui HUD command panel and emits gameplay UI input through `UICommandBus`; `GameUIManager` translates those UI inputs into `LogicCommand` payloads when enough target data exists.
- HUD resource numbers are supplied through `RenderQueue` using `UpdateHudResources`; avoid hardcoding live economy values in `SfmlHudOverlay`.

## Maintenance Notes

- When adding a new `.cpp` or header that must build, update `CMakeLists.txt` together with the source change.
- Keep header and implementation folders mirrored where practical so future agents can find ownership quickly.
- If a change introduces a new subsystem, scene, platform backend, asset root, or build target, update this document in the same commit.
