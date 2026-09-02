# Evolution rollout — design, costs, literature

**Status:** Active design (2026-08-28)  
**Companion:** [DESIGN-NOTES.md](DESIGN-NOTES.md) §4, [RESEARCH-LOG.md](RESEARCH-LOG.md)

This document tracks the path from **proto-agency** to **evolutionary closure**: **R0 insertion (HGT)** first, then parthenogenesis, mating, geography-scaffolded training. Operators: insertion / duplication / deletion on `(G_seq, G_axon)`.

---

## 1. Sensory hierarchy — taste before vision

Perceptor RGB cloaca decoding is **downstream technology** in biological terms. The first interface with foreign energon is the **mouth** (taste / postingestive feedback), which can trigger gag reflex and conditioned aversion long before spatial colour semantics stabilise in evolution.

### 1.1 Shipped (2026-08-28)

| Layer | Role | Implementation |
|-------|------|----------------|
| **P** | Spatial pre-conscious scan; cloaca band → approach/flee | `OrganismPerceptor.cpp`, `CloacaSignal` |
| **M** | Postingestive diet EMA; gag reflex on distress-heavy diet | `recordMouthDietBite`, `mouthOutboundConfidence` |
| **C** | Compare P vs M valence for CTA RPE / dispatch + believe trust | `OrganismComputer.cpp`, `NeuronTrust.cpp` |

M→P/C/A axons now emit **fuel/diet satiation** (not fuel alone): distress-cloaca-heavy diet caps outbound at ≤2 even when the local store is full — conditioned taste aversion analogue.

**Literature (CTA / postingestive):**

