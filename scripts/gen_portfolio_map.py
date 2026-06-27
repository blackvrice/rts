#!/usr/bin/env python3
"""Generates data/maps/portfolio.json: a showcase scenario tuned for the
portfolio video (base -> forest/canyon pathfinding -> central battle -> assault
on the enemy base in the opposite corner).

Re-run after editing layout: python scripts/gen_portfolio_map.py
Coordinates: world units are pixels; tile = 16px; map is 256x256 tiles (4096px).
Walls are authored in TILE space and filled here into blockedTiles.
"""
import json
import os

TILE = 16
W = H = 256                      # tiles
WORLD = W * TILE                 # 4096 px

blocked = set()


def wall(x0, y0, x1, y1, gaps=()):
    """Fill an inclusive tile rectangle, skipping any [gx0, gx1] x-gap (or
    [gy0, gy1] when the wall is vertical) so a chokepoint stays passable."""
    for ty in range(y0, y1 + 1):
        for tx in range(x0, x1 + 1):
            in_gap = False
            for g in gaps:
                axis, lo, hi = g
                v = tx if axis == "x" else ty
                if lo <= v <= hi:
                    in_gap = True
                    break
            if not in_gap:
                blocked.add((tx, ty))


# --- Pathfinding barriers between the player base (bottom-left) and the field.
# Lower ridge just above the player base: one central pass funnels the army out.
wall(34, 188, 150, 192, gaps=[("x", 94, 104)])
# Vertical spur near the centre so routes must weave, not go straight.
wall(126, 128, 130, 182)
# Mid ridge between the centre and the enemy side: pass offset from the lower one.
wall(104, 120, 214, 124, gaps=[("x", 150, 162)])
# Canyon mouth right before the enemy base: tight chokepoint for the final push.
wall(198, 70, 202, 116, gaps=[("y", 96, 104)])
wall(214, 70, 218, 116, gaps=[("y", 96, 104)])
# A few scattered boulders in the open field for obvious avoidance on camera.
for bx, by in [(150, 150), (175, 145), (110, 160), (165, 168), (140, 175)]:
    wall(bx, by, bx + 2, by + 2)

# --- Entities (world pixels) -------------------------------------------------
buildings = [
    # Player base, bottom-left.
    {"type": "town_hall", "team": "player", "x": 560,  "y": 3360},
    {"type": "barracks",  "team": "player", "x": 900,  "y": 3360},
    # Enemy base, top-right (the assault objective).
    {"type": "town_hall", "team": "enemy",  "x": 3440, "y": 620},
    {"type": "barracks",  "team": "enemy",  "x": 3060, "y": 620},
    {"type": "barracks",  "team": "enemy",  "x": 3440, "y": 980},
]

units = [
    # Player workers (resource demo).
    {"type": "worker", "team": "player", "x": 360, "y": 3240},
    {"type": "worker", "team": "player", "x": 410, "y": 3240},
    {"type": "worker", "team": "player", "x": 360, "y": 3300},
    {"type": "worker", "team": "player", "x": 410, "y": 3300},
    # Player starting army (selection / control / combat demo).
    {"type": "warrior", "team": "player", "x": 720,  "y": 3520},
    {"type": "warrior", "team": "player", "x": 784,  "y": 3520},
    {"type": "warrior", "team": "player", "x": 848,  "y": 3520},
    {"type": "warrior", "team": "player", "x": 912,  "y": 3520},
    {"type": "archer",  "team": "player", "x": 720,  "y": 3600},
    {"type": "archer",  "team": "player", "x": 800,  "y": 3600},
    {"type": "archer",  "team": "player", "x": 880,  "y": 3600},
    {"type": "marine",  "team": "player", "x": 1000, "y": 3520},
    {"type": "marine",  "team": "player", "x": 1064, "y": 3520},
    # Enemy scouting patrol near the centre (first contact, ~1:20 beat).
    {"type": "warrior", "team": "enemy", "x": 2200, "y": 1640},
    {"type": "archer",  "team": "enemy", "x": 2264, "y": 1640},
    # Enemy workers.
    {"type": "worker", "team": "enemy", "x": 3650, "y": 440},
    {"type": "worker", "team": "enemy", "x": 3700, "y": 440},
    {"type": "worker", "team": "enemy", "x": 3750, "y": 440},
    # Enemy garrison (final large-scale battle).
    {"type": "warrior", "team": "enemy", "x": 3260, "y": 820},
    {"type": "warrior", "team": "enemy", "x": 3324, "y": 820},
    {"type": "warrior", "team": "enemy", "x": 3388, "y": 820},
    {"type": "warrior", "team": "enemy", "x": 3452, "y": 820},
    {"type": "archer",  "team": "enemy", "x": 3260, "y": 900},
    {"type": "archer",  "team": "enemy", "x": 3340, "y": 900},
    {"type": "archer",  "team": "enemy", "x": 3420, "y": 900},
    {"type": "marine",  "team": "enemy", "x": 3560, "y": 900},
    {"type": "marine",  "team": "enemy", "x": 3624, "y": 900},
    {"type": "marine",  "team": "enemy", "x": 3688, "y": 900},
]

