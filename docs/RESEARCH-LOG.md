# evo-lab — research log (publication-oriented)

**Started:** 2026-08-27  
**Status:** Active exploratory / engineering research  
**Working title:** *Distributed metabolic regulation in a minimal aquatic artificial organism (CAMP camper)*

### 2026-08-30 — Heritable tumble, energon anchor, pineal analogue

**Sim changes:** Tumble is **heritable** (`tumbleRateFactor`, `tumbleTurnFactor`, `tumbleChiralityBias` — jittered at spawn / parthenogenesis). Tumble fires only when **unanchored**: no P food/threat lock, no directional M taste (symmetric ambiguity still tumbles — Lorenz orbit, not lock-on). Hub-satiation tumble boost removed. M→A gain and turn scale down when P locked (vestigial mouth chemo).

**Biology note (user query — pituitary vs pineal):** The “deep buried photosensitive number co-opted for circadian control” story matches the **pineal** (epiphysis), not the **pituitary** (hypophysis — endocrine master gland, not a photoreceptor). Literature supports:
- **Anamniotes / lamprey–fish–amphibian:** pineal is **directly photosensitive**; melatonin rhythms driven locally ([Falcón et al. 2007 PMC1693265](https://pmc.ncbi.nlm.nih.gov/articles/PMC1693265/); [J Pineal Res cross-species scRNA 2024](https://doi.org/10.1111/jpi.12927)).
- **Mammals:** pinealocytes **lost direct photosensitivity**; light via **retina → SCN → sympathetic** innervation; gland co-opted for **neuroendocrine melatonin** ([ICB 1983 pineal photoperiod review](https://doi.org/10.1093/icb/23.3.597)).
- **Parietal / “third eye”:** dorsal median photoreceptor in sauropsids; mammals retain condensed **pineal** without parietal opening ([“The lonely eye” PMC1772576](https://pmc.ncbi.nlm.nih.gov/articles/PMC1772576/)).
- **Duplication / composite median eye:** recent work posits vertebrate retina repurposed from a **composite ancestral median eye** with ciliary + rhabdomeric modules ([Kafetzis et al. 2026 Current Biology preprint](https://badenlab.org/wp-content/uploads/2026/02/2026-Kafetzis-et-al-Current-Biology.pdf)).

**Sim mapping:** **P** = lateral focal vision (dominant lock); **M** taste = vestigial omnidirectional chemo (pineal/barbel analogue) — full gain when P silent, damped when P locked. **A** tumble = Berg run-and-tumble on the **unanchored** Lorenz surface (EVOLUTION.md §2.3).

### 2026-08-30 — Symmetric food choice + stem-cell Hz coordinator

**Observation (live runs):** When energon is roughly equidistant / symmetric around a camper, heading wanders — matches nursery-blind homing but visible in open field. Root cause in sim: M taste **vector-sums** salience-weighted directions → zero bearing when symmetric; tumble suppression treats |bearing|≤0.25 as “tracking” → straight run with no break.

**Literature (puppy / two-bowl analogue):**
- **E. coli dual gradients:** Decision uses **Tar/Tsr receptor ratio** as internal bias, not pure geometry ([Kalinin et al. 2010, JB](https://journals.asm.org/doi/10.1128/jb.01507-09); [PRX Life 2024](https://doi.org/10.1103/prxlife.2.013001) — phenotypic variability → “bet hedging”).
- **No gradient / flat field:** Berg run-and-tumble — **stochastic tumbles** break symmetry; temporal ΔC when field is not static.
- **Multichoice spatial decisions:** Vector averaging fails; brains use **serial binary bifurcations** ([PNAS 2021 geometry paper](https://www.pnas.org/doi/10.1073/pnas.2102157118)).
- **Equal-alternative forced choice:** **Idiosyncratic bias** emerges from neural noise without external reason ([Nature Human Behaviour 2019](https://www.nature.com/articles/s41562-019-0682-7)).

**Sim mitigations (2026-08-30):** `mouthTasteSymmetricAmbiguity` when vector cancels + flat Δ; **idiosyncratic bearing bias** per mouth node; **tumble boost** (`kMouthTasteSymmetryTumbleBoost`) so campers don’t lock straight.

**Taste grid weights (2026-08-30):** Coarse M layer now deposits **all wet grounded energon** with byte weights — sunfall/cornucopia **1.0**, fragment **0.85**, distress blue **0.12**, baseline green **0.05** (mate/waste/signal **0**). Peak cell picks highest weighted mass so fresh sunfall beats sparse blue; blue-only fields support cannibal homing without equating alarm to feast.

**Coarse taste sensory layer (2026-08-30):** Separate **256×256** byte-density map over world extent (`EnergonTasteSensoryGrid`, rebuilt each tick with spatial queries). Mouth taste steers toward the **peak coarse cell** within taste radius — clumping / mass-flux analogue, not per-blob vector sum. Fine P cone unchanged.

**Population sync (2026-08-30, visual):** Multiple campers under the same sunfall / tide / patch geometry fall into **the same heading/tumble regime** — looks like cannibal lock-step but tracks **shared choice paralysis** (symmetric taste, run-and-tumble, REFUSE/chew FSA), not coordinated feeding on death emissions. Diagnose with per-Nom inspector + `V` neuron overlays before assuming social copying.

**Mouth→C hub (2026-08-30):** When M wallet sits at the 32 B peripheral cap, further bites were **silently dropped** — energon vanished from the field but C hub never grew. Fix: overflow bytes route to C; tick order is chew → **digest** → M conveyance (DESIGN-NOTES order).

**Feedbag oracle birth (2026-08-30):** Oracle uses the same ε structural morphogenesis as natural parthenogenesis (chaos preserved). Spawn gate is `campGenotypeValid` (P+M+C+A floor + legal axons), not full `isCampNom()` — inspector labels non-canonical topology **freak**. A lone **M** label usually means P/C/A died post-spawn (fuel), not a single-neuron birth: deletion cannot remove the last neuron of each type.

**Stem-cell Hz coordinator (precursor C):** `NeuronCoordinator.cpp` — every node runs local pattern match (4-byte register) over fuel + sense + Δ → `coordinatorDutyScale`. Full C neuron composes dispatch gain × duty (recursive mini-C inside C). Tick: after M taste, before advect. Constants: `kCoordinator*` in `CellConstants.hpp`.

---

## 1. Research frame

### Core question
Can a **minimal four-module organism** (Perceive → Metabolize → Compute → Actuate) achieve **viable homeostasis** in a tidal energon field without hand-tuned controllers—such that **evolutionary search** (genotype + trust plasticity) is the primary mechanism for long-horizon survival?

### Hypotheses (testable)

| ID | Hypothesis | Operational metric |
|----|------------|-------------------|
| H1 | Decentralized peripheral wallets + central hub reserve create **selective pressure** for conveyance/trust policies that refuel starving modules | Time-to-A-refuel from hub; A death before C death rate |
| H2 | Satiation brakes (M→A, C→A) sufficient for **open-loop regulation** at generation-0 without learning | Stroke suppression after feed; crawl burn ↓ after satiation |
| H3 | Multi-mouth morphologies improve **intake bandwidth** vs single deep mouth buffer | Bytes/tick ingested per morphology at equal total M storage |
| H4 | Star topology (C hub, P/M/A radial) improves **spatial sampling** vs chain topology | Food contacts / fuel-day; P scan hit rate |
| H5 | Trust plasticity on believe + feed channels converges to **functional axon motifs** under oracle survival | Belief entropy ↓; feed trust ↑ on rewarded edges |
| H6 | **Upper-bound satiety** (abundant food): regulation suppresses crawl and routes excess via hub signal expulsion | Stroke rate ↓; Signal-origin blob rate ↑; Fragment flood absent |
| H7 | **Autotrophic break-even:** single-M intake cannot match idle burn at net +1 B/chew; indefinite life requires reserves, env tuning, or multi-M / yield change | At **+8 B net/chew**, feedbag needs ~1 bite/tick for crawl break-even; see DESIGN-NOTES §2.11 |

### Axon bundles (musculature analogy, 2026-08-27)

A skeletal **P↔C** link may correspond to a **bundle** of directed neural axons (e.g. P→C and C→P), not a single edge. Pruning/chaos may break symmetry over evolutionary time. Mechanical flex (future) may co-evolve with bundle density on the same bone — see KINEMATICS phases 5–6.

### Non-goals (explicit)
- Full-tank quantum simulation (see DESIGN-NOTES §4.6).
- Hand-authored controllers for crawl/feed.
- Indefinite population sustainability without environmental tuning (pre-train phase accepts attrition).

---

## 2. System under study

**Organism:** CAMP camper — developmental P(1), M(2), C(3), A(4); 12 neural axons (all directed pairs); skeleton **Y-star** (C hub, P forward, M/A on ±120° arms).

**Environment:** BarrenWorld tidal shallow sea; sunfall energon; wet placement at spawn.

**Control architecture:** Universal 0–7 confidence bytes; believe-trust + feed-trust; Computer pattern register + dispatch gain; no RL, no backprop.

**Key constants (see `CellConstants.hpp`):** basal 1 B/tick/neuron; mouth cap 32 B; hub reserve ¼ fuel-day; stroke 2 B from A wallet; bite net **+8 B** (`kBiteNetYieldBytes`); basal grace **8 ticks** before neuron death when unpaid.

### Feedbag oracle protocol (benchmark harness)

Upper-bound intake benchmark — **not** default tank behaviour:

| Control | Setting |
|---------|---------|
| Food | Wet well at mouth every tick; sunfall spawn **off** |
| Perceive | **Skipped** (no threat/flee blocking grazing) |
| `computerFeedGain` | **1.0** in test only (full dispatch) |
| Axon trust | **100%** feed + believe in test only (`prepareFeedbagOracleAxons`) |
| Spawn | 2 fuel-days nominal; no hub pre-fill |
| Day length | `kVisualDayCyclePeriodTicks` = 1800 |

Shared runner: `runFeedbagOracle(cycleDays, worldSeed, energonSeed)` in `test_regulation_satiety.cpp`. Logs daily snapshots via Catch2 `INFO` (fuel, hub, mouth, actuator, bites, strokes, signal expulsions).

---

## 3. Observations (chronological)

### 2026-08-27 — Energon flow review (pre-train pause)

**Setup:** Default seeding `chaosInitialStorage` 1–3 fuel-days; 50% hub / 50% peripheral split; 60 campers; sunfall 6–14 blobs/tick.

**Findings:**
- Regulation architecture is **structurally closed-loop** (digest, brakes, conveyance, computer dispatch).
- Actuator wallet decoupling from hub is **intentional evolutionary pressure** (author): legs thirsty while hub full.
- Population-level sunfall **<** full-population basal duty cycle → expect **thinning** until intake matches drain.
- Test coverage: 400-tick intact without feed (short vs fuel-day scale); no long-horizon autotrophy benchmark yet.

**Screenshot (tick ~777, seed 42, day ~22):** HUD reports **0 intact CAMP / 60 degraded**; inspected camper #13 = single actuator survivor. Consistent with peripheral bankruptcy + neuron death cascade over multi-hour run. **Action item:** add telemetry run logging survival curves before env tuning.

### 2026-08-27 — Kinematics intent

**Current:** `KinematicLocalPose` yawDelta = **0** everywhere → rigid bind pose. Engine supports joint constraints (±π default); no IK; no actuator-driven flex.

**Target morphology:** Flux-capacitor Y — C center, P forward, A/M on 120° arms (see KINEMATICS.md update pending).

**Multi-mouth:** Scale intake **horizontally** (N mouths × cap) not deep single-M buffer; star-mouth factory precedent exists (`makeStarMouthOrganism`).

### 2026-08-27 — Upper-bound satiety regulation test

**Test:** `tests/unit/test_regulation_satiety.cpp` — abundant food well at mouth; hub pre-filled to satiation threshold.

**Expected signatures (H6):**
- Mouth bites >92% of measurement ticks
- Actuator stroke ≤15% of ticks; inhibited ≥70%
- Computer hub expels **Signal**-origin blobs (blue); Fragment (red M-cloaca) stays low
- CAMP topology intact after 600 ticks

**Ramp companion:** 4000-tick always-eating run from nominal spawn confirms hub monotonic growth toward expulsion regime.

**First run results (2026-08-27):** Upper-bound harness **passes** (`132` tests). Steady-state satiety window (480 ticks, hub pre-filled):
- `strokesPaid=0`, `actuatorInhibited=480/480` — crawl fully suppressed
- `signalOut≈447` — computer hub expelling ~1 blue byte/tick (primary cloaca under max satiety)
- `fragmentOut=0` — no red M-cloaca flood from axon feed
- `bites≈102`, `feedSuppressedWithFood≈217` — regulation **refuses** excess intake when M+C satiated (not gluttony)
- `mouth.store≈143` when hub near max — digest backlog above 32 B cap when hub acceptance is saturated (design note for multi-M)

Ramp: `hubPeak≈57k` from `43k` start in 4000 ticks; hub end lower due to basal+expulsion equilibrium — satiation threshold (~222k B) not reached from spawn alone in that horizon.

### 2026-08-27 — Three visual day-night cycles (food well, nominal spawn)

**Test:** `test_regulation_satiety.cpp` — `[regulation][satiety][long]` — `DayCycle(1800)` × 3 = **5400 ticks** (~6.25% of one fuel-day); abundant wet food at mouth; **2 fuel-day** nominal spawn (no hub pre-fill); diurnal sun on energon + perception.

**Results (first run):**
- **Alive + intact** after 3 cycles — all four CAMP neurons survive
- **Fuel:** 172 800 → 85 987 B (~50% net drain); `hubMin≈85 748`
- **Intake:** ~1402 bites / 5400 ticks (~0.26/tick) — not enough to match burn
- **Crawl:** 460 strokes (~8.5%) — more than pre-satiated short window (0)
- **Hub signal expulsion:** 0 — never reached satiated steady state; no blue vent regime
- **Drain rate:** ~16 B/tick net (basal floor ~4–6 B/tick + P costs + strokes + conveyance/entropy)

**Verdict:** **Survives** three day/night cycles in a food well but **does not defy entropy** — reserves monotonically fall. Extrapolation: ~6 visual days to bankruptcy from 2 fuel-day spawn at this drain rate (linear, optimistic). **Not** proof of indefinite life; **not** satiated equilibrium. Contrast with pre-filled hub short test (H6) where regulation + venting dominate.

**Note:** Visual day (1800 ticks) ≠ fuel-day (86400 ticks). Long-horizon autotrophy benchmark at fuel-day scale remains P0 backlog.

### 2026-08-27 — Eight-byte feedbag + 3 visual day oracle

**Constants:** `kEnergonUnitsPerByte = 9`, `kBiteNetYieldBytes = 8`; feedbag grazing (`allowFoodBite` unless threat/flee); dev axons `η=1`, `trustFeed=100%`.

**Test:** `[regulation][satiety][long]` feedbag oracle (no perceive/threat; food well; sunfall off).

| Metric | Before (1 B net) | After (8 B net feedbag) |
|--------|------------------|-------------------------|
| Bites / 5400 ticks | ~1264 (~23%) | **5400 (100%)** |
| Fuel end (2 fuel-day spawn) | 85 987 | **136 973** |
| Net Δ / tick | −14.4 B | **−6.6 B** |
| Linear runway from end | ~3.7 visual days | **~11.5 visual days** |

**Life projection (linear, pessimistic):** at −6.6 B/tick from end reserves, **~20 700 ticks ≈ 11.5 visual days ≈ 0.24 fuel-days** until bankruptcy. Much of the 3-day dip is **peripheral spawn buffer draining to 32 B cap** (not steady-state crawl deficit); longer runs should flatten toward hub accumulation.

**Not yet indefinite** at 8 B net in 3-day window — peripheral spawn buffers still draining. Default tank (with threat + partial contact) still harder.

**Design decision (same day):** **Hub basal subsidy rejected** — no auto-pay from C when P/M/A wallets empty. **Basal grace = 8 ticks** before death (viability runs before conveyance; grace absorbs same-tick refuel lag). See DESIGN-NOTES §2.12.

### 2026-08-27 — Nine visual day feedbag oracle (post-cap equilibrium)

**Test:** `[regulation][satiety][long][extended]` — 9 × 1800 = **16 200 ticks**; seeds world=11, energon=17; shared `runFeedbagOracle`.

**Results:**

| Metric | 3-day (prior) | 9-day |
|--------|---------------|-------|
| Bites / window | 5400 (100%) | **16 200 (100%)** |
| Fuel start (2 fuel-day) | 172 800 | 172 800 |
| Fuel end | 136 973 | **180 049** |
| Fuel min | — | **116 649** (days 1–3 peripheral buffer drain) |
| Net Δ / tick | −6.6 B | **+0.45 B** (net accumulation) |
| firstThird Δ/tick | — | ~−7.1 B (early buffer drain) |
| lastThird Δ/tick | — | **+4.0 B** (hub accumulating post-cap) |
| Alive + intact | yes | **yes** |

**Interpretation:** Early window **understates** steady-state — large peripheral spawn allocations drain toward 32 B cap while hub accepts overflow. By days 7–9 the organism is **net positive** (~+4 B/tick in last third), trending toward hub satiation / signal vent rather than bankruptcy. Still an **oracle** (100% contact, no threat); not proof of default-tank indefinite life.

**Life projection at day-9 end:** non-draining over measured window; linear extrapolation from +0.45 B/tick would grow reserves (hub-limited by vent threshold ~222k B).

### 2026-08-27 — Twenty-seven visual day feedbag oracle (cloaca / steady-state health)

**Test:** `[regulation][satiety][long][marathon]` — 27 × 1800 = **48 600 ticks** (~15 s headless); seeds world=11, energon=23.

**Results:**

| Metric | 9-day | 27-day |
|--------|-------|--------|
| Bites / window | 16 200 (100%) | **48 600 (100%)** |
| Fuel end | 180 049 | **283 168** |
| Fuel min | 116 649 | 116 649 (same early dip) |
| Net Δ / tick | +0.45 B | **+2.27 B** |
| Hub peak | — | **259 198 B** (~99.99% of `kComputerHubStoreMaxBytes`) |
| Hub signal vents (C cloaca) | — | **26 463** total; **12 600** in final 7 days (~1 B/tick) |
| Field Signal blobs (net) last week | — | **0** (TTL purge = expulsion rate; use `lastHubSignalExpelledThisTick` for truth) |
| Fragment expulsions (M red cloaca) | — | **0** |
| Alive + intact | yes | **yes** |

**Cloaca health verdict:** Hub reaches satiation threshold and vents **blue Signal-origin** bytes at ~**1 tick⁻¹** in steady state — classic “digest in, signal out” regularity. **No Fragment flood** (dysregulated M/axon feed spam). Field blob count plateaus once energon TTL balances expulsion; direct hub-vent counter added (`lastHubSignalExpelledThisTick`) for oracle telemetry.

**Interpretation:** 27 visual days under feedbag oracle is **indefinitely viable** at this intake/burn — reserves climb until hub-cap + vent equilibrium, not bankruptcy. Still oracle conditions (no threat, 100% food contact, boosted trust). Default tank remains harder.

---

## 4. Planned experiments (backlog)

| Priority | Experiment | Output |
|----------|------------|--------|
| P0 | Headless survival cohort: 1/12/60 campers × 1–3 fuel-days × sunfall {6,14} | Kaplan-Meier-style alive/intact/degraded curves |
| P0 | Upper-bound satiety regulation (`test_regulation_satiety.cpp`) | ✅ initial harness |
| P0 | Flux-cap factory (C-root star) + render regression | Visual + FK tests |
| P1 | Mouth cap sweep {16, 24, 32} × mouth count {1, 3} | Hub response lag; bytes to first dispatch |
| P1 | Generation-0 regulation audit: brake engagement rate | % ticks stroke suppressed when M or C satiated |
| P2 | Trust convergence over 10 fuel-days (single camper, food-rich patch) | Axon trust heatmaps |

---

## 5. Publication checklist (lightweight)

- [ ] Related work: ALife minimal cognition, chemotaxis run-tumble, distributed metabolism
- [ ] Reproducible seeds + config dump per run
- [ ] Figure: energon flow schematic (field → M → C → axons → P/A)
- [ ] Figure: morphology comparison chain vs Y-star
- [ ] Table: generation-0 parameters + viability floors
- [ ] Ethics: N/A (in silico)

---

## 6. Venue sketch (informal)

Plausible directions: **ALife conference** (short/long); **ECAL**; workshop track on artificial metabolic networks. PhD qualification: yes *if* formalized around a novel genotype oracle + empirical viability study—not merely implementation.

---

*Append new dated sections below as runs complete.*

### 2026-08-28 — Evolution closure kickoff

**Mouth diet layer shipped:** postingestive EMA on energon origin/cloaca band; `mouthOutboundConfidence` applies gag reflex when distress-cloaca share ≥ 55% (CTA analogue). P decodes spatial RGB; M confirms ingestion — predictive machinery hook for C trust (RPE) next.

**First design question answered:** baseline `[CAMP]` parthenogenesis ≈ **259k B** reproductive debit + **86k B** parent reserve (median endowment). Reproduction is metabolically expensive relative to idle runway (~29k ticks at 6 B/tick for 172k spawn).

**Next implementation:** R0 field insertion scaffold → R1 parthenogenesis with entropy morphogenesis + variable cost. Full plan: [EVOLUTION.md](EVOLUTION.md).

### 2026-08-28 (b) — HGT vs reproduction, variable child cost, Lorenz training

**Literature verdict:** Horizontal gene transfer / communal code exchange **predates** tight vertical reproduction (Woese Darwinian threshold; Vetsigian *Sci. Rep.* 2018; Fournier & Gogarten 2015). **Revised order:** R0 insertion (mouth-mediated field graft) → R1 parthenogenesis.

**Variable child cost:** Parthenogenesis runs per-locus entropy (default 3%); operators {dup/del/ins} with literature weights. `[CCAMP]` costs more than `[CAMP]`. Grover amplifies **viable** candidates, not insertions specifically.

**Lorenz strange attractors:** Training curriculum explicitly inspired by bounded chaotic orbits — not hill-climbing to a single optimum.

### 2026-08-28 (e) — Partial topology death cascade (rev 2)

**No free-floating axons.** Death leaves **partial topologies**: dangling axons with one uncapped end. Axons subject to death cascade — both-end-dead axon prunes; one-end-live **draws transit basal from downstream cell** (proposed `kAxonTransitBasalCostPerTick`) accelerating collapse when upstream flow stops.

**HGT dock:** entropy at **uncapped end brushes neuron** — not field blob, not live-live bump. Grover `{P,M,C,A}` floor confirmed for eventual birth collapse.

### 2026-08-28 (g) — PARTHENOGENESIS.md drafted

R1 spec: [PARTHENOGENESIS.md](PARTHENOGENESIS.md) — two-layer entropy (structural gate + mandatory parametric jitter), unified energon-vs-outcome ledger (abort = spent, no child), Grover floor at birth, rub-until-birth test plan.

### 2026-08-28 (h) — R1 parthenogenesis implemented + birth-rub tests

**Implementation:** `OrganismParthenogenesis.cpp` — eligibility (age ≥600, solvency ≥345 600 B hub, wet, valid CAMP, no basal arrears), morphogenesis pipeline (init 864 B + 19×8 B step basal + finalisation to **259 200 B** baseline), Gate 2 parametric jitter (trust, η, bones, register, sense radius, heading), Gate 1 structural dup/del/ins on axon graph (**developmental edges protected** from deletion). Child endowment **172 800 B** (2 fuel-days). Integrated in `CellPopulation` tick after HGT prune.

**Harness:** `tests/unit/test_parthenogenesis.cpp` — tags `[parthenogenesis]`, `[birth_rub]`; `ParthenogenesisPassOptions` exposes `structuralRateOverride` and `skipEligibilityChecks` for oracle parents.

**Results (Release build, seed 42 world):**

| Test | Result | Key metric |
|------|--------|------------|
| Wealthy aged parent spawns faithful camp child | ✅ | `bytesSpent` = **259 200**; parent hub debited exactly; Gate 2 jitter on P→M axon; `offspringSpawnedCount` = 1 |
| Young parent cannot spawn | ✅ | age &lt;600 → **0 B** spent, no child |
| Insolvent parent aborts without spawn | ✅ | partial pipeline debit (**964 B**: full wallet consumed — 864 init + 12×8 step basals + remainder on failed step 13); `aborted` = true |
| 0% structural rate preserves axon count | ✅ | child axons **12/12** (matches parent) |
| 100% structural rate rub (16 trials) | ✅ | **16/16** valid CAMP spawns (`campGenotypeValid`); dup/ins may add axons, del skips `{P,M,C,A}` developmental edges |
| Population tick adds child | ✅ | population 1→2; parent telemetry set |

**Full suite:** **161/161** tests passed (was 156 pre-R1; +5 parthenogenesis cases, 28 assertions in `[parthenogenesis]` filter).

**Interpretation:** Reproductive debit matches design target (259 200 B ≈ 3 fuel-days at 1 B/tick basal duty). Abort path correctly spends init + partial morphogenesis without spawning — unified ledger behaviour. Structural entropy at 100% does not break CAMP viability when developmental axons are immutable (R1 axon-level ops only; full `G_seq` locus operators deferred to R1b).

**Backlog:** 50%/10% structural-rate binomial calibration (mirror death-feast HGT rub); Grover `{P,M,C,A}` floor at birth; inspector HUD for `lastParthenogenesisBytesSpent` / `offspringSpawnedCount`; headless cohort survival with spontaneous parthenogenesis under default tick (no oracle skip).

### 2026-09-02 — Interoception audit, stem dedup, marathon viability

**Interoception closure (P/M/C/A):** All camp neuron intent paths now read organism fuel/state only through gathered interoception at tick time — no side-channel `store.size()` or mutable out-params in intent compute. **A:** locomotion fuel unit/bytes in `ActuatorInteroception`. **C:** hub fuel, satiation, reserve floor, conservation export, distress/mate, vent affordances in `ComputerInteroception`; feed gain and cloaca band from interoception only. **P:** body hunger, scan payment caps, `canAffordMaxScanPayment`, `selfMateReady` via shared body gather. **M:** `mouthChewPaused` in interoception/`FeedIntent`.

**P scan + cloaca centralization:** `perceptorScanPaymentBytes` / `canAffordMaxScanPayment` / `seedPerceptorScanPaymentInteroception`; early scan gate uses worst-case scan+transduction payment. Distress/mate predicates live in `NeuronStem::gatherCampBodyInteroception` (removed P→C header coupling).

**Stem / bind refactor:** Gen-0 camp via `assembleOrganismFromStemPlan()` + `StemAssemblyPlan` replay at parthenogenesis birth. `closeStemNeuralGraphAmongLoci` alone wires canonical 4-locus clique (12 directed axons); redundant post-assembly `ensureCampDevelopmentalAxons` dropped from stem bind path (kept for clone/mutant repair). **Endowment dedup:** single `splitCampStorage` + `endowCampNodesFromSplit` (per-type share split for multi-P/A freaks); parthenogenesis child endowment and `makeRandomCampMutant` route through stem helpers. **Axon helper:** `makeDevelopmentalAxon` consolidated in `OrganismNeuron.hpp`.

**Feedbag / parthenogenesis visibility:** Console log on feedbag spawn; marathon telemetry (`firstBirthTick`, parent/child ids, stroke/scan rates). Parthenogenesis **does** fire at tick **120** for feedbag oracle (parent id=1 → child id=61); easy to miss without log/HUD.

**Marathon (6000 ticks, seed 42, 60 campers):**

| Metric | Value |
|--------|-------|
| Alive | 60 → 43 |
| Seed survivors | ~41/59 |
| Cumulative energon spawns | ~113k |
| Stroke rate | ~25.6% |
| Percept scan rate | ~0.19% |
| First birth | tick 120, feedbag parent |
| Offspring alive end | 1 (feedbag line) |
| Seed-organic offspring (non-feedbag parents) | 0 at 6k; tracked via `seedOrganicBirthCount` in ultra |

**Modularity assessment (~65% for production camp):** Stem owns fuel/bind/endowment/surplus; gather→intent pattern is sound. Remaining duplication: dual-computer test factory manual endowment; unused `kStemBindCooperativeStrength` / `kStemBindAssemblyEpsilonFactor`; parthenogenesis clone-then-bind-replay (often no-op); test factories bypass `StemAssemblyPlan` where morphology is intentionally freakish.

**Population viability framing:** Seed cohort is **metabolically viable** (40+ intact CAMP at tick 600; 30+ survive 6000 ticks) but **reproductively feedbag-led** at marathon horizon — organic seed births need ~270k B hub + age 600 + wet terrain; ultra-marathon gate now tracks `seedOrganicOffspringAlive`. ALife/PhD hook: stem-cell genotype oracle + empirical viability under sunfall/throttle, not hand-authored behaviour trees.

**Next:** R0 HGT dock; trust/RPE on mouth CTA; headless Kaplan-Meier cohort; Grover floor at birth; reduce feedbag-oracle dependence once seed-organic births stable in ultra-marathon.

### 2026-09-02 (b) — Torpedo chain morphology (M→P→C→A, ram nose)

**Literature:** Aquatic prey capture spans a ram–suction spectrum (Norton & Brainerd 1993; Wainwright et al., *J. Exp. Biol.* 2001). Ram suspension feeders swim forward with an open mouth and use forward motion to stream water (and particles) through the oral cavity — distinct from pump/suction feeders that rhythmically expand the buccal cavity (Sanderson & Wassersug 1993; Sanderson et al., Frontiers in Marine Science 2023–2024). For pelagic “tow-net” feeding, the **mouth leads** the body axis; sensors are commonly set back or lateral (e.g. herring, mackerel, paddlefish/basking ram filterers).

**Gen-0 layout:** Colinear **torpedo chain** along `heading`: **M (ram nose) — P — C — A (tail/root)**. Mouth contact is at the foremost node; P sits one segment aft with a forward focus cone (preview corridor before/at ram contact). Actuator at tail; stroke along body heading.

**Stem genotype:** `StemChainRecord` links A→C→P→M; inherited via `StemAssemblyPlan.chains`. Hub-star `binds` retained for legacy test morphologies.

**Validation:** `organismHasCampTorpedoChain()` / skeleton checks; parthenogenesis replays 3 chain bind steps.
