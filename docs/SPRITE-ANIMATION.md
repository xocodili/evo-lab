# Sprite animation — engine module design

**Status:** Implemented (2026-08-30) — see `engine/gfx/sprites/` and `#include "engine/gfx/sprites/Sprites.hpp"`.  
**Companion art guide:** [ART-STYLE.md](ART-STYLE.md) (palette, grid, mouth sheet, scale vs sim radii)  
**Principle:** Reusable **2D sprite-sheet animation** in `evolab_engine`. Game code (`evolab_game`) chooses clips and drives state; sim code never imports textures.

---

## Problem

Today neuron nodes render as **solid-colour screen-space billboards** (`OrganismDrawer::appendCellBillboard`, `kNeuronDiameterPx`). That is sufficient for prototyping but cannot express:

- Per-type iconography (mouth, eye, actuator wiggle, CPU electron)
- **Idle vs active** clips (paid P scan, A stroke, M taste spike, C dispatch)
- Shared atlas assets and consistent pixel scale across HUD + world

We need a small, testable engine module for **animated sprite sheets** that plugs into the existing OpenGL billboard path without pulling sim types into the engine.

---

## Architectural split

```
┌─────────────────────────────────────────────────────────────────┐
│  §3 Game — evolab_game                                          │
│  OrganismDrawer: map NeuronType + activity flags → clip names   │
│  GameRenderer: batch textured billboards after terrain/energon    │
└────────────────────────────▲────────────────────────────────────┘
                             │ SpriteDrawInstance[], clip ids
┌────────────────────────────┴────────────────────────────────────┐
│  §2 Assets — resources/sprites/ + manifest JSON                     │
│  atlases PNG, per-clip frame rects, fps, loop mode                │
└────────────────────────────▲────────────────────────────────────┘
                             │ load via resolveAssetPath
┌────────────────────────────┴────────────────────────────────────┐
│  §1 Engine — engine/gfx/sprites/  (NEW)                           │
│  SpriteAtlas, SpriteClip, SpriteAnimator, SpriteBillboardBuilder  │
│  zero sim deps; OpenGL texture upload optional sub-module       │
└───────────────────────────────────────────────────────────────────┘
```

| Layer | Path | Reusable elsewhere? |
|-------|------|---------------------|
| **§1 Engine** | `engine/include/engine/gfx/sprites/` | Yes — any top-down 3D or pure 2D overlay |
| **§2 Assets** | `resources/sprites/` | Project content — see [ART-STYLE.md](ART-STYLE.md) |
| **§3 Game** | `OrganismDrawer`, `GameRenderer` | evo-lab specific mapping |

---

## §1 — Core types (engine)

Namespace: `evolab::engine::gfx::sprites`

### 1.1 Data model

```cpp
struct SpriteFrame {
  int x, y, w, h;       // pixel rect in atlas
  float durationSec;    // hold time (default 1/fps if omitted in manifest)
};

enum class SpriteLoopMode { Once, Loop, PingPong, HoldLast };

struct SpriteClip {
  std::string name;           // e.g. "perceptor_idle"
  std::vector<SpriteFrame> frames;
  SpriteLoopMode loop = SpriteLoopMode::Loop;
  float defaultFps = 12.0f;
};

struct SpriteAtlas {
  unsigned textureId = 0;     // 0 until uploaded
  int widthPx = 0;
  int heightPx = 0;
  std::unordered_map<std::string, SpriteClip> clips;
  // UV helpers: frame → normalized u0,v0,u1,v1
  SpriteUvRect uvForFrame(const SpriteFrame&) const;
};
```

```cpp
class SpriteAnimator {
public:
  void setClip(const SpriteClip* clip);  // nullptr → hidden
  void update(float dt);                 // advances local time, frame index
  const SpriteFrame* currentFrame() const;
  bool finished() const;                 // Once + past last frame
  void reset();
private:
  const SpriteClip* clip_ = nullptr;
  float localTime_ = 0.0f;
  int frameIndex_ = 0;
  int pingPongDir_ = 1;
};
```

```cpp
struct SpriteDrawInstance {
  float worldX, worldY, worldZ;
  float halfSizeWorld;       // same semantics as current billboard halfSize
  float tintR, tintG, tintB, tintA;
  const SpriteClip* clip;    // resolved by game layer
  float animTimeSec;         // optional: drive from sim tick for determinism
  int flipX = 0;             // mirror for heading-aware icons
};
```

**Determinism note:** For replay/tests, prefer `animTimeSec = simTick / fixedSimHz` over wall-clock `dt` when building instances from sim state. Engine `update(dt)` remains for HUD/menu.

### 1.2 Atlas loading

**Manifest format (JSON v1)** — one file per atlas, e.g. `resources/sprites/camp_neurons.json`:

```json
{
  "image": "camp_neurons.png",
  "clips": {
    "mouth_idle": { "fps": 8, "loop": "loop", "frames": [[0,0,16,16], [16,0,16,16]] },
    "mouth_taste": { "fps": 12, "loop": "once", "frames": [[32,0,16,16], [48,0,16,16], [64,0,16,16]] },
    "perceptor_idle": { "fps": 6, "loop": "loop", "frames": [[0,16,16,16]] },
    "perceptor_scan": { "fps": 15, "loop": "loop", "frames": [[16,16,16,16], [32,16,16,16]] }
  }
}
```

Loader API:

