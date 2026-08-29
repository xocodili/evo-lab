# Evo-Lab — Design Notes

Consolidated review of architecture, simulation, evolution, engine, and quantum-integration discussions.

**Status:** Phase 2.x Actuator camper + water-column bands — public WIP  
**Last updated:** 2026-08-27 (CAMP camper + metabolic break-even + M FSA)

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

**Kinematics engine:** reusable FK in `engine/kinematics/` — see **[KINEMATICS.md](KINEMATICS.md)** for rollout phases (skeleton resource, local pose, constraints, future IK).

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

**Viability predicate (minimum):** contains ≥1 P, ≥1 M, ≥1 C, **≥1 A**; length within bounds; development succeeds; axon targets reference valid instantiated nodes (when axons active). Inviable → stillbirth. **Grover/QIEA floor:** same check — ensures no collapsed child lacks a module type (EVOLUTION.md §4.4, §10.5). Duplicated loci (`[CCAMP]`) pass; `[AMP]` fails.

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

### 2.5 CAMP neural signaling (Phase 2.x — shipped)

Universal **0–7 confidence byte** on the believe channel (`NeuronSignal.hpp`). Semantics depend on source neuron:

| Source | Encoding |
|--------|----------|
| **M** | Mouth fuel satiation (0 = starving … 7 = full local store) |
| **P** | World-focus valence (0 = avoid … 7 = approach) |
| **C** | Hub satiation (0 = starving … 7 = full `bodyStorage`) |
| **A** | Flagellar activity (stroke paid this tick) |

Mouth **pre-advect emit** (`emitPreAdvectSignals`) writes fuel confidence on M→P, M→C, and M→A axons. **C** emits hub satiation on C→P and C→A during the same phase. Bite credits **Fresh** bytes to M’s local store; **digest** moves mouth surplus into the hub; **conveyance** routes wallet/hub surplus after spend. The late **signal** phase skips duplicate CAMP mouth emit.

**Tick order (CAMP camper):** `perceive → feed → digest+computer → preAdvect (M+C emit) → advect → metabolise → viability → convey → signal → prune`

| Phase | Who emits | Who reads (same tick unless noted) |
|-------|-----------|-----------------------------------|
| perceive | P → P→M, P→C, P→A | — |
| feed | — | M ← P→M (same tick); M ← A→M (**prior tick**) |
| digest+computer | C expels blue signal at hub satiation | C pattern-match inbound P/M/A bytes vs register |
| preAdvect | M → M→P/C/A; C → C→P/A | — |
| advect | A → A→P, A→M (end) | A ← P→A, M→A, C→A (same tick) |

**Actuator interoception (chemotaxis v1):** A gathers P→A (approach/flee), M→A and **C→A** (satiation), trust-weighted via `OrganismNeuron`. Motor intent = `max(baseline crawl, approach) × (1 − satiation)` with **hub brake** from C→A. Graded stroke pays 0–`kActuatorStrokeCostPerTick` bytes from the A wallet.

**Mouth interoception (feed v1):** Go/NoGo appetite from hunger × P approach (or horror baseline); chew satiation subtracts drive; threat/flee hard-stops bite. **Chew FSA:** hysteresis pause at **5/7**, resume at **3/7**; when paused, food-locked, and P approach ≤0.15, **REFUSE** sets `allowFoodBite = false` (strong food approach overrides; horror feedbag without food lock still bites).

**Perceptor temporal gradient (shipped):** After each paid scan, P stores best food Go/NoGo score; next scan nudges outbound confidence by `Δscore × kPerceptTemporalGradientGain` (Berg-style run bias when the food channel improves, tumble bias when it fades). Jitter-free score used for memory; winner selection still uses chaos jitter.

**Mouth satiation encoding:** `mouthChewFill` (0–32 B from field bites only) → `mouthFuelConfidence` (0–7). Conveyance fuel in M wallet does not raise chew satiation. Spawn: empty chew buffer; reserve fuel lives in hub.

**Spawn (gen-0 camper):** Mouth wallet and chew buffer empty; hub carries starter fuel so basal can run without false “full mouth” brakes.

**Shared neuron layer:** `OrganismNeuron.hpp` — inbound axon fusion, P valence decode, outbound confidence emit. Stem basal metabolism remains in `OrganismDetail`.

**M as cyclic FSA (design intent):** See **§2.9** — M is a tick-rate consumer that runs until interoceptive intercept (satiation / threat / flee) modulates or halts bites; not a fixed byte budget. Target analogue: stomach distension → hunger relaxes — **felt correct**, not mathematically exact.

#### Neural axon trust (three-factor believe + developmental feed)

| Field | Role | Baseline |
|-------|------|----------|
| `trustBelieveByConfidence[8]` | Weight on incoming **confidence byte 0–7** | 100% per bin |
| `trustFeed` | Gate on **energon** shared along this directed edge | 33% at spawn (CAMP dev axons) |

**Runtime learning (`NeuronTrust.cpp`):** Post-perceive on M→P / A→P; post-feed on P→M / A→M; post-advect on P→A / M→A; post-transfer on feed-channel `trustFeed`. **`trustFeed` runtime plasticity** via `conveyCampEnergon` + `nudgeTrustFeed`.

**Deferred:** ~~**C inbound trust**~~ — post-digest RPE believe learning on P→C and M→C (`applyCampComputerTrustLearning`); register match trust remains on A→C.

**Pruning:** When **all eight** believe bins **and** `trustFeed` reach **0**, the axon is removed structurally.

At spawn, CAMP developmental axons use believe baseline **100%** and feed baseline **33%** before ε-chaos jitter. `computerRegister[8]` is jittered at spawn (template bytes 0–2, dispatch weights 4–6).

