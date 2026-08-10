# Evo-Lab — Design Notes

Consolidated review of architecture, simulation, evolution, engine, and quantum-integration discussions.

**Status:** Phase 2.x Twin Mouth + water-column bands — public WIP  
**Last updated:** 2026-08-10 (Noms naming, `WaterColumn`, shallow-water placement)

**Delivery:** Native C++ binary (SDL desktop). Web viewer deferred.

**Audience:** This document and the repo are **public**. Prefer stable terminology in docs; mark experimental APIs and behaviour explicitly.

**Research posture:** The project author is **always open to interesting applications of quantum techniques** where the encoding is honest and the classical tank remains the arbiter of long-horizon survival. Propose new integration points freely; document oracle + register design before implementation.

---

## 1. Vision

A **virtual evolutionary laboratory**: simple organisms in a **tidal shallow-world** (early-Earth analogue)—top-down land and sea, rising/falling water, emergent geographic isolation—and observe metabolism, reproduction, and long-term adaptation.

**Long-term goal:** integrate quantum computing where peer-reviewed literature supports it—primarily at **genotype boundaries** (seed generation, mating collapse, mutation search)—**not** as a replacement for the classical tidal tank.

**Quantum philosophy (2026-07):** *Quantum diversifies who gets a ticket into reality; the tide decides who keeps it.* Use quantum or quantum-inspired methods to **diversify the seeding generation** with clear **operational floors** (viability predicates). Do **not** attempt a superpositioned simulation of the full tank (tide, energon field, every cell state)—that register is too large. The tank is the **long oracle** and the **stage for one collapsed timeline**; the qubit register lives over **twin-string topology** and discrete mating outcomes.

**Philosophy:** **Self-determining pathways**—local rules (survive, mate, recognize offspring) over global fitness oracles. **Chaos reigns supreme**—small ε everywhere so no lineage locks shut. **No front-loaded trophic tiers**—scavenging, signaling, and specialization should emerge from local rules, not authored food chains.

---

## 2. Organism Model

### 2.1 Neuron types (exactly four)

There are **only four** neuron types. Do not conflate **Mouth** with a separate “Consumer” — ingestion and processing are split across **M** and **C (Computer)**.

| Char | Type | Role |
|------|------|------|
| **P** | Perceptor | **Focus/attention.** Scans the environment within a focus region (direction + width). Detects **Energon** blobs (sunfall strings, signal trails). Costs energy to perceive. **Tide sense (planned):** expose **Δwater level** (and optionally local flow vector) as a sensory channel — organisms track *rate of change*, not only instantaneous wetness; closer to biological proprioception of environmental dynamics. |
| **M** | Mouth | **Ingestion.** On overlap with Energon, bites off `mouth_width` bytes (phenotype from genome) and passes raw bytes down-chain. |
| **C** | Computer | **Digestion, storage, signaling.** Primary **mitochondrial store** (largest internal energy reservoir). Absorbs bytes from M; converts to storable energy; 8-register pattern match; when **full/satiated**, may **expel** excess as a short **signal blob** (v1: fixed blue byte-string). Landauer erase costs deferred to v2. |
| **A** | Actuator | **Movement** (direction/speed on XZ). Draws movement cost from chain energy (ultimately C’s store). Can receive axonal energy refunds under starvation (v2). |

**Alphabet:** `{P, M, C, A}` only. Example genotypes: `PMCA`, `PPMC`, `PMMMCA`.

**Axons:** Dual channel—**energy** and **information**, directed, with transmission loss (η < 1), rate limits, and type constraints. Avoid energy-laundering loops.

**Implementation (Phase 2.x):** Skeleton **bones** (`SkeletonLink`) are kinematic only. **Neural axons** (`NeuralAxon`) are a separate directed graph for byte signals + store sharing. Trust, η, and pruning live on the axon; **emit timing lives on the neuron** (Mouth bites / heartbeat).

### 2.2 Dual genotype

Evolution targets two coupled layers:

```
G = ( sequence , graph )
      PMCA        edges + weights (signal, energy)
```

- **Sequence:** ordered neuron types (developmental read order).
- **Graph:** adjacency + axon weights between instantiated nodes.

**Twin-string encoding (implementation view):**

```text
G_seq  = [ P M C A … ]     ← developmental chain (neuron types in read order)
G_axon = [ A1, A2, A3 … ]  ← outbound axon targets (instantiated node ids / indices)
G      = ( G_seq , G_axon )
```

This is the **right-sized register** for Grover, QIEA, and parent-biased collapse: discrete, enumerable, parseable—**not** the full tidal world state.

**v1 simplification:** fixed chain wiring `i → i+1` while sequence evolves; axon string may be empty (StemCell stage) or stubbed.  
**v2:** free graph; co-evolve edges (QIEA qubits per edge on/off).

### 2.3 Development parser (critical)

String `PMCA` must map to runnable phenotype. Recommended v1:

1. Read left → right; spawn one neuron per character.
2. Wire `neuron[i] → neuron[i+1]` (signal + energy channels).
3. Attach P to world sensors (focus); M to ingest; A to locomotion; C to storage/expel.

