# Evo-Lab

Virtual evolutionary laboratory — procedural **3D tidal world** with genome-based organisms and optional quantum-inspired evolution.

**Current focus (Phase 2.x):** **Twin-mouth organisms** — two Mouth neurons linked by a kinematic bone and bidirectional **neural axons** with developmental **trust** (signal + energon channels), spawn **chaos** (±3% jitter), and activity-dependent **axon pruning**. The visual app seeds ~60 twin-mouth dumbbells on wet terrain. **Phase 0** (barren land/water/tides) remains the world foundation.

**Native C++ desktop binary.**

## Design documentation

See **[docs/DESIGN-NOTES.md](docs/DESIGN-NOTES.md)** for architecture, twin-mouth topology, trust model, chaos module, and roadmap.

See **[docs/TESTING-PHASE0.md](docs/TESTING-PHASE0.md)** for how Phase 0 (barren world) is tested and verified.

## Build

Requires **CMake** and a C++20 compiler. On Windows (MinGW via winget):

```powershell
# One-time: Kitware.CMake + BrechtSanders.WinLibs.POSIX.UCRT.LLVM
cmake -B build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build build --target evo-lab evo-lab-tests
ctest --test-dir build --output-on-failure
```

**Focused test filters (Phase 2.x):**

```powershell
.\build\tests\evo-lab-tests.exe "[chaos]"
.\build\tests\evo-lab-tests.exe "[twomouth]"
```

## Run (interactive 3D viewer)

```powershell
.\build\src\evo-lab.exe
# optional: --seed 42 --resolution 128
```

Window title: **evo-lab — Phase 2.x Twin Mouth**. Each organism is a dumbbell (two mouths, one bone, neural links). Inspector shows axon trust as percent (e.g. `feed:101% believe:98%`).

**Controls:** drag = orbit, scroll = zoom, Space = pause tide, R = regenerate world.

## Smoke test (headless, no GPU)

```powershell
.\build\src\evo-lab.exe --headless --frames 120 --seed 42 --exit
# or
.\tests\smoke\run_headless.ps1
```

Expected output includes: `smoke ok: seed=42 ...`

## Planned stack

- C++ / CMake / vcpkg
- SDL2 + Box2D + (optional) EnTT + Dear ImGui
- Engine + platform submodules; this repo as the game/sim layer
