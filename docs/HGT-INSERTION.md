# Horizontal gene transfer & the INSERTION operator

**Status:** R0 implemented (2026-08-28)  
**Scope:** R0 evolutionary closure — **before** parthenogenesis (R1)  
**Companions:** [EVOLUTION.md](EVOLUTION.md), [DESIGN-NOTES.md](DESIGN-NOTES.md) §4, [RESEARCH-LOG.md](RESEARCH-LOG.md)

This document consolidates today’s design conversation into a single evidence-based specification for **horizontal gene transfer (HGT)** and the **INSERTION** genome operator in evo-lab.

---

## 1. Executive summary

| Principle | Decision |
|-----------|----------|
| **Historical order** | HGT before vertical reproduction (pre-LUCA literature) |
| **INSERTION unit** | New **directed axon edge** (and later `G_seq` locus) — not whole neurons |
| **Primary vector** | **Death cascade** → partial topology → **uncapped axon ends** |
| **Entropy location** | **`kAxonDockRate` at dock only** — not live-live bump |
| **Rejected** | Random bump-conjugation; free-floating axon field blobs |
| **Chemical HGT** | Mouth ingests corpse/cloaca bytes → register (H0) — **shipped partial** |
| **Grover** | Floor `{P,M,C,A}` at **birth** collapse only — eventual, not at dock |

**One-sentence model:** Dying Noms leave **broken wiring** (dangling axons) that still metabolically burden survivors; when an **open axon end** brushes a compatible live neuron, chaos may **INSERT** a foreign edge motif — horizontal acquisition of connectivity before reproduction closes the loop.

---

## 2. Biological evidence

### 2.1 HGT predates modern reproduction