```cpp
class SpriteAtlasLibrary {
public:
  bool loadAtlas(const std::string& manifestPath, const std::string& exeBasePath);
  const SpriteAtlas* find(const std::string& atlasId) const;
  const SpriteClip* findClip(const std::string& atlasId, const std::string& clipName) const;
  void uploadAll(GlContext& gl);   // create GL textures once context exists
  void shutdown();
};
```

Uses existing `resolveAssetPath(exeBasePath, relativePath)` from `UiAssets.hpp`.

**Future:** optional Aseprite/Liberated JSON import; v1 is hand-authored rects.

### 1.3 Billboard geometry

Current `CellVertex` packs `{pos, rgba, lx, ly}` where `lx,ly` are **corner weights** (−1…1), not UVs.

**Option A (recommended v1):** New vertex type for textured sprites:

```cpp
struct SpriteVertex {
  float x, y, z;
  float r, g, b, a;
  float lx, ly;   // billboard corner (unchanged math)
  float u, v;     // atlas UV
};
```

`SpriteBillboardBuilder::append(SpriteDrawInstance, eyePos, mvp, viewport, SpriteAnimator&)` reuses the same camera-facing basis as `appendCellBillboard` but sets UVs from `SpriteAtlas::uvForFrame`.

**Option B:** Extend `CellVertex` with u,v — breaks current shader stride assumptions; avoid unless we unify all billboards.

### 1.4 Rendering hook

New minimal shader pair in `engine/gfx/sprites/` or `game/GameShaders`:

- Vertex: world position + billboard corner → clip space (same as cells)
- Fragment: `sample2D(atlas, vUV) * vTint`
- **Alpha cutout or blend:** default BLEND with premultiplied-friendly PNG

`GameRenderer` draw order (unchanged conceptually):

1. Terrain
2. Energon strings
3. Organism bones / axons (lines)
4. **Sprite billboards** (neurons, future props)
5. Solid cell fallbacks (if clip missing)
6. Text overlays (HUD, inspector)

One atlas bind per batch where possible; sort instances by `textureId` if multiple atlases.

---

## §2 — Asset conventions

| Rule | Value |
|------|--------|
| Pixel grid | 16×16 or 32×32 cells; power-of-two atlas (256² or 512²) |
| Pivot | Center-bottom for ground-anchored nodes (mouth on terrain) |
| Naming | `{neuron}_{state}` — `mouth_idle`, `mouth_taste`, `actuator_stroke`, `computer_dispatch`, `perceptor_scan` |
| Active clips | Loop while sim flag true; fall back to idle on `finished()` for `Once` clips |
| Colour tint | Keep existing neuron RGB as multiply tint for team/state without redrawing art |

Place atlases under `resources/sprites/`; CMake `POST_BUILD` copy mirrors `resources/sprites/` next to the executable.

---

## §3 — Game integration (later, not in engine PR)

`OrganismDrawer` mapping sketch:

| Neuron | Idle clip | Active clip (sim flag) |
|--------|-----------|-------------------------|
| Mouth | `mouth_idle` | `mouth_taste` when `mouthTasteGradient` or feed intent |
| Perceptor | `perceptor_idle` | `perceptor_scan` when `lastPerceptScanPaid` |
| Actuator | `actuator_idle` | `actuator_stroke` when `lastStrokePaid` |
| Computer | `computer_idle` | `computer_dispatch` when `lastComputerDispatchPaid` |

Activity flags already exist on `Organism` for inspector/HUD; drawer reads the same fields.

**Migration path:**

1. Engine module + one test atlas (solid-colour frames OK)
2. `GameRenderer` dual path: textured sprites where clip exists, solid quads fallback
3. Replace circles incrementally per neuron type
4. Remove solid neuron billboards when atlas complete

---

## §1.5 — API surface summary

| File | Responsibility |
|------|----------------|
| `SpriteAtlas.hpp/cpp` | Manifest parse, UV math, clip lookup |
| `SpriteAnimator.hpp/cpp` | Time integration, loop modes |
| `SpriteBillboardBuilder.hpp/cpp` | World billboard quads + UVs |
| `SpriteAtlasLibrary.hpp/cpp` | Load/cache/upload lifecycle |
| `SpriteTypes.hpp` | Shared structs/enums |

**Tests (engine, no GL):**

- JSON parse → frame count, fps, loop mode
- Animator: Loop wraps, Once finishes, PingPong reverses
- UV rect: pixel frame → normalized coords

**Optional GL test (manual):** load atlas in VisualApp debug overlay.

---

## Non-goals (v1)

- Skeletal bone animation / spine
- 3D mesh sprites
- Particle systems (birth firework stays procedural)
- Sim-side knowledge of clip names
- Automatic Aseprite pipeline (document only)

---

## Open questions

1. **Sim-tick vs render-tick animation** — default to sim-tick for organism icons so pause freezes animation; HUD uses render `dt`.
2. **Multiple mouths / P nodes** — one clip instance per `SkeletonNode`; same clip name, independent `animTimeSec`.
3. **Depth sort** — billboards already camera-facing; optional slight Y bias per node type to reduce z-fight (same as today).

---

## Success criteria

- [x] Engine loads one atlas from `resources/sprites/` without sim includes
- [ ] Unit tests pass for animator + manifest parse
- [ ] GameRenderer draws at least one textured neuron billboard beside existing solid quads
- [ ] Clip swap on `lastPerceptScanPaid` visible in VisualApp
- [ ] Documented mapping table for CAMP neuron icons (§3 above)
