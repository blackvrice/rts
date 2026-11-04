# Third-Party Asset Sources

Every third-party asset shipped with or loaded by the game is listed here. Add a row
whenever a new asset pack, sound, music track, or font is added. The license terms
must be confirmed from the original source before a public release (roadmap
`M9-E1-03`).

| Asset | Path in repo | Author / source | License status |
|---|---|---|---|
| Tiny Swords (Free Pack) — units, buildings, terrain, resources, UI elements, particle FX | `Tiny Swords (Free Pack)/Tiny Swords (Free Pack)/` | Pixel Frog — https://pixelfrog-assets.itch.io/tiny-swords | Not verified yet. The pack ships no license file; check the store page terms before release. |
| Gameplay sound cues | (none, synthesized at runtime by `SfmlAudioManager`) | Generated in code | Project code, no third-party license |

Notes:

- The original `Tiny Swords (Free Pack).zip` archive and its macOS `__MACOSX`
  metadata were removed from git on 2026-11-04. The extracted folder above is the
  tracked source of truth.
- `assets/audio/` is currently empty apart from its README. Record each `.wav` or
  `.ogg` file you add there in this table.