**Viability predicate (minimum):** contains ≥1 P, ≥1 M, ≥1 C; length within bounds; development succeeds; axon targets reference valid instantiated nodes (when axons active). Inviable → stillbirth (partial energy return to parents optional).

These rules are **operational floors**—candidates must pass them *before* entering the tank (seed batch) or *after* mating collapse (offspring). Ecology (tide, energon, starvation, stranding) is the **long oracle** applied only to collapsed individuals.

### 2.4 Per-tick behavior (minimal loop)

```text
P  → scan focus cone for Energon (pay cost)
A  → move (pay cost; bias toward strongest percept if signal present)
M  → if overlapping Energon, bite min(mouth_width, blob.remaining)
C  → digest bytes → energy; update registers; if satiated → expel signal blob
Mate → if wet, proximate, energy thresholds met (see §3.5)
```

No separate Consumer neuron. **M eats; C processes.**

### 2.5 Twin-mouth prototype (Phase 2.x — shipped)

Simplest “complex” organism for axon/trust experiments:

```text
  [M1] ===== bone (kinematic, energyEta=0) ===== [M2]
    |                                             |
    +--- neural axon M1→M2 (signal + energon) ---+
    +--- neural axon M2→M1 ----------------------+
```

- **Factory:** `makeTwoMouthOrganism()` — dumbbell topology; body storage on organism; per-mouth local `store`.
- **Tick order (organism):** advect root → metabolise → feed (each M) → **mouth signal fire** → prune axons at 0% trust → colony transfer.
- **Bite:** Personal ingest only — byte + `kBiteCost` tax into mouth store (or **M-cloaca** spit if store full). **No axon fire on bite.**
- **Mouth signal fire (`Organism::signal`):** receive → forward → spit. Under store pressure (≥24 B), heartbeat (every 30 ticks), or hard full: emit **signal tag** (`0xF0`, believe channel) then **feed burst** (`floor(64 × trustFeed/256 × η_energy)` bytes). Received overflow at dst → **M-cloaca**.
- **Energy (legacy skeleton):** star-mouth rim→hub via bone `energyEta`. Neural feed uses signal-fire burst, not separate drip.

#### Neural axon trust (developmental model)

Inspired by exuberant early connectivity + activity-dependent refinement (see peer discussion 2026-08). Trust is **not** inhibition—it is **synaptic efficacy** on two channels:

| Field | Role | Baseline |
|-------|------|----------|
| `trustBelieve` | Weight on incoming **signal byte** (future Hebbian / per-byte table) | 100% |
| `trustFeed` | Gate on **energon** shared along this directed edge | 100% |

**Fixed-point:** `256 = 100%` (`kTrustBaseline`). Modifiable range **85–426** (~33%–166%). Values below ~33% feed trust do not share energon.

**Pruning:** When **both** `trustBelieve` and `trustFeed` reach **0**, the axon is removed structurally (`pruneNeuralAxons`)—“kill me”, not “shh”. Partial zero (one channel only) leaves a degraded but living edge (signal-only or feed-only).

**E/I note:** Excitation vs inhibition is **not** modeled as negative trust; future inhibitory channels should be separate axon classes or signed signal interpretation—not a scalar below zero.

#### Spawn chaos on axons

At spawn, each axon gets developmental baseline **100% ± 3% jitter** on `trustBelieve`, `trustFeed`, `η_signal`, and `η_energy`. Emit decisions remain on the Mouth neuron.

## 3. World & Simulation Loop

### 3.1 Tidal shallow-world (primary direction)

**View:** **3D terrain** with pan/orbit/zoom camera (default: oblique top-down). Macro ↔ micro same as before, but height and depth are visible (islands, lake basins, shelves).

**Terrain data:** 2D heightmap `h(x, z)` on a regular grid—procedurally generated at startup (seeded). Each sample stores **elevation**; water **depth** at a point = `max(0, water_level − h)`.

| Feature | How it emerges |
|---------|----------------|
| **Ocean / sea** | `h < water_level` (open to tide) |
| **Islands** | `h > water_level` at mean tide; may become peninsulas at high tide |
| **Lakes** | Local basins with `h` below **surrounding** ridges but can be above or below global tide—enclosed wet when tide + fill rules say so (v1: basin floor below `water_level` and enclosed depression) |
| **Land** | `h ≥ water_level` (exposed) or shallow submerged shelves |

**Map generation (Phase 0+):** standard procedural stack—no hand-authored maps required.

- **Base:** Simplex/Perlin **FBM** (fractional Brownian motion) height noise  
- **Optional v1 tweaks:** ridged noise for ridges, exponentiation for sharper coasts, simple flood-fill to tag ocean vs enclosed lake basins  
- **Seed:** `uint64_t world_seed` — reproducible barren worlds  
- **Defer:** hydraulic erosion, tectonic plates, biomes

**Tides:** Global `water_level(t)` (sine or stepped cycle). A surface point is **wet** iff submerged: `h(x,z) < water_level(t)` (lake enclosure rules refined when organisms arrive).

#### Water-column bands (shipped 2026-08)

