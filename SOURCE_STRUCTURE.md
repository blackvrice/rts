# Source Structure

This document is a quick orientation map for future AI agents working in this repository. Keep it current when source ownership, folders, entry points, or build/runtime flow changes.

## Build And Entry Points

- `CMakeLists.txt` defines the `RTS` executable target and the source list used by the build.
- `src/main.cpp` is the application entry point.
- The project currently targets C++23 in CMake and links SFML 3, OpenGL, tmxlite, and ImGui.
- `include/platform/sfml/SfmlAssetPaths.hpp.in` is configured into the build directory so SFML platform code can locate packaged assets.
- Typical build commands are:

```powershell
cmake -S . -B build
cmake --build build
```

## Top-Level Layout

- `include/` contains project headers. Most implementation files under `src/` mirror these namespaces and folders.
- `src/` contains application, core, game, and platform implementations.
- `external/tmxlite/` is the tmxlite dependency used for map loading. Treat it as third-party code unless the task explicitly targets it.
- `Tiny Swords (Free Pack)/` contains art assets used by the game.
- `cmake-build-debug/` is a local build output folder and should not be treated as source.

## Core Layer

- `include/core/app` and `src/core/di` hold application startup and dependency wiring, including `GameApp`.
- `include/core/command` defines command types and command buses used between UI and logic.
- `include/core/thread` and `src/core/thread` contain logic-thread support.
- `include/core/manager` and `src/core/manager` contain cross-scene managers such as scene, camera, and path management.
- `include/core/scene` defines scene interfaces.
- `include/core/ecs` contains the custom ECS registry and entity primitives.
- `include/core/entity`, `include/core/model`, and `include/core/data` define gameplay data, entities, and model types.
- `include/core/factory` and `include/core/prototype` create game objects from static/prototype data.
- `include/core/world`, `include/core/path`, and `src/core/world` contain grid/world representation, coordinate transforms, path queries, and map-facing logic.
- `include/core/render` defines render commands, queues, context, and render-manager interfaces.
- `include/core/ui` and `src/core/ui` contain reusable UI elements such as text boxes and select boxes.
- `include/core/font` and `src/core/font` contain font loading and font metric abstractions.
- `include/core/viewmodel` and `src/core/viewmodel` build render/UI-facing view models.

## Game Layer

- `include/game/login` and `src/game/login` contain the login scene, UI manager, and logic manager.
- `include/game/lobby` and `src/game/lobby` contain the lobby scene, UI manager, and logic manager.
- `include/game/game` and `src/game/game` contain the main gameplay scene, UI manager, and logic manager.
- `include/game/game/systems` and `src/game/game/systems` contain gameplay systems such as collision, control groups, movement, and selection.

## Platform Layer

- `include/platform/IWindow.hpp` defines the platform window abstraction.
- `include/platform/sfml` and `src/platform/sfml` contain SFML-backed windowing, rendering, HUD overlay, font metrics, and asset-path integration.

## Maintenance Notes

- When adding a new `.cpp` or header that must build, update `CMakeLists.txt` together with the source change.
- Keep header and implementation folders mirrored where practical so future agents can find ownership quickly.
- If a change introduces a new subsystem, scene, platform backend, asset root, or build target, update this document in the same commit.
