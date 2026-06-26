# RTS QA Checklist

Use this checklist for the manual pass that complements the headless smoke test.
Record failures in `DEVELOPMENT_LOG.md` with the exact build, command path, and
short reproduction steps.

## Startup

- Build `RTS` with the CLion-bundled CMake path.
- Launch `cmake-build-debug\RTS.exe` from the repository root.
- Confirm the game reaches the lobby without missing DLL, data, or asset errors.

## Core Match Flow

- Start a normal game from the lobby.
- Select one unit, box-select several units, and clear selection by clicking empty terrain.
- Right-click walkable terrain and confirm selected units move without obvious path stalls.
- Right-click an enemy and confirm attack orders continue until the target dies.
- Use attack-move, patrol, stop, and hold-position commands on combat units.
- Select workers and gather gold/wood; confirm carrying, drop-off, and HUD resource deltas.
- Place a valid building, try an invalid blocked placement, and confirm the feedback path is clear.
- Train units from a building, cancel at least one queued item, and confirm resource/food changes.
- Fight until at least one unit and one building die; confirm corpse/dead state does not keep blocking commands.
- Play until victory or defeat and confirm the result overlay/input lock appears.

## Visibility And Navigation

- Toggle the F3 debug overlay and confirm tick/hash text updates.
- Move player units into unexplored terrain and confirm fog of war reveals and re-hides correctly.
- Use camera pan/zoom and verify world input still maps to the clicked location.
- Use the minimap for camera movement and world orders, including during fogged areas.

## Save And Replay

- Press F5 during a live match, then F9, and confirm the match restores to the saved snapshot.
- After load, issue at least one move, gather, build, train, and attack command.
- Exit a live match to the lobby and confirm a replay file is created under AppData.
- Open the replay browser, load the newest replay, and confirm live gameplay input is ignored.
- Let a replay reach the end and confirm it freezes on the final frame instead of becoming controllable.
- Watch the log for replay hash divergence messages.

## Regression Notes

- If a check fails, capture the latest commit hash, the map path, the command used, and the first visible bad behavior.
- Prefer one focused fix per failure so the next automated or manual pass can isolate regressions.