Noms do **not** use a deforming water mesh. Placement is a cheap **category model** over depth — one sample per entity per tick, no extra grids.

| Band | Depth (sim units) | Meaning |
|------|-------------------|---------|
| **Dry** | 0 | Land / exposed shelf |
| **Benthic** | ≤ 2 | Bed-dominated shallow wet |
| **Shallow** | ≤ 6 | Under-surface shelf |
| **Pelagic** | ≤ 18 | Mid column |
| **OpenDeep** | > 18 | Deep open water (free surface still at top) |

**Module:** `src/sim/WaterColumn.hpp` — `sampleWaterColumn`, `classifyWaterBand`, `placementY(column, NomHabitat)`.

| `NomHabitat` | Placement when wet |
|--------------|-------------------|
| **Surface** (default swimmers, wet energon) | Free surface + clearance — **rides the tide** |
| **Benthic** | Seabed + clearance |
| **Shallow** / **Pelagic** | Lerp between bed and surface (for future morphologies) |

Constants: `kWaterBandBenthicMaxDepth`, `kWaterBandShallowMaxDepth`, `kWaterBandPelagicMaxDepth` in `WaterColumn.hpp`.

**Why categories:** Correct enough for a tidal evo lab, discrete enough for future **P** senses (“which band am I in?”), cheap enough to run forever. Terrain wetness is still **vertex colour** on fixed geometry; Noms use the band model for vertical truth.

| Tide state | Effect |
|------------|--------|
| **High** | Seas connect; low islands shrink; lake shores expand |
| **Low** | Land bridges, exposed shelves; refugia in pools and lakes |
| **Transition** | Emergent isolation / migration corridors |

**Organisms (post–Phase 0):** Aquatic **Noms** — swim where wet. Stranded on land when exposed: stress/dormancy/death (tune). Kinematic FK on XZ with **water-column placement** for Y. **No land survival** until transitional morphologies are designed.

**Energon (post–Phase 0):** The unified **information–energy** substrate (technical name). Sunfall strings and fragments are **Noms** too — they nom nothing, but they get nommed. See §3.5.

**Defer:** multi-basin independent water levels; Navier–Stokes; arbitrary file drops.

### 3.5 Energon — information–energy substrate

**Energon** is any byte-string entity in the world: sunlight rain, uneaten fragments, or Computer expulsions. One primitive, multiple origins.

#### Origins

| Source | When | Typical size | Notes |
|--------|------|--------------|-------|
| **Sunfall** | Day phase of cycle | Random 1–8 bytes | Spawns at sky; falls on land and sea; **active only in wet cells** (or rots on land) |
| **Expulsion** | C satiated | Short 1–3 byte blob | v1: fixed **blue** signal trail; attracts P focus → A approach → mating proximity |
| **Decay** | TTL exhausted | — | Uneaten Energon **dissolves** (entropy); prevents memory blow-up |

#### Day/night

```text
day_phase ∈ [0, 1)
sunfall_rate ∝ max(0, sin(day_phase))   // rain during day, dark at night
```

Strings may land half on shore, half in water—fine. Only wet Energon is food for aquatic life.

#### Entity model (sim)

```cpp
struct EnergonBlob {
  uint64_t data;           // up to 8 bytes per bite; carcass-like blobs hold more total
  uint16_t remaining;      // bytes left in this blob
  uint8_t  origin;         // Sunfall | Signal | Fragment
  float x, y, z;
  float ttl;               // decay timer — mandatory
};
```

**Entropy rule:** every blob has TTL; if not eaten, it decays. No infinite accumulation.

#### Ingestion (emergent granularity)

- `mouth_width` from phenotype (e.g. count of M in sequence, capped 1–8).
- Each tick: bite `min(mouth_width, blob.remaining)`.
- Simple organisms (1×M) nibble single bytes off long sunfall strings; complex (multiple M) take larger bites **without** authoring “top/bottom feeder” roles.

#### Signaling (emergent, not front-loaded)

When **C’s energy store ≥ satiation threshold**:

1. C expels a **small blue Energon blob** (1–3 bytes, low total energy).
2. Nearby organisms’ **P** detects it as weak food/signal within focus.
3. **A** biases movement toward it → **increased mating proximity** for well-fed individuals.

This is intentionally **bee-dance-simple** in v1 (fixed blue pattern). Custom signaling languages deferred—registers and expulsion rules can evolve later.

**Not in v1:** authored waste tiers, “complex must leak for simple to survive,” or forced scavenger niches.

#### Visual language (render)

| Element | Look |
|---------|------|
| **Sunfall strings** | Bright warm tones (white/yellow); **length ∝ byte count**; thin vertical streaks falling from sky; larger blobs read as “fat” strings |
| **Signal expulsions** | **Blue** short trails, smaller, fade fast |
| **Focus (P)** | Debug overlay: cone/wedge from organism showing scan direction (optional toggle) |
| **Landfall** | Strings visible on terrain but grey/dim; dissolve quickly if dry |
| **Wet Energon** | Full saturation; slight emissive glow underwater |

Camera should allow seeing rain at oblique orbit; zoom in to watch organisms swarm a large string.

### 3.1.1 Phase 0 Earth — startup experience

