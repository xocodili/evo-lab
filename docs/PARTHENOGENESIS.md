# Parthenogenesis — vertical reproduction & two-layer entropy

**Status:** Design specification (2026-08-28)  
**Scope:** R1 evolutionary closure — **after** R0 HGT ([HGT-INSERTION.md](HGT-INSERTION.md))  
**Companions:** [EVOLUTION.md](EVOLUTION.md) §4, [DESIGN-NOTES.md](DESIGN-NOTES.md) §4, [MARKETING-COMMS.md](MARKETING-COMMS.md)

This document specifies **asexual reproduction** in evo-lab: when a long-lived, well-fuelled CAMP camper splits into a child copy of itself, passing through **two layers of entropy** — structural operators first, parametric jitter always — with all outcomes framed as **energon spent vs offspring gained**.

---

## 1. Executive summary

| Principle | Decision |
|-----------|----------|
| **Historical order** | R0 HGT shipped first; R1 closes vertical inheritance |
| **Trigger** | Age + solvency + geography + valid parent — not a button |
| **Clone unit** | Full `(G_seq, G_axon, G_skel)` + instantiated phenotype |
| **Gate 1 — structural** | Per element: faithful copy **or** {DUPLICATION, DELETION, INSERTION} (~3%) |
| **Gate 2 — parametric** | **Always** on every copied element: ±3% jitter on numeric variables |
| **Outcome frame** | Single ledger: **bytes debited vs child spawned** — no separate “stillbirth taxonomy” |
| **Grover** | `valid(G) ≥ {P,M,C,A}` at **birth collapse** only (v1 classical; v2 K-candidate) |
| **Cost** | **Variable** by drawn child complexity — parent pre-authorises solvency band |

**One-sentence model:** A camper that has survived the tide long enough and hoarded enough energon may pay for **cytokinesis** — cloning its flux-capacitor topology locus by locus and axon by axon; entropy may rewrite structure at each step, then **always** whispers ±3% into every trust, η, bone, and register byte; if the running bill exceeds what the parent can pay, or the assembled child fails the viability floor, the energon is **spent anyway** and **no one spawns**.

---

## 2. Biological evidence

### 2.1 Vertical reproduction after horizontal exchange

