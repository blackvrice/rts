# Development Log

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