| Claim | Source |
|-------|--------|
| Pre-Darwinian communal evolution; horizontal code exchange before stable lineages | Woese “Darwinian threshold”; Vetsigian et al., *Sci. Rep.* 2018 ([doi:10.1038/s41598-018-21973-y](https://doi.org/10.1038/s41598-018-21973-y)) |
| Ancient HGT at/before LUCA; LUCA is coalescence not first life | Fournier & Gogarten, *BMC Evol. Biol.* 2015 ([PMC4427996](https://pmc.ncbi.nlm.nih.gov/articles/PMC4427996/)) |
| Replication–division coupling emerges **after** communal phase | Tang, *Biol. Theory* 2020 ([doi:10.1007/s13752-020-00359-2](https://doi.org/10.1007/s13752-020-00359-2)); Szathmáry & Smith 1997 |
| Communal protocell model of genetic code origin | Vetsigian et al. 2018; Woese 1998/2002 |

**Design consequence:** Ship **R0 INSERTION / HGT** before **R1 parthenogenesis**.

### 2.2 Natural transformation (environmental / corpse DNA)

| Claim | Source |
|-------|--------|
| Bacteria integrate **fragmented, damaged DNA** from environment (≥20 bp) | Overballe-Petersen et al., *PNAS* 2013 ([PMC3856829](https://pmc.ncbi.nlm.nih.gov/articles/PMC3856829/)) |
| Short degraded DNA is evolutionary substrate, not just food | Overballe-Petersen & Willerslev, *BioEssays* 2014 ([doi:10.1002/bies.201400035](https://doi.org/10.1002/bies.201400035)) |
| HGT not limited to live cell–cell contact | BioEssays 2014 review |

**evo-lab mapping:** **H0** — M bites wet energon (corpse spill, cloaca trail, sunfall) → digest → hub/register bytes. Shipped: mouth contact, digest, diet EMA, gag reflex on distress-heavy cloaca (`mouthOutboundConfidence`).

### 2.3 Conjugation is not random bumping

| Claim | Source |
|-------|--------|
| F-plasmid transfer requires **pilus + mating pair stabilization (MPS)** | Atkinson et al., *Nat. Microbiol.* 2022 ([doi:10.1038/s41564-022-01146-4](https://doi.org/10.1038/s41564-022-01146-4)) |
| Plasmids **select partners** (TraN sensor ↔ recipient OMP) before transfer | Gaudin & Lanfranconi, *NAR* 2023 ([doi:10.1093/nar/gkad678](https://doi.org/10.1093/nar/gkad678)) |
| Pilus contact alone insufficient under physiological conditions | Atkinson 2022; Hamilton et al. *Microbiology* 2005 (TraN/OmpA) |

**Design consequence:** **Reject** `chaosBernoulli(ε)` on every live neuron–neuron touch — would force **clumping** and mis-model biology.

### 2.4 Gene insertion / duplication / deletion (vertical operators)

| Operator | Biology | Source |
|----------|---------|--------|
| **Duplication** | Ohno redundancy → neofunctionalisation (rare) | Ohno (1970); Long et al., *Genome Res.* 2010 |
| **Deletion** | Majority of duplicates pseudogenise / lost | Lynch & Conery 2000; Zhang 2013 review |
| **Insertion** | HGT, de novo ORFs, mobile elements | Jain HGT reviews; Kaessmann de novo genes |

**Design consequence:** INSERTION in **parthenogenesis/mating** (R1+) uses ~15% weight among indel events; **R0 INSERTION** is ecological HGT via axon dock, not locus string edit yet.

### 2.5 Sensory hierarchy (supports H0 quality control)

Postingestive confirmation before spatial semantics (CTA analogue):

| Finding | Source |
|---------|--------|
| Delayed postingestive flavour learning | Nature 2025 ([doi:10.1038/s41586-025-08828-z](https://doi.org/10.1038/s41586-025-08828-z)) |
| CGRP/PBN circuits for taste aversion | Sweeney & Yang, *Neuron* 2019 ([PMC6250580](https://pmc.ncbi.nlm.nih.gov/articles/PMC6250580/)) |

P decodes spatial cloaca RGB; M confirms ingestion composition — future C trust compares P prediction vs M outcome.

---

## 3. Genotype model

Twin-string genotype (DESIGN-NOTES §2.3):

```text
G = ( G_seq , G_axon )

G_seq  = developmental locus string over {P, M, C, A}   e.g. [CAMP]
G_axon = directed neural edges (signal + feed channels)
G_skel = mechanical Y-star (derived from G_seq at factory — topology evolution deferred)
```

### 3.1 INSERTION operator (formal)

**R0 (ecological HGT):** INSERTION adds one **directed axon edge** `(srcType → dstType)` with inherited or jittered `(trustBelieve[], trustFeed, η_energy, η_signal)`:

```text
INSERTION(G, edge E) → G'  where G_axon' = G_axon ∪ { E retargeted to live node ids }
```

**Constraints:**

- `src` and `dst` neuron types must exist and be **alive** on recipient
- No duplicate parallel edge unless duplication operator (R1+)
- `|G_axon| ≤ kAxonChannelCapacity` (64)
- Recipient pays `kHgtInsertionCostBytes`

**R1+ (vertical morphogenesis):** INSERTION may also add a **locus** to `G_seq` and spawn neuron + skeleton — deferred until topology co-evolution spec (EVOLUTION.md §9).

### 3.2 Related operators (not R0)

| Operator | Effect | When |
|----------|--------|------|
| **DUPLICATION** | Repeat locus / edge bundle | Parthenogenesis entropy morphogenesis |
| **DELETION** | Remove locus / edge | Death prune, morphogenesis, trust death |
| **DOCK** | Complete dangling half-edge onto neuron | R0 INSERTION mechanism |

---

## 4. Rejected HGT models

| Model | Verdict | Reason |
|-------|---------|--------|
| Live-live **bump conjugation** | ❌ | Clumping; requires TraN/MPS in biology |
| **Free-floating axon blobs** in energon field | ❌ | No metabolism; orphan particles |
| **Instant chimera merge** (two organisms → one tick loop) | ❌ deferred | Endosymbiosis-grade bookkeeping |
| **Grover at dock** | ❌ | Grover marks **birth** candidates, not every attachment |

---

## 5. Approved model: two-layer HGT

```text
┌─────────────────────────────────────────────────────────────┐
│  H0 — CHEMICAL (transformation)          ✅ partial shipped │
│  Corpse/cloaca/sunfall bytes → M → C hub / register         │
│  Quality gate: mouth diet EMA, gag reflex on distress         │
└─────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────┐
│  H1 — PARTIAL TOPOLOGY (death cascade)     ✅ R0 shipped   │
│  killNeuron → dangling axons, not immediate axon delete     │
│  Both-end-dead axon prunes; one-end-live = open stub        │
└─────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────┐
│  H2 — INSERTION via uncapped-end dock        ✅ R0 shipped   │
│  Open axon end near compatible neuron → chaos → cap edge    │
│  Same organism = rewire; foreign organism = true HGT        │
└─────────────────────────────────────────────────────────────┘
```

---

## 6. Death cascade & partial topology

### 6.1 Current behaviour (to change)

```text
killNeuron(N):
  spill N.store → field (Fragment energon)
  removeNeuralAxonsForNode(N)   ← deletes all axons touching N
  N.alive = false; N.neuron = None
```

### 6.2 Target behaviour

```text
killNeuron(N):
  spill N.store → field
  for each axon A incident on N:
    if other_end.alive:
      A.state = DANGLING; A.uncappedNodeId = N.id   // open end at N pose
    else:
      prune A
  N.alive = false; N.neuron = None
  organism survives if any functional neuron remains
```

### 6.3 Phenotypic cascade (`[CAMP]`)

| Stage | Live modules | Conceptual `G_seq` | Axon graph |
|-------|--------------|-------------------|------------|
| Healthy | P,M,C,A | `[CAMP]` | 12 edges, all capped |
| P dead | M,C,A | `[MAC]` | P→* dangling from survivors |
| M dead | C,A | `[CA]` | Feed/crawl crippled; more stubs |
| C dead | A | `[A]` | Hub gone |
| All dead | — | — | Bytes spill; axons pruned |

Same `Organism` id throughout — **partial topology**, not fission into orphan nom records (deferred).

---

## 7. Axon metabolism — transit basal

**Today:** η **payload loss** when bytes move (`applyHopLoss`, ~88% delivered). **No idle axon tax.**

**Proposed:** each live or **dangling** axon debits **`kAxonTransitBasalCostPerTick`** (default **1 B/tick**) from **downstream dst** wallet/hub.

| Condition | Effect |
|-----------|--------|
| Bytes conveyed this tick | η hop loss + transit basal (from src path) |
| Upstream dry, dangling stub | **Still taxes dst** — “feeding on remaining cell” |
| Dst cannot pay | Axon `transitArrearsTicks++`; prune after `kNeuronBasalGraceTicks` (8) |

**Rationale:** Death propagates along the wiring diagram; severed P→M becomes parasitic leak on M until docked or pruned.

**Literature hook:** Neural/axonal energy budgets — Attwell & Laughlin 2001 (DESIGN-NOTES §8.2 cites for hop η; extend to line maintenance).

---

## 8. INSERTION mechanism — uncapped-end docking

### 8.1 When entropy applies

**Only here:**

```text
if distance(uncappedEnd(U), neuron N) ≤ kAxonDockRadius
   and compatibleDock(U, N, organism)
   and chaosBernoulli(kAxonDockRate):      // default 0.03, ∈ Chaos.hpp family
     INSERTION(organism, cap(U) → N)
     pay kHgtInsertionCostBytes from N-side wallet
```

**Not** on: live-live swim-by, neuron-neuron bump, every tick proximity.

### 8.2 Compatibility predicate

```text
compatibleDock(U, N, org):
  typeMatch(U.openEnd expects, N.neuron) OR openSlotPolicy(org, N)
  AND org.axonCount < kAxonChannelCapacity
  AND NOT wouldCreateIllegalCycle()
  AND N.alive
```

### 8.3 Internal vs foreign INSERTION

| Target | Classification |
|--------|----------------|
| Same `Organism`, dangling stub caps to own live neuron | **Rewire** (death recovery) |
| Foreign `Organism` neuron caps dying nom’s stub | **HGT** (horizontal edge acquisition) |
| Scavenger M at corpse + stub brush | **H0 + H2** unified ecology |

### 8.3 Uncapped end position

Use dead node’s last `worldX/worldZ` (or bone extrapolation from live cap). Render: stub visible at partial topology for inspector/debug.

---

## 9. Energon economics

| Event | Cost (proposed) | Pool |
|-------|-----------------|------|
| Neuron basal | 1 B/tick | per-neuron wallet / hub |
| Axon transit basal | 1 B/tick | **dst** downstream |
| Successful INSERTION dock | `kHgtInsertionCostBytes` ≈ **4 B** | dst (2 × `kEnergonRainEntropy`) |
| H0 bite | 1 B chew tax; +8 B net food | M store |
| Failed dock attempt | 0 | — |
| Corpse byte spill | 0 (already dying) | field Fragment |

Global entropy (`kEnergonRainEntropy = 2.0`, `kMisalignmentRate = 0.03`) scales field pressure — HGT is not free.

---

## 10. Grover floor (eventual — birth only)

At **parthenogenesis / mating collapse** (R1+), not at ecological dock:

```text
valid(G) :=
  count(P) ≥ 1 ∧ count(M) ≥ 1 ∧ count(C) ≥ 1 ∧ count(A) ≥ 1
  ∧ parseOK(G)
  ∧ axonTargetsLegal(G)
```

| Epoch | Effect |
|-------|--------|
| Early | Permissive — `[CCAMPAMC]` passes; amplifies diversity |
| Late | Blocks module-less abominations (`[AMP]`, `[CC]`) |
| Never | Does not cap complexity above floor |

Grover amplifies **marked viable** superposition candidates — not insertions specifically (DESIGN-NOTES §5, EVOLUTION.md §4.4).

---

## 11. Tick integration

Insert into existing CAMP order (DESIGN-NOTES §2.5):

```text
perceive → feed → digest+computer → preAdvect → advect
  → metabolise (viability: neuron basal + NEW axon transit basal)
  → convey
  → dockPass (NEW: uncapped-end INSERTION attempts)
  → signal → prune
```

**Death:** `killNeuron` during viability phase transitions axons to partial topology before convey/dock of that tick (order TBD in implementation — dock should see stubs same frame or next).

---

## 12. Constants (proposed)

| Constant | Value | File |
|----------|-------|------|
| `kAxonTransitBasalCostPerTick` | 1 | `CellConstants.hpp` |
| `kAxonDockRadius` | `cellSize × 0.5` | `CellConstants.hpp` |
| `kAxonDockRate` | 0.03 | `Chaos.hpp` (align `kMisalignmentRate`) |
| `kHgtInsertionCostBytes` | 4 | `CellConstants.hpp` |
| `kAxonChannelCapacity` | 64 | existing |
| `kNeuronBasalGraceTicks` | 8 | existing (reuse for axon arrears) |

---

## 13. Implementation phases

### Phase R0a — Partial topology (foundation)

1. Extend `NeuralAxon` or parallel state: `Dangling`, `uncappedNodeId`
2. Replace `removeNeuralAxonsForNode` in `killNeuron` with partial-topology transition
3. Tests: P death leaves M,C,A alive with dangling P→* stubs

### Phase R0b — Axon transit basal

1. `tickAxonTransitBasal(organism)` in viability/metabolise
2. Debit dst; arrears; prune
3. Tests: dangling P→M drains M faster when upstream dry

### Phase R0c — INSERTION dock

1. `findUncappedAxonEnds()` + spatial query vs all neurons
2. `attemptUncappedDock(end, neuron, rng)`
3. Telemetry + inspector strings
4. Tests: foreign dock = new edge; internal dock = rewire; illegal reject

### Phase R0d — H0 completion (optional polish)

1. C register write from foreign byte tags (beyond digest)
2. M→C prediction error / trust on diet mismatch

---

## 14. Test plan

| Test | Pass criterion |
|------|----------------|
| Partial death | P killed; M,C,A alive; dangling axon count > 0 |
| Axon drain | M store falls faster with dangling inbound stub vs intact CAMP |
| Internal dock | Chaos seed forces dock; edge completed; transit basal resumes |
| Foreign HGT | Nom A dying near Nom B; B gains edge motif from A stub |
| Anti-clump | Two healthy Noms overlapping 100 ticks → **zero** docks without dangling ends |
| Capacity | Dock rejected at 64 axons |
| Grover floor | `valid([AMP])` false; `valid([CCAMP])` true (unit test parser) |
| **Death feast rub** | `[death_feast]` — 100%/50%/10% dock rate calibration vs binomial expectation |

---

## 15. Open questions & deferred

| Topic | Status |
|-------|--------|
| `G_seq` locus INSERTION (spawn new neuron) | R1 parthenogenesis morphogenesis |
| Topology co-evolution `(G_seq, G_axon, G_skel)` | EVOLUTION.md §9 |
| Orphan nom fission (`[CA]` as separate organism) | Deferred |
| Active donor conjugation (hub vent + tag) | Future; requires machinery predicate |
| ColonyAxon syntrophy | H3 deferred |
| NEAT vs INSERTION relationship | Research |

---

## 16. Bibliography

| Topic | Citation |
|-------|----------|
| Pre-LUCA communal evolution | Vetsigian et al. (2018) *Sci. Rep.*; Woese (1998/2002) |
| Ancient HGT | Fournier & Gogarten (2015) *BMC Evol. Biol.* |
| Pre-Darwinian → LUCA | Tang (2020) *Biol. Theory*; Szathmáry & Smith (1997) |
| Natural transformation | Overballe-Petersen et al. (2013) *PNAS*; (2014) *BioEssays* |
| Conjugation MPS / TraN | Atkinson et al. (2022) *Nat. Microbiol.*; Gaudin & Lanfranconi (2023) *NAR* |
| Gene duplication | Ohno (1970); Long et al. (2010) *Genome Res.* |
| Duplicate gene loss | Lynch & Conery (2000) |
| Conditioned taste aversion | Sweeney & Yang (2019) *Neuron*; Nature (2025) postingestive CFA |
| Neural energy budget | Attwell & Laughlin (2001) |
| Digital evolution | Ofria & Wilke, Avida (2004) |
| Neuroevolution topology | Stanley & Miikkulainen, NEAT (2002) |
| QIEA / Grover | Han & Kim (2000/2002); Grover (1996); DESIGN-NOTES §5 |

---

## 17. Summary diagram

```text
     [CAMP Nom alive]
            │
            ▼ peripheral bankruptcy
     killNeuron(P) ──► bytes spill (H0 food)
            │
            ├──► dangling P→M, P→C, P→A  (partial topology)
            │         │
            │         ├── transit basal drains M,C,A
            │         │
            │         └── uncapped end brushes scavenger neuron
            │                   │
            │                   ▼ chaosBernoulli(kAxonDockRate)
            │              INSERTION(new edge)  ← R0 HGT
            │
            ▼ eventually all modules dead → corpse field

Separate track (R1): parthenogenesis + Grover valid(G) floor at birth
```

**The INSERTION operator is how foreign connectivity enters the population. Death is how it becomes available. Entropy at the dock is how it stays rare. Grover at birth is how children without a chance never collapse into reality.**