**Goal:** Launch app → immediate **barren Hadean-style world**—land, water, tides only. No organisms, no food, no GA.

```
Startup → generate heightmap(seed) → mesh terrain → animate water_level
        → user sees coastlines, islands, lakes, depth; camera moves; tide runs
```

**Phase 0 acceptance:**

- [ ] Procedural island/continent-like landmass from noise  
- [ ] Visible **depth** (shallow shelf vs deep ocean; lake basins)  
- [ ] Tide cycle changes wet/dry coastlines in real time  
- [ ] 3D camera (orbit + zoom; optional snap to top-down)  
- [ ] ImGui: seed, tide period, pause tide, regenerate world  
- [ ] Fixed timestep loop + letterbox render  

This *is* Phase 0—not a blank window. Later phases add life on top of this world.

### 3.2 Wetness & terrain API

```cpp
float heightAt(float x, float z);           // terrain elevation
float depthAt(float x, float z);            // max(0, water_level - height)
bool  isWet(float x, float z);              // height < water_level (v1 open water)
bool  isLake(float x, float z);             // wet + enclosed basin (flood-fill tag)
void  generateWorld(uint64_t seed);         // procedural heightmap + mesh
```

Geography is **where you can swim** once life exists—derived from 3D data, not painted regions.

### 3.3 Tick order (discrete, fixed timestep)

**Target (full P/M/C/A chain):**

1. Update `water_level(t)` and **day/night phase**
2. Refresh wetness / stranding checks
3. **Spawn sunfall Energon** (day rate); apply gravity/fall
4. Physics step (Box2D) — organisms + blob drift
5. **Energon decay** (TTL); despawn expired
6. Perceptor scans within **focus** (paid energy)
7. Neural propagate (chain order)
8. Mouth ingest overlaps → Computer digest / store / **expel if satiated**
9. Actuator movement cost (+ land penalty if stranded)
10. Death
11. Reproduction (local mate preference when wet paths allow)
12. Render (terrain, water, Energon strings, organisms, focus debug)

**Current implementation (Phase 2.x twin-mouth):**

1. World tick + day cycle + energon tick (sunfall with chaos jitter on spawn params)
2. Per organism: advect root (heading, tide/food sense, FK) → metabolise → feed (each M) → purge depleted blobs → neural energy transfer → signal → **prune neural axons** → colony transfer → remove dead
3. Render terrain, water, energon, organisms (bone + one neural line per dumbbell)

### 3.4 Geographic isolation (tidal)

Isolation is **dynamic**: low tide → refugia; high tide → gene flow. Mate **locally** by default (proximity in same wet component). **ε migration** and tide cycles prevent permanent split or merge. Optional later: persistent tidal pools (low basins that stay wet).

---

## 4. Evolution & Genetics

### 4.1 Classical GA (baseline)

- **Genotype:** string over `{P, M, C, A}` (+ optional ∅ junk for frameshift tolerance).
- **Crossover:** 1-point (or aligned) on parent strings; graph crossover by locus alignment when graph unlocked.
- **Mutation:** duplication, insertion, deletion; ~3% **misalignment** (crossover/splice shift); point type swaps (rare).
- **Example:** P1=`PMCA`, P2=`PMMCCA` → child `PMMCCA`; deletion → `PMCA` still viable if one M remains.

### 4.2 Quantum-inspired layer (QIEA — classical CPU)

Han & Kim style: qubit amplitudes per locus (and per edge in v2); **collapse → simulate → rotate** toward high-outcome samples.

- Superposition explores **possible** genotypes/children; does **not** simulate all exponentially many outcomes.
- **No true tank fitness at collapse time** without running the sim (or a trained surrogate).

### 4.3 Mating & offspring — self-determining + parent pattern

**Implicit parent fitness:** only organisms that **survived and mated** reproduce—no external fitness function required at mating.

**Child selection (default):**

```
amplitude(child) ∝ √(sim(child, P1)) · √(sim(child, P2)) · valid(child)
```

Recognition metrics: edit distance on sequence, graph overlap, register similarity. Collapse → viable child → spawn.

**Chaos clause (ε-greedy):**

```
if random() < ε_random_child:   # suggest 0.03
    child = randomViableGenome()
else:
    child = parentBiasedCollapse(P1, P2)
```

Similarly `ε_random_mate` (~0.05) for partner choice. ε never zero.

**Optional short trial** `T_eval` for sibling comparison at mating; **lifetime survival/offspring** remains the long-horizon filter. Parent fitness does not transfer—only biases sampling.

### 4.4 Chaos & diversity knobs

**Philosophy:** *Chaos reigns supreme* — small ε at **every initialization and boundary** so no lineage locks shut. Macro-randomness (placement, day bucket, byte bias) provides large dice; **±3% multiplicative jitter** (`kChaosJitterRate = 0.03`) is the thin coat on all baselines.

**Single module:** `src/sim/Chaos.hpp` + `Chaos.cpp`. All spawn jitter and future ε-greedy decisions should call here—not ad-hoc `uniform_*` on baselines.

