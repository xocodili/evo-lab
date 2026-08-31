# Art style — evo-lab simulator sprites

**Status:** Living guide (2026-08-30)  
**Engine companion:** [SPRITE-ANIMATION.md](SPRITE-ANIMATION.md) (atlas loading, clips, billboards in `evolab_engine`)  
**Public palette / brochure tone:** [MARKETING-COMMS.md](MARKETING-COMMS.md) §1

Simulator art lives under **`resources/`** (not `assets/`). Marketing diagrams and fonts stay in `assets/docs/` and `assets/fonts/`.

---

## 1. Scope (v1 skeleton)

This pass is **placement + scale only** — one mouth atlas, neurons still solid billboards. Future passes add perceptor, actuator, computer sheets and richer animation without moving paths.

| Layer | Location | Owns |
|-------|----------|------|
| **Content** | `resources/sprites/` | PNG atlases + JSON manifests |
| **Engine** | `engine/gfx/sprites/` | Load, UV, animate, batch draw |
| **Game** | `OrganismDrawer`, `GameRenderer` | Neuron type → clip, screen diameter, tint |

---

## 2. Style guides

### 2.1 Visual language (in-world)

- **Read at a glance:** silhouettes must read at 6–10 px on screen; no hairline detail.
- **1950s brochure DNA:** flat fills, limited palette, warm coral/mustard accents — aligned with [MARKETING-COMMS.md](MARKETING-COMMS.md), but **pixel grid** not halftone print.
- **Keyhole mouth motif:** circular aperture on a short stalk — matches lifecycle diagram **M MOUTH** grammar.
- **Transparency:** atlas background is **magenta/black in source**; engine treats fully transparent pixels in PNG alpha. Idle mouth is mostly outline; eating fills the aperture.

### 2.2 Canonical palette (simulator sprites)

| Token | Hex (approx) | Use |
|-------|--------------|-----|
| Mouth body | `#e8956a` | Ring + stalk |
| Mouth eating fill | `#c0392b` → `#e74c3c` | Frames 3–4 pulse |
| Perceptor (future) | `#3498db` | Eye / scan wedge |
| Actuator (future) | `#2a9d8f` | Flagella stroke |
| Computer hub (future) | `#f4a261` | C dispatch spark |
| Neuron fallback tint | Existing `OrganismDrawer` RGB | Multiply tint on sprites |

Keep **≤ 16 colours per atlas** for consistency and small PNG size.

### 2.3 Pixel grid & scale

| Rule | Value |
|------|--------|
| Frame cell | **16×16 px** |
| Atlas layout | Horizontal strip or power-of-two sheet |
| Neuron billboard diameter | **8 px** (`kNeuronDiameterPx`) |
| Mouth sprite diameter | **8 px** (`kMouthSpriteDiameterPx`) — same scale as neuron |
| Mouth **sticky** adhesion (sim) | Anchor discovery, prune, and chew co-advect all use `kMouthStickyRadiusFactor` (0.75× cellSize). Co-advect while approaching uses bite contact; while chewing, tether stays within sticky radius. Taste homing drives locomotion only — it does not widen the vacuum. |
| Pivot | Center of frame (billboard anchor at node world position) |

**Screen ↔ world:** `OrganismDrawer` sets `halfSizeWorld = (diameterPx × 0.5) × worldPerPx`. Sticky radius is tuned in sim constants so adhesion matches the **mouth sprite footprint**, not the taste horizon.

### 2.4 Clip naming

Pattern: `{neuron}_{state}` — see [SPRITE-ANIMATION.md](SPRITE-ANIMATION.md) §2.

| Clip | When |
|------|------|
| `mouth_idle` | No food contact last tick |
| `mouth_eating` | `lastMouthHadFoodContact` |

Future: `mouth_taste` optional one-shot if we want a distinct chemo pulse separate from chew.

### 2.5 Animation timing

| Clip | FPS | Loop | Notes |
|------|-----|------|-------|
| `mouth_idle` | 6 | loop | Subtle ring shimmer (2 frames) |
| `mouth_eating` | 10 | loop | Red fill pulse while attached / chewing |

Sim time drives animation: `animTimeSec = simTick / fixedSimHz` (deterministic replays).

---

## 3. Mouth sprite sheet (current)

**Files:** `resources/sprites/mouth_sprites.png`, `resources/sprites/mouth_sprites.json`

**Atlas:** 64×16 px — four frames in one row.

```
[ idle_0 | idle_1 | eat_0 | eat_1 ]
  0–15     16–31    32–47   48–63
```

| Frame | Rect (x,y,w,h) | Description |
|-------|----------------|-------------|
| 0 | 0,0,16,16 | Idle — hollow orange keyhole |
| 1 | 16,0,16,16 | Idle — 1 px outline shift |
| 2 | 32,0,16,16 | Eating — dark red fill |
| 3 | 48,0,16,16 | Eating — brighter red fill |

Manifest excerpt:

```json
{
  "image": "mouth_sprites.png",
  "clips": {
    "mouth_idle":   { "fps": 6,  "loop": "loop", "frames": [[0,0,16,16],[16,0,16,16]] },
    "mouth_eating": { "fps": 10, "loop": "loop", "frames": [[32,0,16,16],[48,0,16,16]] }
  }
}
```

CMake copies `resources/sprites/` next to `evo-lab.exe` on build. Default manifest path: `resources/sprites/mouth_sprites.json` (`SpriteAtlasLibrary::kDefaultMouthSpriteManifestRelPath`).

---

## 4. Checklist (new sprite)

- [ ] 16×16 (or documented multiple) on transparent PNG
- [ ] JSON manifest beside PNG in `resources/sprites/`
- [ ] Clip names follow `{neuron}_{state}`
- [ ] Diameter constant in `OrganismDrawer.hpp` matches art intent
- [ ] Sim interaction radii documented here if they must match art (mouth contact/sticky/taste)
- [ ] Unit test or smoke load via `test_sprite_animator.cpp`
- [ ] Cross-link engine behaviour in [SPRITE-ANIMATION.md](SPRITE-ANIMATION.md)

---

## 5. Planned atlases (not yet authored)

| Atlas | Clips (idle / active) |
|-------|------------------------|
| `camp_neurons.json` | perceptor, actuator, computer |
| Shared props | energon glint, death-feast dock flash |

Dedicated sprite-engine topic (batching, multi-atlas HUD) is deferred; this doc only tracks **content** conventions.
