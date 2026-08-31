# evo-lab test tiers

Run from the **build** directory after `cmake --build . --target evo-lab-tests evo-lab`.

Visual binary: **`build/src/evo-lab.exe`** (not the build root).

---

## Decision flow

```
Tier 3 — 6000-tick marathon (+ fuel log) ──pass──► visual QA / ship
        │
        fail
        ▼
Tier 2 — nursery (cornucopia, simplified world)
        │
        fail ──► Tier 1 — basic diagnostics (narrow filter)
        │
        pass ──► Tier 1 tank/metabolism slice, then re-run tier 3
```

**6000 ticks is the primary gate.** `tank_marathon_fuel.log` has per-tick hub deltas, bites,
equilibrium columns (`eqP`/`eqM`/`eqA`/`eqC`), famine, and vents — derive regressions from the log
instead of adding longer runs.

---

## Tier 3 — Full simulation marathon (primary gate)

When tier 3 passes, the visual session is trustworthy for morphology and fuel dynamics.

```powershell
.\tests\evo-lab-tests.exe "[tier3][marathon]"
```

| Test | Ticks | Output |
|------|-------|--------|
| `[viability]` camp morphology retained | 600 | console stats |
| `[tank][marathon]` sunfall + fuel ledger | **6000** | `tank_marathon_fuel.log` |

Optional long runs (not part of default gate):

```powershell
.\tests\evo-lab-tests.exe "[tier3][optional]"
```

Ultra-marathon (18000 ticks) and other slow regressions live here.

**If tier 3 fails:** drop to tier 2, then tier 1 to localize (metabolism → nursery → tank).

---

## Tier 2 — Nursery (simplified world)

Cornucopia / infinite food, frozen locomotion, mutant drift. Validates chemotaxis, crawl–eat–puke,
and parthenogenesis wiring without full rain/tide pressure.

```powershell
.\tests\evo-lab-tests.exe "[nursery]"
.\tests\evo-lab-tests.exe "[camper][cornucopia]"
```

Key files: `test_cornucopia_chemotaxis.cpp`, `test_nursery_mutant.cpp`

---

## Tier 1 — Basic diagnostics (metabolism + unit)

Fast tests for energon, conveyance, neurons, coordinator, computer, equilibrium, chaos, morphology.
Run when tier 3 fails or after sim-layer edits.

```powershell
# CTest target (excludes nursery, marathon, long drift)
ctest -R tier1_diagnostic

# Or direct Catch filter:
.\tests\evo-lab-tests.exe "~[nursery]~[marathon]~[ultra-marathon]~[long]~[drift]~[optional]"
```

Representative filters:

| Area | Filter |
|------|--------|
| Energon / rain / attach | `[energon]` |
| Conveyance / equilibrium | `[energon_conveyance]`, `[stem][equilibrium]` |
| CAMP computer / hub | `[camp][computer]` |
| Coordinator / famine | `[coordinator]` |
| Perceptor / mouth / tumble | `[perceptor]`, `[camp][mouth]`, `[camp][tumble]` |
| Parthenogenesis (unit) | `[parthenogenesis]` |
| Nom morphology | `[nom]` |

---

## Headless app smoke

```powershell
.\src\evo-lab.exe --headless --frames 120 --seed 42 --exit
```

Visual tank (after tier 3 passes):

```powershell
.\src\evo-lab.exe
# expect startup line: "60 CAMP Noms, 0 stem, archetype=Nom"
```

---

## CMake CTest labels

| Target | Command |
|--------|---------|
| `tier3_marathon` | `[tier3][marathon]` |
| `tier2_nursery` | `[nursery]` |
| `tier1_diagnostic` | excludes nursery/marathon/long |
| `smoke_headless` | 120-frame headless run |