| Parameter | Role | Suggestion | Code (`Chaos.hpp`) | Status |
|-----------|------|------------|-------------------|--------|
| `kChaosJitterRate` | ±3% multiplicative jitter on baselines | 0.03 | `chaosJitterFloat`, `chaosJitterTrust`, `chaosJitterHeading` | **Shipped** — organisms + energon |
| `ε_random_child` | Random viable genome instead of parent-biased child | 0.03 | `kEpsilonRandomChild`, `chaosBernoulli` | Constant only (Phase 3 mating) |
| `ε_random_mate` | Random partner | 0.05 | `kEpsilonRandomMate` | Constant only |
| `migration_rate` | Random jump when wet path exists / chaos override | 0.01 | `kMigrationRate` | Constant only |
| `misalignment_rate` | Crossover/splice error | 0.03 | `kMisalignmentRate` | Constant only |
| `macromutation_rate` | Random locus type swap | 0.001 | `kMacromutationRate` | Constant only |
| Parent similarity β | Sharpness of recognition bias | tune | — | Planned |

**Edge of chaos:** too ordered → monoculture; too chaotic → no inheritance. Geography splits lineages; ε and migration prevent permanent lock-in.

### 4.4.1 Chaos implementation (shipped 2026-08)

**Helpers (all in `evolab::`):**

| API | Purpose |
|-----|---------|
| `chaosSpawnRng(worldSeed, salt)` | Deterministic RNG per spawn class (stem / star-mouth / twin-mouth / energon) |
| `chaosJitterFloat(baseline, rng)` | ±3% on floats (bone length, η, sky height, fall speed, …) |
| `chaosJitterTrust(baseline, rng)` | ±3% on trust, clamped to `[kTrustMin, kTrustMax]` |
| `chaosJitterHeading(heading, rng)` | ±3% of one revolution (additive micro-jitter) |
| `chaosSpawnHeading(rng)` | Macro-random heading ∈ [0, 2π) |
| `chaosInitialStorage(rng)` | 1–3 fuel-days + jitter, clamped to valid storage band |
| `nominalBoneLength(cellSize)` | Baseline bone length **before** jitter |
| `chaosBernoulli(rate, rng)` | ε-greedy gate (future mating / migration) |

**Organism spawn (one ritual):**

```text
build factory (nominal parameters)
  → organism.finalizeSpawn(rng)   // axon trust, skeleton links, heading — each baseline jittered once
  → optional post-step (kinematics, land-adjacent)
```

Do **not** jitter the same parameter twice (e.g. bone `restLength` pre- and post-factory). Regression test: `bone length receives a single chaos jitter at spawn`.

**Population spawn:** `CellPopulation::seedOnWetTerrain(...)` — shared wet placement, surface Y jitter (`kSpawnSurfaceYOffset`), `finalizeSpawn`, archetype-specific factory callback. Used by stem-cell, star-mouth, and twin-mouth seed paths.

**Where ±3% jitter applies today:**

| Layer | Jittered parameters |
|-------|---------------------|
| **Neural axon** | `trustBelieve`, `trustFeed`, `η_signal`, `η_energy` |
| **Skeleton link** | `restLength`, `jointAngle`, `energyEta` |
| **Organism** | `heading` (micro, after macro random) |
| **Spawn site** | surface Y offset above terrain/water |
| **Initial storage** | 1–3 day byte budget |
| **Energon sunfall** | byte count, sky Y, fall speed, grounded segment heading |

**Still macro-random (by design):** wet XZ placement, energon byte-count bias distribution, world seed. Large dice first; jitter second.

**Tests:** `tests/unit/test_chaos.cpp`, `[twomouth]` spawn trust cases.

### 4.5 Fitness (when explicitly needed)

Composite for experiments/meta-optimization only:

```
F = w1·food_eaten + w2·survival_ticks + w3·offspring_bonus − w4·metabolic_cost
```

Default self-determining mode: **who lives and breeds is the fitness signal.**

### 4.6 Seed generation — diversified basics (planned)

**Goal:** First generation (and optional world re-seeds) should span a **covering set** of minimal viable topologies—not clones, not uniform random junk.

**Pipeline:**

```text
Superposition / search over twin-strings (G_seq , G_axon)
        ↓  oracle: valid(G) ∧ covers_next_basic_class
   Seed population (K diverse, floor-qualified genotypes)
        ↓
   One classical tidal tank (60 Hz)
        ↓
   Survival, mating, ε-chaos — tide decides who keeps the ticket
```

**Operational floors (static oracle, cheap):**

| Check | Purpose |
|-------|---------|
| `parse(G)` succeeds | Runnable phenotype |
| ≥1 P, ≥1 M, ≥1 C | Perception, ingestion, storage (starvation-without-lesson otherwise) |
| Length / axon caps | Bounded register |
| Legal axon targets | No dangling edges (v2+) |

**Basic-class coverage (diversity oracle):** Grover, QIEA, or classical biased sampling repeated until K seeds span functional classes—e.g. minimal feeder (`…M…`), minimal storer (`…C…`), minimal mover (`…A…`), simple axon motifs. Optional ε-random viable genomes for chaos.

