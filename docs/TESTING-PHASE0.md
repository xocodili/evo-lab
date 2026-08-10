# Phase 0 — Testing & Verification Plan

How to verify the **Barren Earth** simulator (procedural 3D land, water, tides) without relying on “it looks fine.”

**Phase 2.x (life):** organism, chaos, and twin-mouth behavior are covered by Catch2 unit tests in `evo-lab-tests` — filters `[chaos]` and `[twomouth]`. Design and invariants are documented in [DESIGN-NOTES.md](DESIGN-NOTES.md) §2.5, §4.4.1.

---

## Verification layers

| Layer | Runs where | Needs GPU/window | Purpose |
|-------|------------|------------------|---------|
| **Unit** | `ctest` / CI | No | Logic, determinism, invariants |
| **Smoke** | CLI `--headless` | Optional OS window | App boots, ticks, exits cleanly |
| **Visual** | Interactive or screenshot | Yes | Human or golden-image check |
| **Acceptance** | Checklist | Yes | “Barren world simulator” definition |

---

## Components → tests

### 1. Build & link

| Test | Pass criteria |
|------|----------------|
| CMake configure + build | Exit 0 on Win/Mac/Linux |
| Link SDL2 + OpenGL + Catch2 | Binary runs `--help` |

**Smoke:** `evo-lab --version` prints build info.

---

### 2. Procedural heightmap (`generateWorld(seed)`)

**Unit tests (Catch2, no window):**

```cpp
TEST_CASE("heightmap deterministic") {
    auto a = generateHeightmap(42, 256);
    auto b = generateHeightmap(42, 256);
    REQUIRE(a == b);
}

TEST_CASE("heightmap seed sensitivity") {
    auto a = generateHeightmap(42, 256);
    auto b = generateHeightmap(43, 256);
    REQUIRE(a != b);
}

TEST_CASE("height bounds") {
    auto h = generateHeightmap(1, 128);
    for (float v : h.samples)
        REQUIRE(v >= h.minConfigured);
        REQUIRE(v <= h.maxConfigured);
}

TEST_CASE("not flat") {
    auto h = generateHeightmap(99, 64);
    REQUIRE(h.variance() > epsilon);  // not constant noise failure
}

TEST_CASE("has land and bathymetry") {
    auto h = generateHeightmap(7, 128);
    REQUIRE(h.countAbove(seaLevel) > 0);   // islands/land
    REQUIRE(h.countBelow(seaLevel) > 0);   // ocean basins
}
```

**Property:** same `(seed, resolution)` → identical floats bit-for-bit (use fixed noise lib / no race).

---

### 3. Depth & wetness (`heightAt`, `depthAt`, `isWet`)

**Unit tests:**

```cpp
TEST_CASE("depth formula") {
    setWaterLevel(10.0f);
    REQUIRE(depthAt(x, z) == Approx(10.0f - heightAt(x, z)).epsilon(0.001));
    // where height >= 10, depth == 0
}

TEST_CASE("isWet consistent with depth") {
    setWaterLevel(w);
    REQUIRE(isWet(x,z) == (depthAt(x,z) > 0));
}

TEST_CASE("tide changes wetness") {
    float h = heightAt(0, 0);  // pick fixed point
    setWaterLevel(h - 1);  REQUIRE(isWet(0,0));
    setWaterLevel(h + 1);  REQUIRE(!isWet(0,0));
}
```

---

### 4. Tide animation (`water_level(t)`)

**Unit tests:**

```cpp
TEST_CASE("tide periodic") {
    float t0 = tideWaterLevel(0);
    float t1 = tideWaterLevel(period);
    REQUIRE(t0 == Approx(t1));
}

TEST_CASE("tide within amplitude") {
    for (int i = 0; i < 100; ++i) {
        float w = tideWaterLevel(i * 0.1f);
        REQUIRE(w >= minLevel);
        REQUIRE(w <= maxLevel);
    }
}
```

**Integration:** advance sim 1000 ticks; record `water_level`; FFT or count local maxima ≥ 1 cycle.

---

### 5. Terrain mesh

**Unit tests:**

```cpp
TEST_CASE("mesh topology") {
    int res = 64;
    Mesh m = buildTerrainMesh(heightmap, res);
    REQUIRE(m.vertexCount == (res+1)*(res+1));
    REQUIRE(m.indexCount == res*res*6);
}

TEST_CASE("mesh matches heightmap") {
    // corner vertex Y == heightmap sample
}
```