| Claim | Source |
|-------|--------|
| Communal HGT predates stable vertical lineages | Woese; Vetsigian et al., *Sci. Rep.* 2018 ([doi:10.1038/s41598-018-21973-y](https://doi.org/10.1038/s41598-018-21973-y)) |
| Replication–division coupling emerges **after** communal phase | Tang, *Biol. Theory* 2020 ([doi:10.1007/s13752-020-00359-2](https://doi.org/10.1007/s13752-020-00359-2)) |
| Major transitions = information copied with error | Szathmáry & Smith 1997 |

**Design consequence:** R1 parthenogenesis follows R0 ecological INSERTION.

### 2.2 Parthenogenesis & developmental cloning

| Claim | Source |
|-------|--------|
| Asexual reproduction produces **near-clonal** offspring with **developmental variation** | Jablonka & Lamb, *Evolution in Four Dimensions* (2005) — epigenetic / phenotypic variation without sequence change |
| Isogenic individuals differ phenotypically (developmental noise) | Lewontin 1966; Waddington canalization tradition |
| Apomictic / automictic parthenogenesis — offspring from unfertilised gamete lineage | Suomalainen et al., *Cytology and Evolution in Parthenogenesis* (1987); Normark 2003 review |

**evo-lab mapping:** Gate 1 = rare structural change; Gate 2 = **mandatory** developmental jitter on faithful copies.

### 2.3 Structural mutation rates (Gate 1 priors)

| Operator | Biology | Source |
|----------|---------|--------|
| **Duplication** | Ohno redundancy; neofunctionalisation rare | Ohno (1970); Long et al., *Genome Res.* 2010 |
| **Deletion** | Majority of duplicates / loci lost | Lynch & Conery 2000 |
| **Insertion** | Macromutation, mobile elements | Jain HGT reviews; `kMacromutationRate` tier |

**Conditional weights** (given Gate 1 fires): ~50% deletion, ~35% duplication, ~15% insertion ([EVOLUTION.md](EVOLUTION.md) §4.3).

### 2.4 Reproduction as costly life-history trade

| Claim | Source |
|-------|--------|
| Reproductive effort traded against somatic maintenance | Stearns, life-history theory review (1992) |
| Large offspring size / cost selects for proven parental condition | Smith & Fretwell 1974 parent–offspring conflict model |

**evo-lab mapping:** ~259k B baseline debit for `[CAMP]` — reproduction is a **luxury good**; only well-fed survivors attempt it.

### 2.5 Digital evolution analogues

| System | Structural vs parametric | Source |
|--------|--------------------------|--------|
| **Avida** | Copy errors vs neutral instruction drift | Ofria & Wilke (2004) |
| **NEAT** | Topology mutation rare; weight perturbation common | Stanley & Miikkulainen (2002) |
| **QIEA / Grover** | Amplify marked viable candidates at collapse | Han & Kim; DESIGN-NOTES §5 |

---

## 3. Genotype & phenotype clone model

Twin-string genotype (DESIGN-NOTES §2.3):

```text
G = ( G_seq , G_axon , G_skel derived )

G_seq  = developmental locus string   e.g. [CAMP]
G_axon = directed edges with (trust[], trustFeed, η_signal, η_energy)
G_skel = Y-star bones from factory rules per locus
```

**Parthenogenesis** copies parent `G` through the morphogenesis pipeline → instantiates child organism via existing factory paths (`makeCampNomOrganism` generalised to `developFromGenotype(G, wx, wz, endowment)`).

Baseline anchor: **`[CAMP]`** — 4 loci, 12 axons, 3 links.

---

## 4. Two-layer entropy

### 4.1 Gate 1 — structural (discrete)

At each **clone step** (locus, axon edge, skeleton link):

```text
if chaosBernoulli(kParthenogenesisStructuralRate):          // default 0.03
  op ← draw {Deletion, Duplication, Insertion}  (weighted)
  apply op → update child G_seq / G_axon / G_skel
  parent pays kParthenogenesisOperatorSurcharge[op]
else
  faithful topological copy
```

| Element | Faithful copy | Gate 1 ops |
|---------|---------------|------------|
| Locus | Copy type char | Dup / del / ins locus |
| Axon | Copy edge + retarget node ids | Dup / del / ins edge |
| Link | Copy bone | Add/remove with locus |

Structural INSERTION at birth differs from R0 ecological INSERTION: here it may add locus + neuron + axon bundle (topology co-evolution — [EVOLUTION.md](EVOLUTION.md) §9).

### 4.2 Gate 2 — parametric (continuous, always)

**Every** element that survives Gate 1 (including faithful copies) passes through jitter:

```text
for each numeric field F in cloned element:
  F' ← chaosJitter(F)     // ±kChaosJitterRate (0.03), clamped to valid range
```

| Field | Jitter function | Existing code |
|-------|-----------------|---------------|
| Axon `trustBelieve[]`, `trustFeed` | `chaosJitterTrust` | `NeuralAxon.cpp` |
| Axon `η_signal`, `η_energy` | `chaosJitterFloat` | `Chaos.hpp` |
| Link `restLength`, `jointAngle`, `energyEta` | `chaosJitterFloat` | `finalizeSpawn` pattern |
| `senseRadiusFactor` | `chaosJitterFloat` | `Organism::finalizeSpawn` |
| `computerRegister[i]` | ±1 byte chaos | `finalizeSpawn` |
| `heading` | `chaosJitterHeading` | `Chaos.hpp` |
| Endowment split | policy from parent, jitter optional | TBD |

**Design intent:** Parent and child are **never identical** even when `G_seq` and `G_axon` topology match. Gate 2 is developmental slop, not macromutation.

### 4.3 Entropy location summary

| Event | Gate 1 | Gate 2 |
|-------|--------|--------|
| Parthenogenesis clone step | ✅ ~3% | ✅ always |
| Factory spawn (today) | — | ✅ `finalizeSpawn` |
| Ecological HGT dock (R0) | ✅ ~3% at dock only | inherit stub motif |
| Live tick | ❌ | ❌ |

---

## 5. Energy vs outcome — unified ledger

**Do not split “structural stillbirth” vs “energon stillbirth” as separate product categories.** They are **transiently dependent**: a deletion at locus 3 may only reveal inviability **after** loci 1–2 were already paid for.

Single accounting model:

```text
runningDebit = 0
for each morphogenesis step:
  runningDebit += stepCost
  if parent.pool < runningDebit + parentReserveMin:
    ABORT  → bytes spent, no child
  apply Gate 1 (+ surcharge if op)
  apply Gate 2 (free — entropy already in step basal)

if not valid(childG):
  ABORT  → bytes spent, no child   // "cost no one would pay" for a broken assembly

runningDebit += finalisationCost(endowment for child)
if parent.pool < runningDebit + parentReserveMin:
  ABORT

SPAWN child with endowment; parent retains reserve
```

| Outcome | Energon | Child |
|---------|---------|-------|
| **Spawn** | `runningDebit` paid | 1 live camper |
| **Abort** | `runningDebit` paid (partial pipeline) | none |

Telemetry (debug): log `bytesSpent`, `stepsCompleted`, `abortReason` — for research, not player-facing taxonomy.

**Author framing:** A structurally inviable draw (e.g. `[AMP]`) is energon spent chasing an assembly **no solvent parent would finish** if previewed; in practice the pipeline aborts at `valid(G)` collapse and the spent bytes are the price of failed cytokinesis.

---

## 6. Reproduction gates (eligibility)

Reproduction is **gated by time + fuel + place**, not entropy.

| Gate | Threshold | Constant (proposed) |
|------|-----------|---------------------|
| **Age** | ≥ 600 ticks (~⅓ visual day) | `kParthenogenesisMinAgeTicks` (= `kMateMinAgeTicks`) |
| **Solvency** | Can pay **estimated upper bound** of child bill + reserve | `estimateParthenogenesisCostMax(parent)` |
| **Topology** | Parent `valid(G_parent)` | parser |
| **Geography** | Wet tile, spawn clearance | tide oracle |
| **No arrears** | No neuron in basal grace exhaustion | `basalArrearsTicks < kNeuronBasalGraceTicks` |
| **Intent** | Optional: hub satiation band / mate-ready cloaca (future) | deferred |

When all pass: parent enters **morphogenesis** state for `N` ticks or single atomic pipeline (implementation choice).

---

## 7. Variable cost formula

Baseline `[CAMP]` anchor (minimal case):

| Line item | Bytes |
|-----------|-------|
| Offspring endowment (median) | 172,800 (2 fuel-days) |
| Construction overhead | 86,400 (1 fuel-day) |
| **Baseline debit** | **~259,200** |
| Parent reserve after | ≥ 86,400 |

General formula:

```text
childCost =
  kEndowmentBytesPerLocus × nLoci
+ kConstructionBytesPerLocus × nLoci
+ kAxonConstructionBytes × nAxons
+ Σ kParthenogenesisStepBasalCost        (one per clone step)
+ Σ kParthenogenesisOperatorSurcharge[op] (Gate 1 events only)
+ entropyHeatSplitTicks                  (linked to kEnergonRainEntropy scale)
```

| Child genotype | nLoci | Relative cost |
|----------------|-------|---------------|
| `[CAMP]` | 4 | 1.0× |
| `[CCAMP]` | 5 | ~1.25× |
| `[AMP]` | 3 | ~0.75× endowment but **aborts at valid(G)** |

Parent solvency check uses **`estimateParthenogenesisCostMax`** before starting; **`commitParthenogenesisCost(actual)`** after pipeline completes.

---

## 8. Birth collapse — Grover floor

At spawn collapse (not during pipeline steps):

```text
valid(G) :=
  count(P) ≥ 1 ∧ count(M) ≥ 1 ∧ count(C) ≥ 1 ∧ count(A) ≥ 1
  ∧ parseOK(G)
  ∧ axonTargetsLegal(G)
  ∧ nAxons ≤ kAxonChannelCapacity
```

| Genotype | valid(G) |
|----------|----------|
| `[CAMP]`, `[CCAMP]` | ✅ |
| `[AMP]`, `[CC]` | ❌ → abort, bytes spent |

**v1:** Single pipeline draw + classical floor.  
**v2:** Draw K candidate children in superposition; Grover amplifies marked viable; collapse one ([EVOLUTION.md](EVOLUTION.md) §4.4).

Grover does **not** favour insertions — it prevents module-less abominations from spawning.

---

## 9. Morphogenesis pipeline (ordered)

```text
attemptParthenogenesis(parent):
  if !eligible(parent): return

  childG ← copy(parent.G)
  runningDebit ← kParthenogenesisInitCost
  debit(parent, runningDebit)

  for each locus L in assemblyOrder(parent.G_seq):
    debit step basal
    Gate 1 on L
    instantiate neuron skeleton for L in child blueprint
    Gate 2 on neuron params

  for each axon A in parent.G_axon (mapped to child ids):
    debit step basal
    Gate 1 on A
    Gate 2 on axon trust / η

  for each link B in parent.G_skel:
    debit step basal
    Gate 1 on B
    Gate 2 on bone params

  Gate 2 on organism-level phenotype (register, senseRadius, heading)

  if !valid(childG) || !solvency(parent, runningDebit):
    ABORT (spent)

  endowment ← drawEndowment(childG)
  runningDebit += endowment + finalisation
  if !solvency(parent, runningDebit):
    ABORT (spent)

  spawn = developFromGenotype(childG, spawnPose, endowment)
  spawn.finalizeSpawn(rng)   // Gate 2 overlap OK — idempotent jitter policy TBD
  population.add(spawn)
```

**Spawn pose:** adjacent wet tile, clearance from parent, tide-safe.

---

## 10. Tick integration

```text
... → metabolise → viability → axon basal → convey → dockPass → signal → prune
  → parthenogenesisPass (NEW: eligible parents attempt split)
  → remove dead
```

Parthenogenesis debits parent during its pipeline ticks; child appears on successful collapse. **Single-threaded per parent** — no parallel splits.

---

## 11. Constants (proposed)

| Constant | Value | File |
|----------|-------|------|
| `kParthenogenesisMinAgeTicks` | 600 | `CellConstants.hpp` |
| `kParthenogenesisStructuralRate` | 0.03 | `Chaos.hpp` (= `kMisalignmentRate`) |
| `kParthenogenesisStepBasalCost` | 8 | `CellConstants.hpp` |
| `kParthenogenesisInitCost` | 864 | `CellConstants.hpp` (~⅓ fuel-day) |
| `kEndowmentBytesPerLocus` | 43,200 | derived from baseline |
| `kConstructionBytesPerLocus` | 21,600 | derived |
| `kAxonConstructionBytes` | 720 | ~12 axons × baseline |
| `kParthenogenesisDeletionSurcharge` | 432 | cheap |
| `kParthenogenesisDuplicationSurcharge` | 2,160 | medium |
| `kParthenogenesisInsertionSurcharge` | 4,320 | expensive |
| `kParthenogenesisParentReserveMin` | 86,400 | 1 fuel-day |
| `kChaosJitterRate` | 0.03 | existing Gate 2 |

Operator surcharges scale with `kEnergonRainEntropy` (2.0) in final tuning pass.

---

## 12. Implementation phases

### R1a — Faithful clone + Gate 2 only

1. `eligibleForParthenogenesis(parent)`
2. `cloneCampFaithful(parent) → child` with Gate 2 jitter only
3. Fixed `[CAMP]` cost, spawn adjacent
4. Tests: parent pays, child alive, topology identical, params differ slightly

### R1b — Gate 1 structural operators

1. Per-locus / per-axon Bernoulli + dup/del/ins
2. Variable cost + abort on insolvency / invalid(G)
3. Tests: `[CCAMP]` dup path, `[AMP]` abort with bytes spent

### R1c — Energon ledger & telemetry

1. `runningDebit` visible in inspector / research log
2. Survival curves: births vs aborts vs bytes burned

### R1d — Grover K-candidate (optional v2)

1. K superposed children, floor oracle, collapse one

---

## 13. Test plan

| Test | Pass criterion |
|------|----------------|
| **Rub-until-birth** | Feedbag parent, 100% structural fidelity override, co-located clearance → spawn in ≤N ticks |
| **Gate 2 only** | Faithful `[CAMP]` child; axon η/trust differ from parent within ±3σ jitter |
| **Gate 1 dup** | Forced dup on A → child has 2 A loci; cost > baseline |
| **valid floor** | Forced `[AMP]` draw → abort; `bytesSpent > 0`; no spawn |
| **Insolvency abort** | Parent with exact budget → pipeline aborts mid-way; bytes spent |
| **Rate calibration** | 100%/50%/10% structural rate overrides → binomial expectations (mirror `[death_feast]`) |
| **No clone without gates** | Young or starving parent never debits reproduction |

Tag: `[parthenogenesis]`, `[birth_rub]`.

---

## 14. Relationship to R0 HGT

| Mechanism | When | Entropy |
|-----------|------|---------|
| **Ecological INSERTION** (R0) | Corpse / dangling stub dock | 3% at dock |
| **Vertical INSERTION** (R1) | Parthenogenesis Gate 1 | 3% per locus step |
| **Parametric drift** | Gate 2 always | ±3% jitter |

HGT brings foreign edges **in**; parthenogenesis copies **down** with error. Both use the same chaos family; boundaries differ.

---

## 15. Open questions

| Topic | Status |
|-------|--------|
| Atomic pipeline vs multi-tick cytokinesis animation | Implementation |
| Endowment: copy parent split ratio vs fresh draw | TBD |
| Gate 2 on `finalizeSpawn` — double jitter if called twice | Policy: jitter once at morphogenesis OR at spawn |
| Mate-ready cloaca as reproduction intent signal | Phase R2 mating |
| `G_skel` co-evolution deep spec | [EVOLUTION.md](EVOLUTION.md) §9 |

---

## 16. Bibliography

| Topic | Citation |
|-------|----------|
| Pre-LUCA → vertical reproduction | Tang (2020); Vetsigian et al. (2018); Szathmáry & Smith (1997) |
| Parthenogenesis | Suomalainen et al. (1987); Normark (2003) |
| Developmental noise / epigenetics | Lewontin (1966); Jablonka & Lamb (2005) |
| Gene duplication / loss | Ohno (1970); Lynch & Conery (2000); Long et al. (2010) |
| Life-history cost of reproduction | Stearns (1992); Smith & Fretwell (1974) |
| Avida | Ofria & Wilke (2004) |
| NEAT | Stanley & Miikkulainen (2002) |
| Grover / QIEA | Grover (1996); Han & Kim; DESIGN-NOTES §5 |
| Lorenz training curriculum | EVOLUTION.md §2.3; Schmidt et al. (2023) |

---

## 17. Summary diagram

```text
     [Parent CAMP — long life, full hub]
                 │
                 ▼ eligibility (age, fuel, wet)
     morphogenesis pipeline ─── runningDebit ↑
                 │
       ┌─────────┴─────────┐
       │  per locus/axon/link │
       │  Gate 1: 3% struct  │──→ dup / del / ins (paid)
       │  Gate 2: always jitter │──→ ±3% on all numbers
       └─────────┬─────────┘
                 │
         valid(G) ∧ solvency?
            │         │
           yes        no → ABORT (bytes spent, no child)
            │
            ▼
        SPAWN child ──→ tide oracle continues
```

**Parthenogenesis is energon betting on a jittered copy of yourself. Gate 1 rewrites the blueprint; Gate 2 ensures the copy is never you; the tide decides if the bet was worth it.**