**Multiple tank instances** (different world seeds / islands) remain a **classical** parallel option—orthogonal to quantum seeding; spend compute, not qubit depth.

**Not in scope:** superpositioned simulation of tide + energon + all cells. The visible tank is always **one collapsed branch**.

---

## 5. Quantum Computing (literature-aligned)

### 5.1 Guiding principle

> **Quantum diversifies who gets a ticket into reality; the tide decides who keeps it.**

| Layer | Role | Hardware |
|-------|------|----------|
| **Qubit register** | Superposition over twin-strings, offspring candidates, edge toggles | QPU or classical qubit sim |
| **Static oracle** | `valid(G)`, basic-class coverage, parent recognition | Classical CPU |
| **Tank sim** | Metabolism, tide, death, mating — **long oracle** for collapsed individuals | Classical CPU (always) |
| **Grover / QIEA** | Amplify or rotate toward floor-qualified, diverse candidates **at boundaries** | Hybrid |

The tidal tank is **not** Grover. It is the **reality engine** and **viability oracle over time**. Grover (or QIEA) operates on the **small genotype register** at seed, mate, and mutation events.

### 5.2 What stays classical

Tank physics, rendering, neural ticks, Energon field, tide, main ALife loop (60 Hz), and the **single timeline the player watches**.

Optional: **N parallel tank instances** (different seeds) until compute budget exhausted—classical ensemble, not quantum superposition.

### 5.3 Credible integration points (priority order)

| Pattern | Use in evo-lab | When | Hardware |
|---------|----------------|------|----------|
| **Floor-qualified seed search** | Diversified generation-0 (and re-seeds) over twin-strings | Phase 3–5 | Classical first; QPU if register large |
| **QIEA** | Qubit amplitudes per locus / edge; rotate after sim windows | Phase 5 | Classical CPU |
| **Parent-biased collapse** | Recognition-weighted offspring at mating | Phase 3+ | Classical or QPU |
| **Grover selection** | Amplify viable children or seed candidates when search space ≫ oracle budget | Phase 5–6 | Research / QPU |
| **QAGA** | Annealing as mutation on small QUBO subproblems | Phase 6 | D-Wave / sim |
| **E-QAOA** | Meta-tune experiment params | Optional | Hybrid |

**Key literature insight:** encoding matters more than algorithm hype; hybrid classical–quantum beats pure annealing on many instances; noise favors population + elitism (CVaR on QPU evaluations if used).

**Open to exploration:** New quantum-inspired or hybrid patterns welcome if they respect §5.1 (register + oracle + classical tank). Document before coding.

### 5.4 Recommended quantum roadmap

1. Classical GA + tank v1 (StemCell → P/M/C/A chain)  
2. Twin-string parser + **classical** floor-qualified seed batch (covering basics)  
3. Mating + parent-biased collapse + ε-chaos  
4. QIEA on sequence (classical qubit simulation)  
5. Unlock graph + edge qubits; axon string co-evolution  
6. Grover / QPU at seed or mating **only if** profiling shows candidate space justifies it  
7. QAGA / E-QAOA experiments (optional)

### 5.5 Interfaces (for engine/sim split)

```cpp
struct IGenomeMutator {
    // collapse, rotateToward, randomViable
};

struct IMatingPolicy {
    // parent_bias, epsilon_random, migration_rate
    // selectPartner, selectChild(P1, P2)
};

struct ParentRecognitionOracle {
    // similarity(sequence, graph, registers)
};

struct ISeedPolicy {
    // valid(G), basic_class(G), covers_missing_class(batch)
    // buildSeedBatch(K) — Grover / QIEA / classical
};
```

Tank sim does not call QPU directly; evolution layer does at **generation boundaries** (seed, mate, optional mutation)—never inside the per-tick loop.

---

## 6. Engine & Platform Architecture

### 6.1 Repo strategy (preferred)

**Engine as git submodule**; optional **platform as separate submodule** for maximum decoupling.

```
evo-lab/                 ← game repo (open in Cursor)
├── engine/              ← submodule: ECS, render, physics wrap, Application
├── platform/            ← submodule (optional): SDL, IWindow, IRenderer
├── src/                 ← sim only: genetics, neurons, world rules
└── CMakeLists.txt
```

**Dependency direction:** game → engine → platform → OS/SDK.

### 6.2 Libraries (indie cross-platform)

- **SDL2** — window, input, OpenGL/Vulkan context  
- **OpenGL 3.3+** (or **bgfx**) — 3D terrain mesh, water plane, depth-friendly shading  
- **Box2D** — physics (Phase 2+), XZ plane  
- **EnTT** — ECS (optional, Phase 2+)  
- **Dear ImGui** — lab debug UI (seed, tide controls from Phase 0)  
- **CMake + vcpkg** — build/deps  
- **FastNoise2 / custom Simplex** — procedural heightmap (evaluate in Phase 0)

Target **Windows, macOS, Linux** via SDL platform backend. PS5 = future `platform_ps5` + Sony SDK.

### 6.3 Resolution policy

**Maintain aspect + letterbox/pillarbox** (engine-level, not automatic from SDL):