### 2.8 Computer neuron (CAMP — shipped, not evolution-ready)

**Topology:** developmental **P(1) → M(2) → C(3) → A(4)** chain; **12 bounded axons** (all pairs among {P,M,C,A}); skeleton links P→M→C→A only.

**Storage:** P/M/A use small `node.store` wallets (~stem-cell scale); **C hub = `bodyStorage`** (`kComputerHubStoreMaxBytes`). Spawn fuel: **½ hub, ⅙ each peripheral**.

**Register (`computerRegister[8]`):** bytes 0–2 = inbound template (P/M/A last-received signatures); bytes 4–6 = dispatch weights for C→P/M/A; byte 7 reserved.

**Digest:** after feed, mouth surplus (> `kNeuronStoreMaxBytes`) moves M→C hub.

**Computer tick:** pattern-match inbound believe bytes vs register; **P vs M CTA RPE** suppresses dispatch when spatial P and postingestive M valences diverge; post-digest believe trust on P→C and M→C from RPE; set `computerFeedGain`; at hub satiation expel cloaca byte to field; pre-advect emit hub confidence.

**Field boundary:** **M only** for environmental energon ingress; internal routing + η loss on axons.

**Modules:** `OrganismComputer.hpp/cpp`, `EnergonConveyance.hpp/cpp` (`conveyCampEnergon`), factory `makeCampNomOrganism`.

**Not ready to evolve yet** — register/dispatch are developmental chaos at spawn; no inheritance or mutation path. Reasonable analogue of early integrated metabolism + sparse signaling.

### 2.7 CAMP energon conveyance (Phase 2.x — shipped)

Directed **axon feed** with **η loss on payload**, **believe + feed gating**, and **M-only field ingress**. Hub surplus dispatches from **`bodyStorage`** when above `kComputerHubReserveBytes`.

**Modules:** `NeuronFuel.hpp/cpp`, `EnergonConveyance.hpp/cpp` (`conveyCampEnergon`).

#### Substrate

| Compartment | Role |
|-------------|------|
| **Field** | Abiotic energon (sunfall, signal fragments, waste) |
| **`node.store`** | Per-neuron wallet (P, M, A) |
| **`bodyStorage`** | **C hub** — digest target + dispatch source |
| **Neural axon (feed)** | Directed byte pipe; bandwidth ∝ `trustFeed × η_energy` |
| **Neural axon (believe)** | 0–7 confidence bytes; route weight uses outbound source byte × inbound believe trust |

**Convey order:** M → C (hub) → P → A (two passes). C routes use `computerFeedGain × register dispatch weights`.

#### Byte lifecycle (provenance)

| Class | Origin | May leave via |
|-------|--------|----------------|
| **Fresh** | Field bite at M | Digest → hub, then axons |
| **Returned** | Axon hop rejected at wallet | Dissipates at source (entropy) |

Death-release (neuron killed → store spat at node) stays distinct from field policy.

#### Ingress (field → M)

Unchanged bite economics: gross **9 B**, mastication tax **1 B** → net **+8 B** credited as **Fresh** (`kBiteNetYieldBytes`). Mouth surplus above **`kNeuronStoreMaxBytes`** digests to hub or routes on conveyance — no field spam on overflow.

#### Conveyance phase

Called from `Organism::transferEnergy` after metabolism/viability:

```text
perceive → feed → digest+computer → preAdvect → advect → metabolise → viability → convey → signal → prune
```

Each tick, **two passes** over **M → C → P → A**. Hub surplus = `bodyStorage` above `kComputerHubReserveBytes`. Hop applies `η_energy` payload loss; feed-trust nudge on successful delivery.

### 2.6 Actuator camper (Phase 2.x — shipped; mouths shelved in visual app)

**Design intent:** One `[A]` neuron, **no mouth** — a camper that **crawls until it starves**. Movement alone is not success; the organism must eventually **know whether its stroke worked**. Before a full **P** neuron exists, we expose **proprioceptive delta fields** on `Organism`:

| Field | Meaning |
|-------|---------|
| `lastDisplacement` | Actual XZ travel this tick (includes tide advection) |
| `lastIntendedThrust` | Gross thrust at η=1 (0 if stroke skipped) |
| `lastMechanicalThrust` | Directed motion after translation η |
| `lastTranslationEntropyLoss` | Body bytes dissipated as heat this tick |
| `lastTideDelta` | `BarrenWorld::waterLevelDelta()` — pre-P tide sense |
| `lastStrokePaid` | Did the flagellar stroke consume fuel? |
| `lastTumbled` | Run/tumble lite: random reorientation this tick |

**Stroke efficiency (inspector):** `lastMechanicalThrust / lastIntendedThrust` = η_translation; `lastDisplacement / lastIntendedThrust` includes tide/passive drift.

**Energy ledger (aligned with mouth/axon economics):**

| Cost | Bytes/tick | Notes |
|------|------------|-------|
| Basal | 1 | Maintenance (`kStemCellBasalCostPerTick`) |
| Stroke | 2 | Active IMF batch (`kActuatorStrokeCostPerTick`) — same order as food **gross** yield (`kEnergonUnitsPerByte`) |
| Crawl total | 3 | Basal + stroke when moving every tick |

| Channel | η | Loss |
|---------|---|------|
| Mastication (bite) | 0.89 net/gross | 1 B tax on 9 B gross → **8 B net** (`kBiteNetYieldBytes`) |
| Neural axon feed | ~0.88 | ~12% per hop |
| **Translation (stroke)** | **0.12** | **~88% dissipates as viscous heat** (`kActuatorTranslationEta`) |

