# Development Log

## 2026-06-06 - Team-Aware Right Click Attack Orders

- Added team ownership to gameplay elements and debug units so command resolution can distinguish local player units from enemy units.
- Updated right-click attack command handling to first inspect the clicked unit: same-team or neutral targets now resolve as move/approach orders, while opposing-team targets resolve as attacks.
- Split unit command action from render animation action so attack-chasing units keep the run animation until the target enters weapon range, then switch to the attack animation while dealing damage.
- Verification: built `RTS` with `C:\Program Files\JetBrains\CLion 2026.1.2\bin\cmake\win\x64\bin\cmake.exe --build cmake-build-debug --target RTS -- -j1`, then launched `cmake-build-debug\RTS.exe` and confirmed it stayed running for 5 seconds.

## 2026-06-06 - StarCraft Command Hotkeys

- Added StarCraft-style gameplay command hotkeys in `GameUIManager`: Move `M`, Attack `A`, Stop `S`, Hold `H`, Patrol `P`, Gather `G`, Build `B`, Repair `R`, and Cancel `Escape`.
- Routed hotkeys through the same `GameplayInputCommand` handling path as the HUD command buttons so keyboard and UI command input stay aligned with the current `LogicCommand` bridge.
- Preserved modifier combinations such as control-group shortcuts by ignoring gameplay command hotkeys when Ctrl, Alt, or System modifiers are held.
- Verification: built `RTS` with `C:\Program Files\JetBrains\CLion 2026.1.2\bin\cmake\win\x64\bin\cmake.exe --build cmake-build-debug --target RTS -- -j1`, then launched `cmake-build-debug\RTS.exe` and confirmed it stayed running for 5 seconds.

## 2026-06-06 - Render Lock Reentry Fix

- Fixed a render-thread stall where `GameUIManager::render()` tried to acquire `GameWorld`'s read lock while `GameScene::render()` already held the same non-recursive `shared_mutex` read lock.
- Kept scene-level locking as the owner for UI sync/update/render world reads and removed the nested HUD snapshot lock from `GameUIManager::render()`.
- Added comments in `GameScene` to make the single-lock ownership clear for future HUD/world read changes.
- Verification: built `RTS` with `C:\Program Files\JetBrains\CLion 2026.1.2\bin\cmake\win\x64\bin\cmake.exe --build cmake-build-debug --target RTS -- -j1`, then launched `cmake-build-debug\RTS.exe` and confirmed it stayed running for 5 seconds.
- Follow-up: longer term, HUD/world snapshots should be collected through an explicit read-only scene snapshot API so render code does not depend on implicit outer locks.

## 2026-06-06 - Selected Unit HUD Details

- Added `UpdateHudSelection` render data so the HUD selection panel can receive the current selected-unit snapshot from gameplay UI code.
- `GameUIManager` now scans selected live units while rendering, sends selected count plus the first selected unit's name, action, HP, and world position to the HUD.
- Replaced hardcoded selection-panel text in `SfmlHudOverlay` with the selected unit snapshot while preserving the existing portrait and command panel layout.
- Verification: built `RTS` with `C:\Program Files\JetBrains\CLion 2026.1.2\bin\cmake\win\x64\bin\cmake.exe --build cmake-build-debug --target RTS -- -j1`, then launched `cmake-build-debug\RTS.exe` and confirmed it stayed running for 5 seconds.
- Follow-up: unit stats such as armor, damage, range, and display name should move into real unit/static data getters before the panel can show a full StarCraft-style stat card.

## 2026-06-06 - Player Resource HUD State

- Added `PlayerResourceState` as the shared model for player economy values: gold, wood, food usage/capacity, and army.
- Added player-id keyed resource storage to `GameWorld`, initialized the local player resource state, and exposed read/update APIs for future economy systems.
- Added `UpdateHudResources` render data so `GameUIManager` can forward the local player's current resource snapshot through `RenderQueue` each frame.
- Replaced hardcoded SFML HUD resource numbers with values from the resource snapshot while preserving the existing top-right resource pill layout and number formatting.
- Verification: built `RTS` with `C:\Program Files\JetBrains\CLion 2026.1.2\bin\cmake\win\x64\bin\cmake.exe --build cmake-build-debug --target RTS -- -j1`, then launched `cmake-build-debug\RTS.exe` and confirmed it stayed running for 5 seconds.
- Follow-up: economy commands still need to call `GameWorld::setPlayerResources(...)` or a future resource system when gathering, spending, supply, and army counts change.

## 2026-06-06 - Movement Stops At Occupied Targets