- Fixed **design resolution** (e.g. 1280×720).  
- Render sim to offscreen buffer at design size.  
- Scale uniformly into drawable; black bars fill remainder.  
- **screenToDesign()** for input mapping.  
- Use **drawable pixels** for HiDPI/Retina.

### 6.4 Layer ownership

| Layer | Owns |
|-------|------|
| **platform** | Window, surface, input, clock, filesystem |
| **engine** | Renderer3D (terrain/water), Camera, Application loop, aspect viewport |
| **evo-lab (game)** | Terrain, tides, wetness, genome/neural chain, Energon rules, evolution |

**Sim source map (Phase 2.x):**

| File | Role |
|------|------|
| `src/sim/WaterColumn.hpp`, `.cpp` | Depth bands, Nom habitat placement, tide-riding Y |
| `src/sim/Chaos.hpp`, `Chaos.cpp` | ε rates, ±3% jitter, spawn RNG, `chaosInitialStorage` |
| `src/sim/NeuralAxon.hpp`, `.cpp` | Axon struct, developmental trust init, pruning predicate |
| `src/sim/Organism.hpp`, `.cpp` | Skeleton + neural graphs, `finalizeSpawn`, tick methods |
| `src/sim/CellPopulation.cpp` | `seedOnWetTerrain`, population tick |
| `src/sim/CellConstants.hpp` | Trust fixed-point (`kTrustBaseline`, min/max), mouth/axon constants |

No `PMCA` genetics in engine. No OS calls in game.

---

## 7. Implementation Phases

### Phase 0 — Barren Earth (land, water, tides) ✓

**Shippable:** CMake/SDL/OpenGL, procedural **3D heightmap**, global tide animation, orbit/zoom camera. **No organisms, no Energon.**

### Phase 1 — Wetness logic + polish

Lake vs ocean tagging (enclosed basins), shoreline refinement, optional terrain LOD, save/load seed.

### Phase 1.5 — Energon rain (abiotic)

Day/night cycle; sunfall byte-strings (1–8 bytes); fall on land/sea; **TTL decay**; visual rain (size ∝ length); wet-only active food. **No organisms yet.**

### Phase 2 — Life shell (in progress)

**2.0 StemCells ✓:** undifferentiated orbs on wet terrain; basal metabolism (1 byte/tick); random 1–3 day initial storage (+ chaos jitter); starvation death; hover inspects architecture; organism count = connected components via colony axons.

**2.1 Tidal advection ✓:** `waterLevelDelta()` drives **drainage-aligned drift** on loose objects (stem cells, energon). **Spill-height hydraulics** govern wetting: each cell precomputes the minimum tide for edge connectivity; impounded basins hold water at the spill rim. **Flow directions** are precomputed once per seed (D8 steepest descent, spill-guided in pits). Ebb follows downhill drainage (with pool convergence blend); flood follows the inverse. **Impounded or disconnected cells do not advect.** No authored dry-land metabolic penalty.

**2.2 Kinematic mouth organisms ✓:** Star-mouth (Computer hub + rim Mouths); organism yaw; food/tide heading; skeleton FK.

**2.x Twin Mouth ✓ (current visual default):** Simplest complex organism — **2 Mouths, 1 bone, 2 directed neural axons**. Separate skeleton vs neural graphs. Developmental axon trust (100% baseline, 33–166% range), spawn chaos module, axon pruning at 0/0 trust. Visual app seeds ~60 twin-mouth Noms.

**2.x Water column ✓:** Band model for Nom vertical placement; surface Noms ride tide; grounded energon re-snaps each tick.

**P neuron (planned):** Perceptor should register **tidal delta** (`waterLevelDelta`, local flow) as a paid sensory input — rate-of-change perception, not static wet/dry alone.

**2.x+ (next):** Hebbian `trustBelieve[256]` per byte; C storage + blue expulsion when satiated; full P/M/C/A chain; mate on proximity.

### Phase 3 — Genetics

Twin-string parser, chain wiring, classical GA, mating, ε-chaos. **Classical floor-qualified seed batch** (covering basics)—prototype for later QIEA/Grover seed policy (§4.6).

### Phase 4 — Lab UI

Population, generation, pause/step, trait overlays on map.

### Phase 5 — QIEA + graph

Qubit genotype, parent recognition; tide-driven geography coupled to GA. Promote seed batch from classical covering search to QIEA/Grover where profiling justifies.

### Phase 6 — Quantum experiments (optional)

Grover at seed/mating boundaries, QAGA mutation, QPU via simulators first. Only where register design is honest (§5.1).

---

## 8. Literature Touchpoints

| Topic | Reference |
|-------|-----------|
| Digital evolution / energy merit | Avida (Ofria & Wilke, 2004) |
| Neural ALife + mating + metabolism | Polyworld (Yaeger) |
| Neuroevolution + topology | NEAT (Stanley & Miikkulainen, 2002) |
| QIEA foundation | Han & Kim (2000, IEEE TEVC 2002) |
| Quantum-assisted GA | King et al., QAGA (2019) |
| EA optimizes QAOA | Acampora et al. (2023); E-QAOA (2025) |
| QGA survey | Lima et al. (2026) |
| Axonal energy biology | Syntaphilin / mitochondrial trafficking reviews |
| Braitenberg / sensorimotor | BraGenBrain, Frontiers 2020 |

