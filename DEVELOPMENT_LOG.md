# Development Log

## 2026-06-05 - Source Structure Documentation Rule

- Updated `AGENTS.md` to require source-structure documentation updates when modules, folders, entry points, runtime systems, or build/runtime ownership changes.
- Added `SOURCE_STRUCTURE.md` with the current top-level layout, core/game/platform layer responsibilities, build entry points, and maintenance notes.
- Verification: reviewed repository file layout with `rg --files`, inspected `CMakeLists.txt`, and left unrelated local source changes untouched.

## 2026-06-05 - Shared AI Documentation Rule

- Updated `AGENTS.md` to require Markdown development notes for completed tasks.
- Future agents should record what changed, why it changed, how it was verified, and any remaining follow-up.
- Verification: reviewed the `AGENTS.md` diff and left unrelated local changes untouched.