- Updated movement collision handling so a moving unit stops when the blocker is occupying the unit's final requested target instead of repeatedly generating avoidance paths around it.
- Added an approach-path fallback for occupied goals: if the exact clicked cell cannot be pathfound because it is blocked by a unit or future gameplay element, the mover paths to a nearby free cell and stops there.
- Broadened runtime move-blocker checks from `Unit` only to any live `IGameElement`, keeping the collision contract ready for building-like gameplay elements.
- Verification: built `RTS` with `C:\Program Files\JetBrains\CLion 2026.1.2\bin\cmake\win\x64\bin\cmake.exe --build cmake-build-debug --target RTS -- -j1`, then launched `cmake-build-debug\RTS.exe` and confirmed it stayed running for 5 seconds.
- Follow-up: building entities still need a runtime `IGameElement` representation and a size/radius source before buildings can block movement with building-specific footprints.

## 2026-06-06 - HUD Gameplay Input Commands

- Added `GameplayInputCommand` and `GameplayInputAction` so HUD/gameplay UI inputs have a command layer aligned with the current `LogicCommand` action set.
- Wired the SFML HUD command buttons to emit gameplay UI commands through `UICommandBus`, then translated supported inputs in `GameUIManager`: Move and Attack set the next world-click order, while Stop, Hold, and Cancel forward matching logic commands.
- Updated `SOURCE_STRUCTURE.md` to note that `SfmlHudOverlay` now publishes gameplay UI input and `GameUIManager` owns the UI-to-logic translation.
- Verification: built `RTS` with `C:\Program Files\JetBrains\CLion 2026.1.2\bin\cmake\win\x64\bin\cmake.exe --build cmake-build-debug --target RTS -- -j1`, then launched `cmake-build-debug\RTS.exe` and confirmed it stayed running for 5 seconds.
- Follow-up: Patrol, Train, Build, Gather, Return, Repair, and Ability UI actions are represented as UI input commands, but they still need selected unit, target, building, resource, or ability payloads before they can create complete logic commands.

## 2026-06-05 - Repair Logic Command

- Added `RepairCommand` so the logic command set now covers the HUD command list's repair action.
- Verification: built `RTS` with the CLion-bundled CMake/Ninja using `C:\msys64\ucrt64\bin` on `PATH`, then launched `cmake-build-debug\RTS.exe` and confirmed it stayed running for 5 seconds.
- Follow-up: repair still needs gameplay handling once workers, repairable targets, and command UI dispatch are implemented.

## 2026-06-05 - HUD Selected Unit Portrait

- Added a selected-unit HUD portrait path that reuses the chosen unit's current sprite texture, animation frame, and transparent-trim bounds.
- The SFML renderer now scans world sprite commands for the selected unit, draws the ImGui HUD, then overlays the unit sprite inside the existing selection portrait panel before drawing the custom cursor.
- Verification: built `RTS` with the CLion-bundled CMake/Ninja using `C:\msys64\ucrt64\bin` on `PATH`, then launched `cmake-build-debug\RTS.exe` and confirmed it stayed running for 5 seconds.
- Follow-up: the HUD text is still placeholder data and can be wired to real selection stats next.

## 2026-06-05 - Sprite Mouse Cursor

- Replaced the native OS cursor with a Tiny Swords cursor sprite rendered by the SFML renderer.
- The cursor now uses `UI Elements/UI Elements/Cursors/Cursor_01.png`, is drawn after the HUD so it remains visible over UI, and applies a hotspot offset so the arrow tip tracks the actual mouse position.
- Verification: built `RTS` with the CLion-bundled CMake/Ninja using `C:\msys64\ucrt64\bin` on `PATH`, then launched `cmake-build-debug\RTS.exe` and confirmed it stayed running for 5 seconds.
- Follow-up: cursor state variants such as attack/selection can be mapped later to `Cursor_02.png` through `Cursor_04.png`.

## 2026-06-05 - Unit Attack Orders And Combat Tick

- Added right-click attack intent: game UI now sends an attack order with the clicked world position, and game logic attacks a living unselected unit near that point or falls back to normal movement on empty ground.
- Implemented selected-unit attack dispatch, attack target picking, attack chase, cooldown damage, retaliation, and dead-unit command filtering.
- Dead units are ignored by selection/collision and no longer render as active units after death.
- Verification: built `RTS` with the CLion-bundled CMake/Ninja using `C:\msys64\ucrt64\bin` on `PATH`, then launched `cmake-build-debug\RTS.exe` and confirmed it stayed running for 5 seconds.
- Follow-up: team ownership is still absent, so "enemy" currently means any living game element that is not part of the current selection.

## 2026-06-05 - Source Structure Documentation Rule

- Updated `AGENTS.md` to require source-structure documentation updates when modules, folders, entry points, runtime systems, or build/runtime ownership changes.
- Added `SOURCE_STRUCTURE.md` with the current top-level layout, core/game/platform layer responsibilities, build entry points, and maintenance notes.
- Verification: reviewed repository file layout with `rg --files`, inspected `CMakeLists.txt`, and left unrelated local source changes untouched.

## 2026-06-05 - Shared AI Documentation Rule

- Updated `AGENTS.md` to require Markdown development notes for completed tasks.
- Future agents should record what changed, why it changed, how it was verified, and any remaining follow-up.
- Verification: reviewed the `AGENTS.md` diff and left unrelated local changes untouched.