---

## 9. Open Decisions

- [ ] **Hebbian trust** — per-byte `trustBelieve[256]` on axons; runtime updates in 33–166% band; who triggers learning (neuron vs axon)?
- [ ] **Seed policy** — hand-curated basis set vs Grover/QIEA covering search for generation 0?
- [ ] **Basic-class taxonomy** — functional (M/C/A presence) vs structural (length) vs ecological (pelagic vs strand)?
- [ ] **Chain vs FC wiring** when leaving v1 chain model.
- [ ] **Platform submodule** separate from engine repo, or `engine/platform/sdl` initially?
- [ ] **Asexual spawn** for very first milestone vs sexual GA from day one?
- [ ] **Computer registers** in genome as 8-byte block or evolved separately?
- [ ] **Expulsion encoding** — fixed blue 1-byte in v1 vs register-derived signal color (v2)?
- [ ] **Surrogate fitness model** after enough sim data (optional acceleration)?
- [ ] **Land stranding** — death vs dormancy vs slow crawl to nearest pool?
- [ ] **Terrain generation** — FBM Simplex only vs ridged + erosion pass?
- [ ] **Lake rule** — enclosed basin fill independent of sea level, or strict global tide only?
- [ ] **Camera default** — oblique orbit vs true top-down ortho?
- [ ] **Tide model** — single global level vs per-basin (v2)?

---

## 10. Summary Principles

1. **Classical tank is truth** — fitness emerges over time in the world; one collapsed timeline on screen.  
2. **Dual genotype** — twin-string `(G_seq , G_axon)` = sequence + graph, co-evolved.  
3. **Quantum at boundaries** — diversify seeds and mating collapse over the genotype register; **never** superposition the full tank.  
4. **Operational floors** — `valid(G)` before spawn; tide is the long oracle.  
5. **Parents pattern offspring** — recognition at mating; ε injects random kids.  
6. **Geography + chaos** — isolation for diversity; ε/migration/misalignment against stagnation; **±3% jitter on every spawn baseline** via `Chaos.hpp`.  
7. **Engine reusable** — platform root, game leaf; letterbox once in engine.  
8. **Phase 0 = barren 3D Earth** — procedural land/water/tides visible at startup; life comes later.  
9. **Native binary** — SDL + OpenGL desktop viewer; sim authority in one process.  
10. **Open to quantum ideas** — interesting techniques welcome if encoding + oracle design are documented first.

---

## 11. Glossary

| Term | Meaning |
|------|---------|
| **Nom** | Collective name for things in the wet layer that exist to **nom** (organisms + energon food) |
| **StemCell** | Undifferentiated life unit (Phase 2.0); base for future P/M/C/A differentiation |
| **Twin-string** | `(G_seq , G_axon)` genotype encoding—neuron chain + outbound axon targets |
| **Operational floor** | Static viability predicate (`valid(G)`) before spawn or after mating collapse |
| **Basic-class coverage** | Seed batch spans minimal functional topologies (feeder, storer, mover, …) |
| **Ticket into reality** | Collapse from genotype superposition → one individual enters the classical tank |
| **Long oracle** | Tank sim over time (metabolism, tide, death)—ecological selection |
| **Short oracle** | Cheap `valid(G)`, recognition, optional `T_eval` trial |
| **QIEA** | Quantum-inspired evolutionary algorithm (qubit individuals, classical implementation) |
| **QAGA** | Quantum-assisted GA (e.g. annealer as mutation operator) |
| **ε-chaos** | Small probability of random mate/child regardless of selection |
| **Recognition** | Similarity of offspring genome to parents ( mating template ) |
| **Stillbirth** | Inviable genotype fails at spawn |
| **Design resolution** | Internal sim/render size before letterbox upscale |
| **Heightmap** | 2D grid of elevation; drives mesh, wetness, depth |
| **Water level** | Global tide height; depth = water_level − height |
| **Phase 0 Earth** | Barren procedural world—no life—default app startup |
| **Wet component** | Connected submerged region—defines local mating pool at low tide |
| **Energon** | Byte-string information–energy in the world (sunfall, signals, fragments) |
| **Sunfall** | Abiotic Energon rain during day phase |
| **Neural axon** | Directed sim edge: byte signal + energon transfer; `trustBelieve` / `trustFeed` (256 = 100%) |
| **Skeleton link** | Kinematic bone between skeleton nodes; optional legacy energy η (0 for twin-mouth) |
| **finalizeSpawn** | Single organism hook: developmental axon trust + ±3% jitter on skeleton/heading |
| **Chaos module** | `Chaos.hpp` — all ε rates, jitter helpers, spawn RNG salts |
| **Water column** | Depth band + habitat placement at a world XZ sample (`WaterColumn.hpp`) |
| **NomHabitat** | Preferred vertical zone: Surface, Benthic, Shallow, Pelagic |
| **Axon pruning** | Structural removal when both trust channels hit 0 (not inhibition) |
| **Focus** | Perceptor scan region (direction + width) |