Mechanical displacement per stroke: `kActuatorStrokeCostPerTick × kActuatorThrustPerStrokeByte × kActuatorTranslationEta` (~0.013 world units/tick). One fuel-day of continuous crawl ≈ **~1100 world units** (~920 cell lengths) — costly vs idle drift, cheap enough to see motion at default zoom.

**Factory:** `makeActuatorOrganism()` — single root node `NeuronType::Actuator`, body storage only.

**Tick (advect):** Pay `kActuatorStrokeCostPerTick` from body → gross thrust intent → apply η_translation → record `lastTranslationEntropyLoss`; optional **tumble**; passive **shore advection**; record deltas. **No food-heading cheat** (mouth-only path).

**Constants:** `kActuatorStrokeCostPerTick`, `kActuatorThrustPerStrokeByte`, `kActuatorTranslationEta`, `kActuatorTumbleRate`, `kActuatorTumbleTurn`.

**Visual:** `seedActuatorOrganisms()` (~60 campers); pale violet orb + heading arrow.

#### Biological model — flagellar motor & run/tumble (literature)

We treat the actuator stroke as a **minimal IMF/PMF analog**, not ATP hydrolysis at the filament:

- **Energy source:** The bacterial flagellar motor is driven by the **ion motive force (IMF)** — proton (PMF) or sodium (SMF) gradient across the inner membrane — not by direct ATP at the rotor ([Frontiers review](https://www.frontiersin.org/journals/microbiology/articles/10.3389/fmicb.2021.659464/full); Mitchell chemiosmotic hypothesis).
- **Cost scales with rotation rate:** Proton flux through stator units sets torque and **rotation frequency**; higher rate ⇒ higher energetic cost ([eLife 77266](https://elifesciences.org/articles/77266) — ~1100–1240 H⁺ per revolution; ~150–380 Hz max in *E. coli*).
- **Sim mapping:** `kActuatorStrokeCostPerTick` = discrete proton batch per stroke (2 B, aligned with food gross yield); `kActuatorThrustPerStrokeByte` = ideal displacement per byte at η=1; **`kActuatorTranslationEta`** = fraction becoming motion (~12% — remainder is translation entropy / heat).
- **Run vs tumble:** CCW bundle rotation → **run**; CW → **tumble** reorientation. Chemotaxis adjusts **CheY-P** (response regulator phosphorylation) to bias motor switching — not a separate “steering muscle” ([Berg, *Random Walks in Biology*](https://book.bionumbers.org/what-is-the-frequency-of-rotary-molecular-motors/)).
- **Sim mapping:** `kActuatorTumbleRate` / `kActuatorTumbleTurn` = stochastic tumble without CheY biochemistry yet; future **P** reads `lastTideDelta` and food cues to modulate tumble bias (chemotaxis-shaped, not hard-wired to nearest blob).

**Success criterion (design-centric):** A stroke is “successful” when `lastDisplacement` reflects paid thrust against tide/passive drift — inspectable now; learnable later when **P→A** closes the loop.

### 2.9 camper visual geometry — triangles over squares

**Design choice (evo-lab specific):** CAMP camper ground presence and heading cues use **triangular** primitives (heading chevron, bone-strip wedges) rather than square billboards alone.

**Rationale:** A triangle is a **more useful shape than a square** for this sim:

| Triangle affordance | Square limitation |
|--------------------|-------------------|
| Single ** apex ** encodes **heading / forward** without ambiguity | Four equal edges — no intrinsic “front” |
| Two base vertices span a **bone / link** naturally (parent–child strip) | Corner-aligned quads read as blocks, not limbs |
| Minimal vertices for **directional affordance** (3 vs 4+) | Billboards are fine for **nodes** (spheres-as-quads); poor for **flow** |

**Implementation:** `OrganismDrawer.cpp` — `appendHeadingChevron` (3 verts, forward tip); `appendGroundBoneStrip` (quad strip along each Y-star link). Node markers remain camera-facing quads. The **flux-cap Y-star** skeleton + triangular chevron give a readable “which way it crawls” at low zoom without a full mesh rig.

**Not engine-generic:** The engine FK layer is shape-agnostic; triangle bias is an evo-lab rendering convention tied to tidal crawl and chemotaxis readability.

### 2.10 M as cyclic FSA — feed ticker & satiation intercept

**Conceptual model:** **M** is a **finite-state automaton** whose active state is “chew when allowed.” Each tick with energon in contact range, M *may* execute one consume step (one field byte → mouth store). **Intercept signals** (same tick, before bite) shift appetite — analogous to **vagal distension / leptin-like relaxation of hunger**: you do not stop at the biologically “correct” joule count; you stop when internal state **feels full enough**.

**States (shipped v1):**

```text
                    threat / flee dominant
                           │
                           ▼
              ┌─── SUPPRESSED (allowFoodBite = false)
              │
              CHEWING ────┤    chewPaused ∧ foodLocked ∧ foodApproach ≤ 0.15
              │           │
              │           ▼
              │    REFUSE (allowFoodBite = false, feedSuppressed)
              │
              └─── CHEWING (allowFoodBite = true) ──► bite +8 net B ──► digest surplus ──► loop
                        ▲
                        │ satiation ≥ 5/7 (latched chewPaused)
                        └── PAUSE band (hysteresis: resume ≤ 3/7)
```

**Intercept inputs (`computeCampFeedIntent`, Go/NoGo):**

| Signal | Effect |
|--------|--------|
| `localSatiation` | From `mouthFuelConfidence(mouthChewFill)` — **chew buffer only** (0–32 B → 0–7) |
| P approach / flee | Go = hunger × approach when food-locked; threat focus hard-stops |
| A activity | NoGo on feed when crawling hard |
| `chewPaused` | Latched at satiation ≥ **5/7**; clears only ≤ **3/7** |
| REFUSE | `chewPaused` ∧ food-locked ∧ P approach ≤ **0.15** → mechanical bite off; strong approach overrides; horror feedbag (not food-locked) unaffected |

**Bytes before feedback “bites back”:**

The **32 B chew buffer** (`kMouthLocalStoreMaxBytes`) drives satiation broadcast and chew FSA. M **wallet** (`store`) can swell from conveyance; surplus digests to hub.

| Feedback | Threshold (confidence byte) | Approx. chew fill | Notes |
|----------|----------------------------|-------------------|-------|
| Gradual appetite ↓ | continuous chew NoGo | from **0 B** upward | Baseline feed drive **0.35** when not food-guided |
| Crawl brake (M→A) | ≥ **5/7** mouth confidence | **32 B** chew fill → **7/7** | Aligns peripheral full with actuator brake |
| REFUSE | `chewPaused` ∧ food-locked ∧ P approach ≤ **0.15** | **~20+ B** chew (≥5/7) | Horror feedbag (not food-locked) still bites at contact; strong food approach overrides REFUSE |
| Hub vent (C expulsion) | ≥ **6/7** hub confidence | hub scale | Blue signal byte; excess route when replete |

**Implementation:** `OrganismMouth.cpp` + `CampNeuronGating.hpp` (`updateMouthChewPause`, `campMouthChewRefuseActive`). `mouthChewPaused` lives on `SkeletonNode`.

### 2.11 Metabolic break-even — bytes/tick to live indefinitely

**Question:** What average **net bytes per tick** must enter the organism (field → M) to balance basal entropy and stay alive forever (reserve neither climbing nor draining)?

**Ledger (`CellConstants.hpp`, shipped):**

| Sink | B/tick | When |
|------|--------|------|
| Basal ×4 (P,M,C,A) | **4** | All neurons alive |
| P scan | **+1** | P wallet ≥ 1, scan runs |
| P transduction | **+1** | Scan finds candidates |
| A stroke | **+2** | Crawl pays (`A` wallet; not always every tick) |
| **Idle regulated floor** | **6** | Basal + P scan + transduction, no stroke |
| **Horror-crawl ceiling** | **8** | Above + full stroke |

| Source | B/tick | Cap |
|--------|--------|-----|
| Field bite (per M) | **+8 net** | **1 bite/tick/M** max (`kEnergonUnitsPerByte` 9 gross − `kBiteCost` 1) |
| Internal conveyance | 0 net | Moves bytes; **does not** create energy |
| Hub signal expulsion | −1 | Satiation vent — exports excess |

**Break-even (steady state, single mouth):**

```text
bites_per_tick × kBiteNetYieldBytes  ≥  burn_per_tick
bites_per_tick × 8  ≥  6   (idle)  →  need 0.75 bites/tick at full contact
bites_per_tick × 8  ≥  8   (crawl) →  need 1.0 bite/tick  (feedbag equilibrium)
```

**Verdict (shipped 2026-08-27):** **8 net B/chew** makes single-M **feedbag** survival feasible when food contact is sustained (~1 bite/tick covers full crawl). Foraging tank still depends on sunfall density and satiation brakes.

**Sweet spots (design levers):**

| Goal | Lever | Example target |
|------|-------|----------------|
| 1 M, idle forever | Raise net/chew or lower burn | `kEnergonUnitsPerByte = 7` (net 6) **or** basal 1.5 B/tick organism-wide |
| 1 M, keep constants | Accept reserve drain | Need **100%** food contact **and** evolution of lower P cost / crawl suppression |
| N mouths, keep constants | Horizontal intake | **≥6 M** at 1 net B/tick each for idle break-even (before η loss) |
| Felt-full regulation | Align 5/7 with 32 B M | §2.10 — stop chewing when wallet nominal-full, not at 31k B |

**Chew / binary economics (evidence, not 4/8/16 per chew today):**

| Interpretation | Value | Role |
|----------------|-------|------|
| **Field byte consumed** | 1 byte (8 bits) | One bite removes one byte from energon string |
| **Gross yield** | **9 B** | `kEnergonUnitsPerByte` |
| **Mastication tax** | **1 B** | `kBiteCost` |
| **Net stored** | **8 B** | `kBiteNetYieldBytes` — feedbag / IV equilibrium with 8 B/tick crawl |
| **Stroke batch** | **2 B** | `kActuatorStrokeCostPerTick` — locomotion slice of total duty |

**Biological scale note (order-of-magnitude):** Real bacterial chemosynthesis can exceed maintenance by ~10× under feast; our **6:1 burn:net-intake** gap at idle is an intentional **evolutionary pressure** (conveyance, multi-M, trust, environment tuning) — document before mating so viability predicates are honest.

**Empirical cross-check:** `test_regulation_satiety.cpp` — 3 visual day/nights, food well, nominal spawn: **~16 B/tick net drain**, **~0.26 bites/tick**; alive but not at equilibrium. Pre-satiated hub test: regulation + vent works; **not** autotrophy proof.

### 2.12 Peripheral bankruptcy — grace, not hub subsidy

**Rejected:** auto-paying P/M/A basal from the C hub when a peripheral wallet is empty (“hub subsidy”). That bypasses conveyance/trust evolution (H1) and lets a full hub keep starving modules alive without axon-mediated refuel.

**Shipped instead:** `kNeuronBasalGraceTicks = 8`. Each neuron tracks `basalArrearsTicks`; if basal cannot be paid from its wallet (or C hub for Computer), arrears increment until grace is exceeded, then the neuron dies. Grace covers **same-tick ordering**: `tickNeuronViability` runs **before** `transferEnergy`, so a hub→peripheral dispatch on the same frame can refill a wallet that was empty at viability check.

**Death rule:** empty wallet + no incoming energon → death after **9** consecutive unpaid basal ticks (arrears 0→8, kill on 9th failure). No bailout from hub unless conveyance actually moves bytes that tick (after viability).

**Gen-0 vs oracle:** factory `trustFeed = kTrustMin` (~33%); feedbag oracle tests set `trustFeed = 100%` in harness only — upper bound on conveyance, not sim default.

---

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

campers do **not** use a deforming water mesh. Placement is a cheap **category model** over depth — one sample per entity per tick, no extra grids.

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

**Why categories:** Correct enough for a tidal evo lab, discrete enough for future **P** senses (“which band am I in?”), cheap enough to run forever. Terrain wetness is still **vertex colour** on fixed geometry; campers use the band model for vertical truth.

| Tide state | Effect |
|------------|--------|
| **High** | Seas connect; low islands shrink; lake shores expand |
| **Low** | Land bridges, exposed shelves; refugia in pools and lakes |
| **Transition** | Emergent isolation / migration corridors |

**Organisms (post–Phase 0):** Aquatic **campers** — swim where wet. Stranded on land when exposed: stress/dormancy/death (tune). Kinematic FK on XZ with **water-column placement** for Y. **No land survival** until transitional morphologies are designed.

**Energon (post–Phase 0):** The unified **information–energy** substrate (technical name). Sunfall strings and fragments are **campers** too — they nom nothing, but they get nommed. See §3.5.

**Defer:** multi-basin independent water levels; Navier–Stokes; arbitrary file drops.

### 3.5 Energon — information–energy substrate

**Energon** is any byte-string entity in the world: sunlight rain, uneaten fragments, or Computer expulsions. One primitive, multiple origins.

#### Origins

| Source | When | Typical size | Notes |
|--------|------|--------------|-------|
| **Sunfall** | Day phase of cycle | Random 1–8 bytes | Spawns at sky; falls on land and sea; **active only in wet cells** (or rots on land) |
| **Cloaca vent** | C hub (routine / mate / distress) | 1–3 bytes by band | **RGB semantic triplet** — see below; not sunfall |
| **Corpse release** | Neuron death / kill cascade | All remaining wallet bytes | Uncontrolled — distinct from voluntary cloaca signals |
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
  uint8_t  origin;         // Sunfall | Signal | Fragment | Waste
  float x, y, z;
  float ttl;               // decay timer — mandatory
};
```

**Entropy rule:** every blob has TTL; if not eaten, it decays. No infinite accumulation.

#### Ingestion (emergent granularity)

- `mouth_width` from phenotype (e.g. count of M in sequence, capped 1–8).
- Each tick: bite `min(mouth_width, blob.remaining)`.
- Simple organisms (1×M) nibble single bytes off long sunfall strings; complex (multiple M) take larger bites **without** authoring “top/bottom feeder” roles.

#### Signaling — RGB cloaca triplet (circumstance + bit cost)

**Principle:** Signalling is **never free** — it spends the same **bytes** the organism needs to live. The RGB metaphor maps to **metabolic tier**, not paint alone: **Red = most expensive / highest semantic value**, **Green = routine**, **Blue = cheapest alarm**. Receivers must infer intent from colour **and** cost (trail length, byte tag, sender health) — ambiguity is intentional; trust learns outcomes.

| Colour | Meaning (sender) | Typical circumstance | Vent cost (design target) | P default bias |
|--------|------------------|----------------------|---------------------------|----------------|
| **Blue** | **Distress** — “something is going wrong” | Basal arrears, peripheral bankruptcy, hub below reserve, pre-death stress; optional cheap ping before corpse dump | **1 B** / tick max; short TTL | **Threat** *or* **Food** (impending corpse) — receiver decides; high false-positive cost if you chase every blue |
| **Green** | **Status quo** — “paying the bills, surviving not thriving” | Metabolically active, eating, venting hub pressure when replete but **not** soliciting mate | **1 B** / tick when hub accepts vent; low salience | Weak / ignore — background conspecific noise |
| **Red** | **Mate-open** — “cloaca open, soliciting” | Mate-readiness predicate (replete + solvent + clear + age); **only** when explicitly willing to reproduce | **2–3 B** / tick; costly tag byte in `data` | **`Mate`** lock **only if** receiver also mate-ready |

**Not the same as death spill:** A dying neuron **already** releases its wallet as energon (corpse feast — uncontrolled, all bytes). **Blue** is a **cheap live alarm** before or beside that: “maybe I die, maybe you eat my corpse, maybe you die the same way.” Scavengers and paranoid campers both have something to learn from blue trails — evolution decides which interpretation wins.

**Hub vent remapping (deprecates v1 “blue = full = mate lure”):**

```text
hub replete, not mate-ready     → Green vent (pressure relief, status quo)
hub replete + mate predicate    → Red vent (solicitation)
distress predicate              → Blue vent (alarm)
starving / silent               → no vent (can't afford signal tax)
```

**Mate-readiness (red sender — “stick your dick in it”):** Fullness is **necessary, not sufficient**:

```text
hubSatiation   ≥ kComputerSatiationConfidence   // can afford red tax
mouthSatiation ≥ kMouthInhibitActuatorConfidence // not starving
A/P solvent    ≥ stroke / scan floors
threatClear    flee ≤ approach
ageFloor       survivalTicks ≥ kMateMinAgeTicks
cooldown       not recently mated
```

**Receiver red response:** Symmetric mate predicate — don't lock Mate on red unless self could pay **red-tier** cost too (quality filter). Hungry campers may still **smell** red but shouldn't commit approach without solvency.

**Energon cost ledger (bits model):**

| Tier | Bytes vented | Net after η/TTL waste | Evolutionary meaning |
|------|--------------|------------------------|----------------------|
| Blue | 1 | ~1 | Cheap gossip / alarm — spam is affordable, trust should down-weight chronic blue |
| Green | 1 | ~1 | Basal social noise — hub breathing |
| Red | 2–3 | 2–3 | Honest costly mate ad — only stable replete lineages sustain chronic red |

**Implementation sketch (Phase 3+):** extend `EnergonOrigin` or tag `data` low byte as `{Distress=0xB?, Baseline=0xG?, Mate=0xR?}`; `tickComputerPhase` chooses band by predicate; `scanFood` / `scanMateTrails` / threat weights split by origin; renderer maps origin→RGB. **Shipped v1** still uses single `Signal` origin on hub full — migrate when mating lands.

**Evolution hook:** Register bytes / trust on P→A learn which blue trails preceded corpse vs bluff; red trails that led to viable offspring raise Mate trust.

**Not in v1:** authored trophic tiers or forced scavenger niches.

#### Visual language (render)

| Element | Look |
|---------|------|
| **Sunfall strings** | Bright warm tones (white/yellow); **length ∝ byte count** |
| **Green cloaca** | Status-quo vent — low emissive, easy to ignore |
| **Blue cloaca** | Distress ping — cold/cyan, short TTL |
| **Red cloaca** | Mate solicitation — siren, costly, rare |
| **Corpse release** | Death dump (all wallet bytes) — may read “feast red” visually; semantically **uncontrolled**, not mate |
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

**Current implementation (Phase 2.x — CAMP camper default):**

1. World tick + day cycle + energon tick (sunfall with chaos jitter on spawn params)
2. Per organism: **perceive** (P scans focus cone) → **feed** (M interoception bite gate) → **neuron pre-advect hooks** (M fuel signals) → **advect** → metabolise → viability → purge → transfer → signal → prune → colony → remove dead
3. Render terrain, water, energon, organisms (bone + neural line + inspector in `game/`)

### 3.4 Geographic isolation (tidal)

Isolation is **dynamic**: low tide → refugia; high tide → gene flow. Mate **locally** by default (proximity in same wet component). **ε migration** and tide cycles prevent permanent split or merge. Optional later: persistent tidal pools (low basins that stay wet).

---

## 4. Evolution & Genetics

See **[PARTHENOGENESIS.md](PARTHENOGENESIS.md)** for full R1 spec and **[EVOLUTION.md](EVOLUTION.md)** for rollout. Brief pointer:

- Two-layer entropy: structural gate (~3%) then mandatory ±3% parametric jitter on every clone
- Unified energon ledger — abort = bytes spent, no child (no separate stillbirth taxonomy)
- Grover `{P,M,C,A}` floor at birth collapse

### 4.0 Parthenogenesis energon budget (proposed)

First reproductive closure is **asexual split** with **variable cost** (see [EVOLUTION.md §4](EVOLUTION.md)). Baseline `[CAMP]` anchor:

| Line item | Bytes |
|-----------|-------|
| Offspring endowment (median) | 172,800 (2 fuel-days) |
| Construction overhead (baseline) | 86,400 |
| **Baseline debit** | **~259k B** |
| Parent reserve after | ≥ 86,400 |

**Duplication raises cost:** `[CCAMP]` child ≈ 1.25× baseline. Gates: age ≥ 600 ticks + solvency for **drawn** child bill. Training curriculum inspired by **Lorenz strange attractors** (EVOLUTION.md §2.3). **R0 insertion (HGT) precedes R1 parthenogenesis** per pre-LUCA literature.

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

#### 4.3.1 Mate focus and downstream approach (shipped path vs Phase 3 gate)

**Status:** Approach chemotaxis toward mates is **wired**; **mating collapse / offspring spawn is not** (Phase 3).

**Design focus:** Mating is **local, self-determining, and metabolism-gated** — no global fitness oracle. Reproduction rewards organisms that (a) survived the tidal oracle long enough, (b) achieved sufficient hub/peripheral fuel, (c) entered the same wet component as a partner. ε-chaos (`kEpsilonRandomMate`) prevents partner lock-in.

**Two discovery channels (v1):**

| Channel | Stimulus | P focus kind | Intent |
|---------|----------|--------------|--------|
| **Direct** | Other camper neurons in focus cone | `PerceptFocusKind::Mate` | Turn toward conspecific bearing |
| **Indirect (bee-dance)** | *(deprecated)* C hub vent read as food | — | v1 mistake — attracted scavengers to “full” |

**Planned (RGB cloaca, §3.5):** **Red** = costly mate-open; **Green** = routine hub breath; **Blue** = cheap distress / pre-corpse alarm. Corpse = uncontrolled death release (feast) — separate from voluntary blue ping.

**Downstream signal chain (shipped, per tick):**

```text
P scan (food + organisms + threat blocks)
  → integrateFocus() — mate weight ≈ 0.75 + hunger×0.15 (less hunger-driven than food)
  → focusToConfidence() — Mate: neutral + salience×2.2 → 0–7 approach byte
  → emitPerceptSignals() on P→* axons (believe channel)

A gatherActuatorInteroception()
  → read P→A (and M→A, C→A) via accumulateApproachFlee()
  → computeCampMotorIntent() — approach raises motivation; satiation brakes unless approach > threshold
  → applyCampChemotaxisHeading() — turn toward gazeHeading + focusBearing (flee adds π)

Advect → stroke toward heading → increased proximity if mate focus wins competition
```

**Phase 3 activation (not shipped):** tick step 11 — `Mate → if wet, proximate, energy thresholds met` — partner choice with `ε_random_mate`, parent-biased genotype collapse (§4.3), spawn offspring on wet tile. Proximity threshold, minimum hub bytes, and intact CAMP predicate TBD at implementation.

**Evolutionary intent:** Trust plasticity on P→A should reinforce approach bytes that preceded successful proximity / mating outcomes once reproduction exists; until then, mate approach is an **open-loop reflex** shaped only by spawn chaos and satiation competition with food/threat.

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
| `chaosSpawnRng(worldSeed, salt)` | Deterministic RNG per spawn class (stem / star-mouth / nom / energon) |
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

**Population spawn:** `CellPopulation::seedOnWetTerrain(...)` — shared wet placement, surface Y jitter (`kSpawnSurfaceYOffset`), `finalizeSpawn`, archetype-specific factory callback. Used by stem-cell, star-mouth, nom, and actuator seed paths.

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

**Tests:** `tests/unit/test_chaos.cpp`, `[nom]` (legacy tag for camper) spawn trust cases.

### 4.5 Fitness (when explicitly needed)

Composite for experiments/meta-optimization only:

```
F = w1·food_eaten + w2·survival_ticks + w3·offspring_bonus − w4·metabolic_cost
```

Default self-determining mode: **who lives and breeds is the fitness signal.**

### 4.5.1 Epoch boundaries — laptop on/off (planned, not shipped)

**Concept:** One **epoch** = one continuous sim session from launch until the user closes the viewer (Escape / window close). The laptop **off switch** is the true environmental catastrophe; within-epoch tide/starvation is ecology.

**Trigger:** Graceful exit from `VisualApp` (Escape or close). **Do not activate** until Phase 3 genetics worth persisting (genotype + trust motifs + survival telemetry).

**Selection at epoch end:**

| Cohort | Fraction | Selection rule |
|--------|----------|----------------|
| **Elite** | Top **18%** | Longest **survival tenure** this epoch (`simTick − createdAtTick` among still-alive organisms; tie-break: higher total fuel, then intact CAMP) |
| **Brink** | Bottom **18%** | Worst health at exit among alive organisms — lowest `(totalFuel / spawnFuel)` or highest basal arrears / nearest peripheral bankruptcy |

**Persist per specimen (minimal):** world seed, epoch id, cohort tag (`elite` \| `brink`), organism id, survival ticks, fuel snapshot, CAMP intact flag, developmental string / twin-string genotype (Phase 3), axon trust snapshot, computer register bytes.

**Storage sketch:** `epochs/<worldSeed>/<epochIndex>/elite/*.json` + `brink/*.json` + `epoch_meta.json` (tick count, population curve, rain config).

**Reload policy (future):** Next epoch seeds from elite-biased batch + ε-random + optional brink controls (negative benchmark); never auto-resurrect brink without author intent.

**Non-goals for v1 checkpoint:** mid-epoch autosave, full energon field freeze, resume-in-place.

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
| **engine** | Renderer3D (terrain/water), Camera, **Viewport letterbox**, **FixedTimestepClock**, Application loop scaffold |
| **evo-lab (game + app)** | Terrain, tides, wetness, genome/neural chain, Energon rules, evolution, **OrganismInspector** UI strings |

**Sim source map (Phase 2.x):**

| File | Role |
|------|------|
| `src/sim/WaterColumn.hpp`, `.cpp` | Depth bands, camper habitat placement (`NomHabitat`), tide-riding Y |
| `src/sim/Chaos.hpp`, `Chaos.cpp` | ε rates, ±3% jitter, spawn RNG, `chaosInitialStorage` |
| `src/sim/NeuralAxon.hpp`, `.cpp` | Axon struct, developmental trust init, pruning predicate |
| `src/sim/Organism.hpp`, `Organism.cpp`, `OrganismDetail.cpp`, `OrganismKinematics.cpp`, `OrganismFactories.cpp` | Skeleton + neural graphs, tick methods split by concern |
| `src/sim/NeuronTick.hpp`, `.cpp` | Pre-advect hooks + locomotion dispatch (actuator vs passive drift) |
| `src/sim/SimConfig.hpp`, `.cpp` | Runtime archetype, counts, tide period, design resolution |
| `src/game/OrganismInspector.cpp` | Hover/architecture label formatting (presentation only) |
| `src/game/OrganismDrawer.cpp`, `GameShaders.cpp` | Organism mesh batch + GLSL sources |
| `engine/Viewport.cpp`, `FixedTimestepClock.cpp` | Letterbox layout + fixed sim timestep accumulator |
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

**2.x CAMP camper ✓ (current visual default):** Developmental **P → M → C → A** chain — perceptor scans, mouth feeds, computer digests/dispatches, actuator propels. Universal 0–7 confidence bytes on outbound axons. Visual app seeds ~60 campers (`--archetype nom`, legacy alias for camper).

**2.x Water column ✓:** Band model for camper vertical placement; surface campers ride tide; grounded energon re-snaps each tick.

**2.x foundations (current):** CAMP metabolism, regulation, population-scaled rain, feedbag oracles — the **RNA/protein of cognition**: buildable substrate, not the finished evolvable organism. **Evolution is for** twin-string inheritance, mating collapse, epoch selection, and trust convergence.

**P2 (next on camper):** Computer evolution (register inheritance/mutation); temporal chemotaxis gradient (Δsalience); mate on proximity. ~~Hebbian believe trust~~ ✓ (`trustBelieveByConfidence[8]`, `NeuronTrust.cpp`).

**Chemotaxis v1 ✓:** A interoception from P→A + M→A + C→A; graded stroke; tumble bias; P-driven heading — see §2.5.

**CAMP energon conveyance ✓:** Hub store, axon routing with η payload loss, M-only field ingress — see §2.7. **Computer neuron ✓** — see §2.8.

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

### 8.1 Synaptic trust, eligibility, and prediction error (CAMP)

Peer basis for the axon believe channel — **not backpropagation**, but **three-factor neo-Hebbian plasticity**:

| Concept | Literature | evo-lab mapping |
|---------|------------|-----------------|
| Co-activation | Hebb (1949); LTP Bliss & Lømo (1973) | Pre byte emitted + post neuron integrates same phase |
| Causal order | STDP Bi & Poo (1998); Markram et al. (1997) | Tick order table §2.5 (P before M/A; A→M uses prior tick) |
| Third factor / neuromodulator | Frémaux & Gerstner (2016) *Front. Neural Circuits*; Izhikevich (2007) *PLoS Comp Biol* | Metabolic outcome after feed/advect |
| Reward prediction error | Schultz (1998, 2016 dopamine RPE reviews) | **Shipped:** `predictionErrorByte` 0–7 third factor (§8.1.1) |
| Tag-specific plasticity | Synaptic tagging (Frey & Morris 1997) | `trustBelieveByConfidence[8]` per emitted byte |
| Homeostasis / bounded weights | BCM Bienenstock et al. (1982); Oja (1982) | Fixed-point clamp 85–426; spawn ε-chaos jitter |

**Eligibility (tick-based, keep for v1):** Literature eligibility *traces* bridge delays between co-activation and reward (Frémaux & Gerstner 2016; Gerstner *Neuronal Dynamics* §19.4). Our sim phases are **discrete and causally ordered within one tick** (perceive → feed → preAdvect → advect), so **same-tick (±1 tick for A→M) co-activation is sufficient** for credit assignment. Multi-tick traces become necessary only if we introduce **delayed outcomes** (e.g. digestion ticks later on C) — defer until C phase.

#### 8.1.1 Universal byte gradient (signals + prediction error)

All neuron axon **believe** traffic uses bytes **0–7**:

| Byte | Signal semantics (P/M/A) | Planned prediction-error semantics (third factor) |
|------|--------------------------|---------------------------------------------------|
| 0 | Strong avoid / empty / idle | **Firmly disbelieve** — outcome contradicts this bin’s prediction |
| 4 | Neutral | **No update** — outcome matches expectation |
| 7 | Strong approach / full / active | **Firmly believe** — outcome strongly confirms this bin |

Discrete RPE: compare predicted confidence (what the post neuron expected from this bin) to realised metabolic outcome; map \((\text{outcome} - \text{expected})\) to a **0–7 confirmation byte** that gates `nudgeBelieveTrustBin` magnitude/sign. This keeps the RNA metaphor one alphabet — no parallel tag namespace (`I_ATE`, `I_ACTUATE`, etc. removed).

**Believe vs feed channels:** `trustBelieveByConfidence[8]` learns which **signal bytes** to weight; `trustFeed` gates **energon** on the edge (developmental 33% baseline + ε-chaos jitter at spawn; runtime `nudgeTrustFeed` after `conveyCampEnergon`).

### 8.2 Metabolic routing, dissipation, and waste export (CAMP conveyance)

Peer basis for axon **feed** channel economics — complementary to §8.1 (believe/plasticity):

| Concept | Literature | evo-lab mapping |
|---------|------------|-----------------|
| Activity-coupled metabolite routing | Astrocyte–neuron lactate shuttle (Pellerin & Magistretti 1994; glutamate-triggered export) | Surplus routes on open feed axons when source confidence + believe trust align |
| Per-hop dissipation | Attwell & Laughlin (2001) — brain energy budget; synaptic costs ≫ Landauer floor | `applyHopLoss`: `delivered = round(bytes × η_energy)` each hop |
| Saturated store → export | Lysosomal exocytosis / secretory autophagy when capacity exceeded | Axon returns to M **dissipate** (entropy sink) |
| Irreversibility / degraded return | Second-law thermodynamics; no “perfect recycle” in open systems | Bytes re-entering field via M cloaca are lower-grade `Waste` blobs |

**Honest gap:** No single biological paper specifies “mouth cloaca only on returns”; the split is a **design hygiene** choice — fresh ingress never bypasses axon circulation, routed bytes lose grade on re-entry.

---

## 9. Open Decisions

- [x] **Hebbian believe trust (M/A receivers)** — `trustBelieveByConfidence[8]`; three-factor updates; tick-aligned eligibility.
- [x] **Believe trust on P** — post-perceive learning on M→P / A→P (`applyCampPerceptorTrustLearning`).
- [x] **Prediction-error byte (0–7)** — discretized RPE third factor (§8.1.1) in `NeuronTrust.cpp` / `NeuronSignal.hpp`.
- [x] **`trustFeed` runtime plasticity** — CAMP axon energon transfer + chaos-jittered `nudgeTrustFeed` after `conveyCampEnergon`.
- [x] **CAMP energon conveyance** — hub store, axon surplus routing, η payload loss, M-only ingress (§2.7).
- [x] **Computer neuron (developmental)** — digest, register match, hub dispatch, signal expulsion (§2.8); evolution deferred.
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
| **camper** | Collective name for things in the wet layer that exist to **nom** (organisms + energon food) |
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
| **Skeleton link** | Kinematic bone between skeleton nodes; optional legacy energy η |
| **finalizeSpawn** | Single organism hook: developmental axon trust + ±3% jitter on skeleton/heading |
| **Chaos module** | `Chaos.hpp` — all ε rates, jitter helpers, spawn RNG salts |
| **Water column** | Depth band + habitat placement at a world XZ sample (`WaterColumn.hpp`) |
| **NomHabitat** | Preferred vertical zone: Surface, Benthic, Shallow, Pelagic |
| **Axon pruning** | Structural removal when both trust channels hit 0 (not inhibition) |
| **Focus** | Perceptor scan region (direction + width) |