| Finding | Source |
|---------|--------|
| Single pairing can link taste to delayed malaise (long-delay credit assignment) | Garcia & Koelling tradition; review in Nature 2025 postingestive CFA mechanism ([doi:10.1038/s41586-025-08828-z](https://doi.org/10.1038/s41586-025-08828-z)) |
| Parabrachial CGRP neurons necessary/sufficient for CTA acquisition and expression | Carter et al., *J. Neurosci.* 2015 ([PMC4391242](https://www.ncbi.nlm.nih.gov/pmc/articles/PMC4391242/)); Sweeney & Yang, *Neuron* 2019 ([PMC6250580](https://pmc.ncbi.nlm.nih.gov/articles/PMC6250580/)) |
| Gustatory cortex population codes shift toward aversive quadrant after CTA | Frontiers Systems Neuroscience 2026 ([doi:10.3389/fnsys.2026.1765204](https://doi.org/10.3389/fnsys.2026.1765204)) |

**Design intent:** P signals “there is blue distress ahead”; M confirms “I am eating mostly blue” → low M→* outbound → C can learn prediction error on P→C vs M→C divergence (future trust on believe channel).

---

## 2. Environmental factors — fuel in the tank

Geography, energon availability, and entropy drive selection. Both energon rain and basal burn **scale with population** (`EnergonRain.hpp`), so geography training is about **spatial structure**, not hand-tuned global food.

### 2.1 Phased geography rollout (“rat training ladder”)

| Phase | World | Purpose | Literature anchor |
|-------|-------|---------|-------------------|
| **E0** | Static seed **42** | Reproducible baseline, regression harness | Standard ALife practice (Avida, Polyworld control runs) |
| **E1** | **8 magic seeds** (fixed set) | Generalisation across geographies | Domain randomisation in RL/ecology ([Tobin et al., 2017](https://arxiv.org/abs/1703.06907) — sim-to-real via varied envs) |
| **E2** | Random seed every run | Robustness; prevent overfit to 8 worlds | — |
| **E3** | **Mazes** — energon only in pockets | Foraging + memory pressure | Classic chemotaxis mazes (*E. coli* capillary assays; Adler, 1966) |
| **E4** | **Button-released** energon bursts | Operator-controlled feast/famine | Laboratory rat operant conditioning analogue |

Energon and entropy remain **systemic** throughout; only geography and release schedule change.

### 2.2 Chaos at epoch boundaries (deferred)

On app close: save **bottom 18%** and **top 18%**; on startup re-seed population from archive + fresh “algae” campers. High mortality + environmental chaos can accelerate convergence (author’s masters thesis — **to be cited when digitised**; see §2.3 for peer-reviewed analogues).

**Literature (environmental stochasticity — nuanced):**

| Claim | Evidence |
|-------|----------|
| Stochasticity alters eco-evolutionary attractors vs deterministic models | Schmidt et al., *Ecology* 2023 ([doi:10.1002/ecy.3873](https://doi.org/10.1002/ecy.3873)) |
| Environmental fluctuations can **help** evolutionary rescue in high-extinction-risk scenarios | Galloway et al., *Proc. R. Soc. B* 2020 ([PMC7575515](https://pmc.ncbi.nlm.nih.gov/articles/PMC7575515/)) |
| Quasi-periodic fluctuations can **hinder** rescue vs periodic ones | Khuriyakova et al., *Proc. R. Soc. B* 2023 ([doi:10.1098/rspb.2023.0770](https://doi.org/10.1098/rspb.2023.0770)) |

**Design stance:** ε-chaos at init/mating/epoch (`Chaos.hpp`) is mandatory; **macro** geography randomisation is the training curriculum; mortality is a feature, not a bug — but we log survival curves (`RESEARCH-LOG`) to distinguish “edge of chaos” from “instant extinction.”

### 2.3 Lorenz strange-attractor inspiration (author note)

The geography ladder, epoch 18%/18% archive, and ε-chaos at every boundary are **explicitly inspired by Lorenz-type strange attractors** — sensitive dependence, non-equilibrium dynamics, and trajectories that never settle but remain bounded. Training is not “converge to one optimum”; it is **orbit a viable region** of phenotype–environment space while entropy drains the unfit.

| Lorenz idea | evo-lab mapping |
|-------------|-----------------|
| Sensitive dependence on initial conditions | `chaosSpawnRng`, seed-42 vs 8-seed generalisation ladder |
| Bounded non-periodic orbit | Population persists but never frozen; clades wax/wane |
| Strange attractor ≠ equilibrium | Schmidt et al. NEEAs — stochastic ecology changes evolutionary attractors ([doi:10.1002/ecy.3873](https://doi.org/10.1002/ecy.3873)) |
| Parameter exploration on chaotic surface | Evolutionary identification of chaotic systems (Lorenz benchmark) — Zelinka et al., AIP 2011 ([doi:10.1063/1.3592478](https://doi.org/10.1063/1.3592478)) |

This is **design philosophy**, not a literal Lorenz ODE in the sim loop — but the training curriculum should feel like riding an attractor, not climbing a hill.

### 2.4 Global entropy (shipped constants)

Entropy is **systemic**, not a parthenogenesis-only tax:

| Layer | Constant | Value | Effect |
|-------|----------|-------|--------|
| Field rain budget | `kEnergonRainEntropy` | **2.0×** | Sunfall must exceed idle duty × population × 2 to break even (`EnergonRain.hpp`) |
| Spawn jitter | `kChaosJitterRate` | **±3%** | Trust, bone length, storage, heading |
| Crossover/splice | `kMisalignmentRate` | **3%** | Genotype misalignment at mating / morphogenesis |
| Stroke / conveyance | `kActuatorTranslationEta`, axon η | **~12%** motion | Remainder = translation entropy (heat) |
| Blob TTL | energon decay | per blob | Uneaten field bytes dissolve |

**Parthenogenesis adds a morphogenetic entropy pass** on top — each locus in the child assembly pipeline is a step where chaos can fire (§4.3).

---

## 3. Genome — topological string algebra

Camper genotype is a **developmental locus string** over `{P, M, C, A}`:

- Baseline: **`[CAMP]`** (developmental order P→M→C→A on the Y-star).
- Future: **`[CAMPAMC]`** etc. via duplication / insertion / deletion on loci **and** parallel axon graph.

### 3.1 Operators (evidence check)

| Operator | Genotype example | Axon graph | Biological analogue | Evidence |
|----------|------------------|------------|---------------------|----------|
| **Duplication** | `[CAMP]` + `[CAMP]` → `[CCAMP]` | Duplicate edge bundle | Ohno redundancy → neofunctionalisation | Ohno (1970) *Evolution by Gene Duplication* ([Springer](https://doi.org/10.1007/978-3-642-86659-3)); Long et al., *Genome Res.* 2010 ([doi:10.1101/gr.101139.109](https://genome.cshlp.org/content/20/10/1313)) |
| **Deletion** | `[CAMP]` + `[CAMP]` → `[AMP]` | Remove locus + incident axons | Pseudogenisation; majority of duplicates lost | Lynch & Conery 2000; Zhang 2013 review |
| **Insertion** | HGT / frameshift tolerance | New edge from junk locus `∅` | Horizontal gene transfer; de novo ORFs | Jain et al. HGT reviews; Kaessmann on de novo genes |

**Insertion / HGT:** See **[HGT-INSERTION.md](HGT-INSERTION.md)** — authoritative R0 spec (partial topology, uncapped-end dock, evidence).

### 3.2 HGT vs reproduction — which came first? (literature review)

**Question:** Should we ship parthenogenesis before horizontal gene transfer?

**Answer (dominant origin-of-life hypothesis): gene acquisition (communal horizontal exchange) predates modern reproduction.**

| Era | Mode | Evidence |
|-----|------|----------|
| **Pre-Darwinian / pre-LUCA** | Communal protocells exchange code fragments; no stable vertical lineages | Woese “Darwinian threshold”; Vetsigian et al., *Sci. Rep.* 2018 ([doi:10.1038/s41598-018-21973-y](https://doi.org/10.1038/s41598-018-21973-y)) |
| **Pre-LUCA** | HGT among progenotes; LUCA is late coalescence, not first life | Fournier & Gogarten, *BMC Evol. Biol.* 2015 ([PMC4427996](https://pmc.ncbi.nlm.nih.gov/articles/PMC4427996/)) |
| **Transition** | Tight coupling of replication + division emerges **after** communal phase | Tang, *Biol. Theory* 2020 ([doi:10.1007/s13752-020-00359-2](https://doi.org/10.1007/s13752-020-00359-2)); Szathmáry & Smith 1997 major transitions |
| **Post-LUCA** | Vertical descent + HGT both; reproduction is the scaffold | Modern microbial evolution |

**evo-lab mapping:** Mouth→digest already moves foreign bytes into hub/register (proto-HGT). **Insertion operator** = field/corpse/cloaca string graft into genotype or register **before** we treat parthenogenesis as the primary closure mechanism.

**Revised rollout order (§3.4):**

1. **R0 — Field insertion (HGT scaffold):** corpse/cloaca energon at mouth → computer register / axon tag graft (horizontal acquisition).
2. **R1 — Parthenogenesis:** vertical child split with locus entropy (duplication/deletion on inherited string).
3. **R2 — Mating + QIEA/Grover collapse** at two-parent boundaries.
4. **R3 — Epoch archive** (18%/18%).

Parthenogenesis alone would skip the historically prior horizontal layer; R0 is the minimal fix.

### 3.3 Insertion operator (horizontal transfer)

Treat insertion as **mouth-mediated uptake** of foreign energon semantics into the child's developing genotype — not teleportation of whole topologies.

| Source | Mechanism | Cost |
|--------|-----------|------|
| Corpse spill | Field bytes at mouth | bite tax + digest |
| Cloaca trail | Tagged bytes (B1/E2/F3) | gag reflex if distress-heavy |
| Neighbour axon fragment | Future: contact transfer | η loss + insertion surcharge |

Insertion surcharge adds to variable child cost (§4.3). High mortality expected — most grafts pseudogenise (Lynch/Ohno).

### 3.4 Recombination order (revised)

See §3.2. Mating deferred until R1 parthenogenesis telemetry is honest.

---

## 4. Parthenogenesis — triggers, entropy morphogenesis, variable cost

**Authoritative spec:** **[PARTHENOGENESIS.md](PARTHENOGENESIS.md)** — two-layer entropy (structural gate + parametric jitter), unified energon-vs-outcome ledger, Grover floor, test plan.

Summary:

### 4.1 Triggers (agreed design)

Reproduction is **gated by time + fuel**, not a button — surviving to reproduce implies the child inherits a viable trajectory.

| Gate | Threshold | Rationale |
|------|-----------|-----------|
| **Age** | ≥ `kMateMinAgeTicks` (**600**) | ~⅓ visual day; reuse mate-maturity clock |
| **Solvency** | Hub + peripherals can pay **variable child bill** (§4.3) + parent reserve | Metabolic proof of viability |
| **Topology** | Parent parses as valid developmental string | No split from corrupted hub |
| **Geography** | Wet tile, spawn clearance | Tide oracle |
| **No arrears** | P,M,A,C basal grace not exhausted | Peripheral bankruptcy blocks reproduction |

When all gates pass: enter **morphogenesis pipeline** (§4.3). Parent pays energon **during** assembly, not from a fixed menu.

### 4.2 Baseline `[CAMP]` cost (reference anchor only)

The **259k B / 346k B** figures in earlier notes are the **minimal** `[CAMP]` case (4 loci, 12 axons). They are **not** fixed — see §4.3.

| Component (baseline) | Bytes |
|----------------------|-------|
| Endowment (median 2 fuel-days) | 172,800 |
| Construction (1 fuel-day tax) | 86,400 |
| Parent reserve after | ≥ 86,400 |
| **Baseline total debit** | **~259k B** (plus reserve) |

Factory spawn overhead today remains **0 B**; parthenogenesis is the first structure creation debit.

### 4.3 Entropy morphogenesis — variable child cost

During parthenogenesis, **each locus step** in the child assembly pipeline rolls against global entropy (`kMisalignmentRate` = **3%** default, linked to `kChaosJitterRate` family).

**Per-locus algorithm (proposed):**

```text
for each locus L in parent genotype (assembly order):
  parent pays kParthenogenesisStepBasalCost   // ongoing entropy bleed during cytokinesis
  if chaosBernoulli(kParthenogenesisLocusEntropyRate):   // default 0.03
    op ← draw {Duplication, Deletion, Insertion} with literature weights
    apply op at L → update child genotype + axon graph
    parent pays kParthenogenesisOperatorSurcharge[op]
```

**Operator weights (evidence-based priors):**

| Operator | Conditional weight | Literature |
|----------|-------------------|------------|
| **Deletion** | ~50% of events | Most duplicates/loci lost (Lynch & Conery 2000) |
| **Duplication** | ~35% of events | Ohno redundancy; neofunctionalisation rare |
| **Insertion** | ~15% of events | HGT/macromutation; higher surcharge |

**Example (author scenario):** Parent `[CAMP]`, entropy ε = 3% hits locus **A** → duplication → child `[CAMP]` with duplicated A arm → **`[CCAMP]`** if C also duplicated, or **`[CAMPA]`** depending on insertion site rules (TBD). Parent pays **extra construction** for additional locus + axon bundle replication.

**Variable cost formula (proposed):**

```text
childCost =
  kEndowmentBytesPerLocus × nLoci
+ kConstructionBytesPerLocus × nLoci
+ kAxonConstructionBytes × nAxons
+ kInsertionSurcharge × nInsertionsApplied
+ Σ kParthenogenesisStepBasalCost   (one per assembly step)
+ entropyHeat (linked to kEnergonRainEntropy scale during split ticks)
```

| Child | nLoci | Relative cost vs `[CAMP]` |
|-------|-------|---------------------------|
| `[CAMP]` | 4 | 1.0× (baseline) |
| `[CCAMP]` | 5 | ~1.25× endowment + 1 extra neuron basal duty |
| `[AMP]` | 3 | ~0.75× endowment but higher viability risk |

**Key property:** parent solvency gate uses **`estimateParthenogenesisCost(parent, rng)`** — pre-roll or commit-to-pay on actual drawn child after morphogenesis completes.

### 4.4 Quantum-inspired child selection (Grover / QIEA)

**Preferred technique (author):** Grover-style amplification at **genotype collapse boundaries** — see DESIGN-NOTES §5.

| Question | Answer |
|----------|--------|
| Does Grover favour insertions/deletions? | **No.** It amplifies **marked viable** candidates in superposition — whichever genotype passes `valid(G)` and (optionally) short oracle survives. |
| **Minimum floor oracle (author)** | **`valid(G)` requires ≥1 P, ≥1 M, ≥1 C, ≥1 A** — the non-duplicated set `[CAMP]`. No child without a chance. Early epochs: floor is permissive → duplications/insertions that preserve floor survive. Late epochs: floor prevents **abominations** (missing module types); complexity can still grow via dup/ins **above** the floor. Interesting quantum-evolution question: floor + amplification biases toward richer superpositions without forbidding them. |
| Where do indels come from? | **Entropy morphogenesis** (§4.3) and mating crossover — classical stochastic operators. |
| When is Grover used? | (a) Seed batch covering search; (b) **optional** — if morphogenesis draws K candidate children before collapse, Grover amplifies viable ones; (c) two-parent mating collapse. |
| QIEA role | Rotate qubit amplitudes per locus/edge **between** sim windows; classical CPU unless register justifies QPU. |

Grover does **not** replace the tide tank oracle. It biases **who gets a ticket into reality** at the small register; the tank decides who keeps it.

**Parthenogenesis v1:** classical entropy morphogenesis only (single child draw). **v2:** K superposed candidates + Grover pick if profiling shows benefit.

### 4.5 Ongoing metabolism reference

| Mode | Burn/tick | Source |
|------|-----------|--------|
| Idle regulated | **6 B/tick** | 4 basal + P scan + transduction |
| Horror crawl | **8 B/tick** | +2 stroke |
| Net food bite | **+8 B/chew** | `kBiteNetYieldBytes` |

Median spawn (172,800 B) at 6 B/tick idle ≈ **0.33 fuel-days** runway without feeding — reproduction remains a luxury good.

---

## 5. Mating (Phase R2 reproduction)

Deferred until parthenogenesis telemetry stable. Parent-biased QIEA collapse (§4.3 DESIGN-NOTES); `ε_random_mate` = 0.05.

Duplication/deletion examples from author:

```text
[CAMP]₁ + [CAMP]₂ → [CCAMP]₃   (C duplicated)
[CAMP]₁ + [CAMP]₂ → [AMP]₃     (C deleted)
```

Validate against Ohno/Lynch: most duplications pseudogenise; neofunctionalisation is rare — **macromutation rate** `kMacromutationRate` = 0.001.

---

## 6. Reference bibliography (evolution rollout)

| Topic | Citation |
|-------|----------|
| Digital evolution / energy merit | Ofria & Wilke, Avida (2004) |
| Neural ALife + metabolism | Yaeger, Polyworld |
| Neuroevolution topology | Stanley & Miikkulainen, NEAT (2002) |
| Gene duplication | Ohno (1970); Long et al. (2010) *Genome Research* |
| Duplicate gene fates | Lynch & Conery (2000) |
| Conditioned taste aversion | CGRP/PBN circuit — Sweeney & Yang (2019) |
| Postingestive flavour learning | Nature (2025) [doi:10.1038/s41586-025-08828-z](https://doi.org/10.1038/s41586-025-08828-z) |
| Environmental stochasticity & evolution | Schmidt et al. (2023) *Ecology* |
| Evolutionary rescue & fluctuations | Galloway et al. (2020) *Proc. R. Soc. B* |
| Bacterial division energetics | Stouthamer (1977); flux models — Pramanik & Keasling (1997) |
| Chemotaxis run-tumble | Berg & Brown (1972); eLife flagellar cost (77266) |
| Natural transformation (dead DNA) | Overballe-Petersen et al. (2013) *PNAS*; (2014) *BioEssays* |
| Conjugation requires MPS / partner choice | Atkinson et al. (2022) *Nat. Microbiol.*; Gaudin & Lanfranconi (2023) *NAR* |
| Pre-Darwinian → LUCA transition | Tang (2020) *Biol. Theory* |
| Lorenz chaos / evolutionary identification | Lorenz (1963); Zelinka et al. (2011) AIP |
| QIEA | Han & Kim (2000, 2002) |
| Grover search | Grover (1996); DESIGN-NOTES §5 |

---

## 7. Implementation checklist

| Item | Status |
|------|--------|
| Mouth diet EMA + gag reflex outbound | ✅ shipped |
| R0 partial topology + uncapped-end dock | ⬜ [HGT-INSERTION.md](HGT-INSERTION.md) |
| ~~R0 free axon field fragments~~ | ❌ superseded |
| ~~R0 contact conjugation~~ | ❌ rejected |
| Topology co-evolution (`G_seq`/`G_axon`/`G_skel`) | ⬜ deferred §9 |
| Parthenogenesis constants + variable cost estimator | ⬜ proposed |
| Entropy morphogenesis pipeline | ⬜ proposed |
| `spawnParthenogenesisChild()` factory | ⬜ |
| Survival / reproduction telemetry | ⬜ |
| Grover K-candidate collapse (optional v2) | ⬜ research |
| Static-42 + 8-seed generalisation harness | ⬜ |
| Epoch 18%/18% archive | ⬜ |
| Mating / QIEA collapse | ⬜ R2 |

---

## 8. On-track questions

1. **Can a camper accumulate enough fuel for `estimateParthenogenesisCost()` on a `[CCAMP]` draw?** Duplication should hurt — if not, raise operator surcharges.
2. **Does R0 insertion (corpse register graft) precede viable parthenogenesis in practice?** Align sim with pre-LUCA ordering.
3. **Does distress-heavy diet reduce crawl via M→A brake faster than P learns to avoid blue?** Trust telemetry on P→A vs M→A.

---

## 9. Topology evolution — complex topic (deferred deep-dive)

**Status:** Notes only. Do not implement until R0 insertion telemetry exists.

Evolution must co-evolve **three coupled graphs**:

| Graph | Encodes | Today |
|-------|---------|-------|
| **`G_seq`** | Developmental locus string `{P,M,C,A}*` | Fixed `[CAMP]` at factory |
| **`G_axon`** | Directed neural edges (signal + feed) | Fixed 12 axons (all pairs) |
| **`G_skel`** | Mechanical Y-star bones + `muscleBundle` flags | **Stem assembly plan** — `Organism::stemAssembly` (loci + bind records); world socket grammar in [WORLD-BINDING-GRAMMAR.md](WORLD-BINDING-GRAMMAR.md) |

**Open problems:**

- Duplicating locus **C** in `G_seq` without axon retargeting → dangling or illegal edges.
- NEAT-style (Stanley & Miikkulainen 2002) add-node/add-edge vs our **insertion** operator — are they the same or is insertion HGT and NEAT mutation separate channels?
- **ColonyAxon** (`Organism.hpp`) already unions organisms for population stats — chimera / meta-organism semantics TBD.
- Pruning (`axonMarkedForPruning`) vs deletion operator — structural loss vs trust death.
- Kinematics: new bones from new loci; render-only vs FK edges (KINEMATICS.md bundle gaps).

**Working rule:** `G_seq` drives **which neurons exist**; `G_axon` drives **who talks to whom**; **`G_skel` is the inherited stem assembly plan** (bind records + loci) executed against **world binding physics** (hub socket count/spacing/glue — not per-type genetics). Gen-0 `[CAMP]` is one fixed plan (`defaultCampStemAssemblyPlan()`); parthenogenesis replays bind steps during morphogenesis. See [WORLD-BINDING-GRAMMAR.md](WORLD-BINDING-GRAMMAR.md).

**Shipped (2026-09-02):** World binding grammar + stem bind replay at factory and parthenogenesis; factory-equivalent Y-star for canonical camp.

---

## 10. R0 INSERTION / HGT

**Authoritative spec:** **[HGT-INSERTION.md](HGT-INSERTION.md)**

Summary:

- **H0** chemical transformation via mouth (partial shipped)
- **H1/H2** death cascade → partial topology → uncapped-end **INSERTION** (implement)
- **Rejected:** bump conjugation, free-floating axon blobs
- **Grover** `{P,M,C,A}` floor at birth only (eventual)

Implementation phases: R0a partial topology → R0b axon transit basal → R0c dock → R0d H0 polish.

---

## 11. Evolutionary biology perspective

See **[HGT-INSERTION.md](HGT-INSERTION.md)** §2 and §17 for full citations. In brief:

- **Partial topology** = decomposing cell leaves broken but coupled processes briefly — not autonomous wires in the water.
- **Dock at open end** = stochastic integration at a broken interface (transformation/conjugation hybrid) without live-live pilus on every touch.
- **Transit basal** couples graph to metabolism — death propagates along edges.
- **Grover at birth** only; docking is classical entropy ecology.

---

## 12. Checklist (evolution rollout)

| Item | Status |
|------|--------|
| Mouth diet / H0 chemical layer | ✅ shipped |
| R0 partial topology + uncapped-end dock | ⬜ [HGT-INSERTION.md](HGT-INSERTION.md) |
| R1 parthenogenesis + Grover floor | ⬜ [PARTHENOGENESIS.md](PARTHENOGENESIS.md) |
| Geography / Lorenz curriculum | ⬜ §2 |
| Topology co-evolution `(G_seq, G_axon, G_skel)` | ⬜ §9 |