**Visual:** no holes, no inverted triangles (back-face cull check in debug).

---

### 6. Camera

**Unit tests (math only):**

```cpp
TEST_CASE("orbit preserves distance") { ... }
TEST_CASE("zoom clamps min/max") { ... }
TEST_CASE("screenToWorld ray hits terrain") { ... }  // optional
```

**Manual / smoke:**

- Mouse drag → view changes  
- Scroll → zoom changes  
- `R` or UI → reset camera  

**Headless smoke:** feed synthetic input events; assert view matrix changed.

---

### 7. Render loop (OpenGL)

**Smoke test mode:**

```bash
evo-lab --headless --frames 120 --seed 42 --exit
# exit code 0; prints: smoke ok: seed=42 ...
```

Or: `tests/smoke/run_headless.ps1` (after build).

Registered in CTest as `smoke_headless`.

Optional:

```bash
evo-lab --screenshot tests/golden/phase0_seed42.png --seed 42 --frame 60
```

Compare to golden PNG (per-platform tolerance or manual update on intentional art changes).

**Debug builds:** `glGetError()` after init and after first draw → `GL_NO_ERROR`.

**Manual visual checklist:**

- [ ] Terrain visible (not flat grey void)  
- [ ] Water distinct from land (color/alpha/shader)  
- [ ] Coastline at plausible `water_level`  
- [ ] Depth readable (deep vs shallow: darker water or terrain slope)  
- [ ] Tide moves shoreline over ~30 s  
- [ ] Letterbox correct on resize  

---

### 8. ImGui / world controls

**Manual:**

| Action | Expected |
|--------|----------|
| Change seed + Regenerate | Heightmap changes; coastlines move |
| Same seed + Regenerate | Identical world (determinism) |
| Tide speed slider | Shoreline animation faster/slower |
| Pause tide | `water_level` frozen; wet map static |

**Smoke:** `--ui-test` script sends programmatic regenerate with seed 1 and 2; dump height checksum to stdout for CI diff.

---

### 9. Fixed timestep

**Unit / integration:**

```cpp
TEST_CASE("accumulator catches up") {
    SimClock clock(1.0/60.0);
    clock.advance(0.25);  // real time
    REQUIRE(clock.consumedTicks() == 15);  // 0.25 / (1/60)
}
```

**Runtime:** ImGui overlay shows `sim_tick` increasing steadily when unpaused.

---

## “Barren world simulator” — acceptance definition

Phase 0 is **done** when all pass:

1. **Deterministic world:** seed `N` → same geography every run.  
2. **Land + water exist:** unit tests prove above/below sea level samples.  
3. **3D presentation:** mesh terrain + water surface; orbit camera.  
4. **Depth:** `depthAt` correct; visually shallow shelves vs deep basins.  
5. **Living tide:** `water_level` cycles; wet footprint changes over time.  
6. **Barren:** zero organism/food/spawn code paths active.  
7. **Controls:** regenerate world from UI; tide tunable.  
8. **CI:** unit tests + headless smoke on at least one platform.

---

## Suggested repo layout

```
tests/
  unit/
    test_heightmap.cpp
    test_tide.cpp
    test_wetness.cpp
    test_mesh.cpp
    test_clock.cpp
  golden/
    phase0_seed42.png      # optional screenshot baseline
  smoke/
    run_headless.ps1       # 120 frames, exit 0
```

CMake:

```cmake
enable_testing()
add_executable(evo-lab-tests ...)
add_test(NAME unit COMMAND evo-lab-tests)
add_test(NAME smoke COMMAND evo-lab --headless --frames 120 --seed 42)
```

---

## CI matrix (when git/CI available)

| Job | Steps |
|-----|--------|
| **build-win** | cmake, build, `ctest` |
| **build-linux** | same |
| **build-mac** | same (optional initially) |

Skip GPU in CI if headless GL fails on runners → rely on **unit tests + smoke without draw** (`--no-render --frames N` tests sim tick + tide only).

---

## Implementation order (test-driven)

1. Heightmap + unit tests (no GL)  
2. Tide + wetness unit tests  
3. Mesh builder tests  
4. Minimal GL window + **manual** visual  
5. Headless/smoke flags  
6. ImGui + regenerate determinism test  
7. Golden screenshot (optional)  

---

## What we are *not* testing in Phase 0

- Organisms, genetics, Box2D life  
- Lake flood-fill vs ocean (Phase 1)  
- Quantum / GA  
- Multi-platform GPU parity (best-effort only)