resources = [
    # Player base minerals + forest.
    {"type": "gold", "x": 240, "y": 3180},
    {"type": "gold", "x": 240, "y": 3320},
    {"type": "gold", "x": 240, "y": 3460},
    {"type": "wood", "x": 620, "y": 3720},
    {"type": "wood", "x": 760, "y": 3720},
    # Contested centre expansion (mid-game economy + objective to fight over).
    {"type": "gold", "x": 2320, "y": 2240},
    {"type": "gold", "x": 2420, "y": 2240},
    {"type": "wood", "x": 2520, "y": 2040},
    {"type": "wood", "x": 2600, "y": 2040},
    # Enemy base economy.
    {"type": "gold", "x": 3880, "y": 520},
    {"type": "gold", "x": 3880, "y": 660},
    {"type": "gold", "x": 3880, "y": 800},
    {"type": "wood", "x": 3380, "y": 360},
    {"type": "wood", "x": 3480, "y": 360},
]

# Forest belt flanking the lower pass: pure decoration/obstacle + gatherable.
# Sits player-side of the lower ridge (tile y~195) with the chokepoint kept clear.
for fx in range(560, 2200, 90):
    if 1480 <= fx <= 1700:   # keep the central pass (tile x 94-104) open
        continue
    resources.append({"type": "wood", "x": fx, "y": 3120})


def footprint_tiles(width_t, height_t, px, py):
    cx, cy = px / TILE, py / TILE
    half_w, half_h = width_t / 2.0, height_t / 2.0
    out = []
    for ty in range(int(cy - half_h), int(cy + half_h) + 1):
        for tx in range(int(cx - half_w), int(cx + half_w) + 1):
            out.append((tx, ty))
    return out


# --- Validation: no entity footprint should sit on a blocked tile -----------
FOOT = {"town_hall": (4, 4), "barracks": (3, 3),
        "gold": (2, 2), "wood": (2, 2)}
problems = []
for e in buildings + resources:
    fw, fh = FOOT[e["type"]]
    for t in footprint_tiles(fw, fh, e["x"], e["y"]):
        if t in blocked:
            problems.append((e, t))
for u in units:
    t = (u["x"] // TILE, u["y"] // TILE)
    if t in blocked:
        problems.append((u, t))
for e in buildings + units + resources:
    if not (0 <= e["x"] < WORLD and 0 <= e["y"] < WORLD):
        problems.append((e, "out-of-bounds"))

if problems:
    print("VALIDATION PROBLEMS:")
    for p in problems:
        print("  ", p)
    raise SystemExit(1)

doc = {
    "_comment": "Portfolio showcase scenario (auto-generated by scripts/gen_portfolio_map.py). "
                "Player base bottom-left, enemy base top-right; forest/canyon chokepoints "
                "between them for the pathfinding demo. Edit the script, not this file.",
    "width": W,
    "height": H,
    "tileSize": TILE,
    "startResources": {
        "player": {"gold": 350, "wood": 200},
        "enemy":  {"gold": 600, "wood": 400},
    },
    "buildings": buildings,
    "units": units,
    "resources": resources,
    "blockedTiles": [[x, y] for (x, y) in sorted(blocked)],
}

out_path = os.path.join(os.path.dirname(__file__), "..", "data", "maps", "portfolio.json")
out_path = os.path.normpath(out_path)
with open(out_path, "w", encoding="utf-8") as f:
    json.dump(doc, f, indent=2)
    f.write("\n")

print(f"wrote {out_path}")
print(f"  buildings={len(buildings)} units={len(units)} resources={len(resources)} "
      f"blockedTiles={len(blocked)}")
