# Evo-Lab

A small **virtual evolutionary laboratory** in a procedural **3D tidal world**. Watch **Noms** — the things that **nom** — swim, drift, and (eventually) evolve in an early-Earth-style shallow sea.

This repo is **public work-in-progress**: cute, experimental, and meant to be fun to run and poke at. APIs and behaviour will change.

## What are Noms?

**Noms** is the umbrella name for everything in the wet layer that exists mainly to **nom**:

| Kind | What it is |
|------|------------|
| **Organisms** | Structured life — mouths, bones, neural links (current default: twin-mouth dumbbells) |
| **Energon** | Byte-string food — sunfall rain, signal trails, fragments |

Energon is still the technical term for the food substrate; **Nom** is the friendly collective when we mean “stuff in the water that eats or gets eaten.”

## Current snapshot (Phase 2.x)

- Procedural heightmap terrain with **global tides** and hydraulic spill/lake rules
- **Water-column bands** — dry / benthic / shallow / pelagic / open deep — so Noms sit on the seabed or ride the free surface instead of hovering over painted slopes
- **Twin-mouth organisms** — two mouths, one bone, bidirectional neural axons, developmental trust, spawn chaos (±3%), axon pruning
- Interactive **SDL + OpenGL** viewer (~60 twin-mouth Noms seeded on wet terrain)
- **Catch2** unit tests + headless smoke test

**Not yet:** mating/genetics, full P/M/C/A chain, Hebbian trust updates, land survival.

## Design documentation

- **[docs/DESIGN-NOTES.md](docs/DESIGN-NOTES.md)** — architecture, Noms, water bands, trust/chaos, roadmap
- **[docs/TESTING-PHASE0.md](docs/TESTING-PHASE0.md)** — barren-world verification; Phase 2.x tests use `[water]`, `[chaos]`, `[twomouth]` filters

## Requirements

- **CMake** 3.20+
- **C++20** compiler
- Windows build tested with **MinGW** (WinLibs LLVM); other platforms may work with minor CMake tweaks

## Build

```powershell
# Windows example (one-time tool install: Kitware.CMake + BrechtSanders.WinLibs.POSIX.UCRT.LLVM)
cmake -B build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build build --target evo-lab evo-lab-tests
ctest --test-dir build --output-on-failure
```

## Run

```powershell
.\build\src\evo-lab.exe
# optional: --seed 42 --resolution 128
```

Window title: **evo-lab — Phase 2.x Twin Mouth**. Hover an organism for architecture/trust readout.

**Controls:** drag = orbit · scroll = zoom · **Space** = pause sim/tide · **R** = regenerate world

## Tests

```powershell
.\build\tests\evo-lab-tests.exe "[water]"
.\build\tests\evo-lab-tests.exe "[chaos]"
.\build\tests\evo-lab-tests.exe "[twomouth]"
```

Headless smoke (no GPU required):

```powershell
.\build\src\evo-lab.exe --headless --frames 120 --seed 42 --exit
```

## Contributing

Issues and PRs welcome. This is an early-stage research toy — prefer small, tested changes. Run `ctest` before opening a PR.

Please do **not** commit secrets, API keys, or machine-specific paths.

## Project layout

| Path | Role |
|------|------|
| `src/sim/` | World, tides, hydrology, Noms, energon, organisms |
| `src/game/` | Terrain mesh, renderer, HUD |
| `src/app/` | `evo-lab` entry point |
| `engine/`, `platform/` | Rendering and OS glue |
| `tests/` | Catch2 unit tests |

## License

No `LICENSE` file in the repo yet — treat the code as **all rights reserved** until a license is added. If you want to fork or redistribute, open an issue or contact the maintainer.

## Stack (planned / partial)

C++ · CMake · SDL2 · OpenGL · Catch2 · (future) Box2D · EnTT · Dear ImGui
