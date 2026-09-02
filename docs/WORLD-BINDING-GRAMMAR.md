# World binding grammar — stem assembly physics

**Status:** Shipped (2026-09-02)  
**Code:** `WorldBinding.hpp/.cpp`, `StemBinding.hpp/.cpp`, `Organism.hpp` (`StemAssemblyPlan`)  
**Companions:** [KINEMATICS.md](KINEMATICS.md), [PARTHENOGENESIS.md](PARTHENOGENESIS.md), [HGT-INSERTION.md](HGT-INSERTION.md)

This document specifies how **Y-star geometry** is built in evo-lab after the stem-binding refactor. The design separates **world physics** (same for every organism) from **inherited assembly plans** (how/when a lineage uses those physics).

---

## 1. Design split

| Layer | Owner | What it encodes |
|-------|--------|-----------------|
| **World binding grammar** | `WorldBinding.hpp` | Hub socket count, angular spacing, glue label, cooperative bind threshold, assembly ε, default bind rate/cost |
| **Stem bind operator** | `NeuronStem.hpp` | `tryStemBindPeripheralToHub()` — creates `SkeletonLink` + `StemBindRecord` |
| **Assembly plan** | `StemBinding.hpp` | Locus list + ordered bind records; `assembleOrganismFromStemPlan()` |
| **Factory / spawn** | `assembleOrganismFromStemPlan()` | One canonical plan for gen-0 camp (`defaultCampStemAssemblyPlan()`) |

**Analogy:** Wang tiles / DNA self-assembly — the **glue and socket geometry** are environmental law; the **stem cell** carries the recipe for which tiles bind in which order.

---

## 2. World physics (not inherited)

Constants in `WorldBinding.hpp`:

| Constant | Value | Role |
|----------|-------|------|
| `kWorldHubSocketCount` | **3** | Peripheral arms on hub (P, M, A) |
| `kWorldHubSocketSeparationRad` | `kCampNomArmSeparationRad` (2π/3) | Slot angular spacing |
| `kStemGlueLabel` | **1** | Single glue type at this stage — all faces/types share it |
| `kStemBindCooperativeStrength` | **2** | Minimum cooperating contacts (future multi-tile rules) |
| `kStemBindAssemblyEpsilonFactor` | **0.01** | Proximity gate for bind attempt |
| `kStemBindRateDefault` | `kAxonDockRate` | Stochastic bind attempt rate |
| `kStemBindCostBytesDefault` | `kHgtInsertionCostBytes` | Energon cost when payment required |

**Socket angle:**

```text
hubSocketAngleRad(heading, slot) = heading + slot × kWorldHubSocketSeparationRad
```

Slot **0** is the organism forward axis (perceptor arm). Slots **1** and **2** are the ±120° mouth and actuator arms. The Y-star emerges from **slot index + assembly order**, not from a hard-coded type→angle map in the factory.

---

## 3. Inherited stem assembly (genotype / use-case)

```text
StemAssemblyPlan
  loci[]   — { nodeId, NeuronType } in developmental order
  binds[]  — StemBindRecord per hub–peripheral link
```

Each `StemBindRecord` stores:

| Field | Purpose |
|-------|---------|
| `hubNodeId`, `peripheralNodeId` | Endpoints after id remap |
| `hubSlot` | Which hub socket (0..2) |
| `peripheralFace` | Stem face enum (reserved for multi-face tiles) |
| `restLength` | Bone length at bind |
| `muscleBundle` | Whether link participates in stroke bundle |

**Canonical camp plan** (`defaultCampStemAssemblyPlan()`):

- Loci: P(1), M(2), C(3), A(4) — hub C at id 3  
- Binds: P→slot 0, M→slot 1, A→slot 2  
- Rest length: `kCampNomBoneLength`  
- Neural graph: 12 developmental axons (all directed pairs) via `closeStemNeuralGraphAmongLoci()`

Gen-0 campers are built by `makeCampNomOrganism()` → `assembleOrganismFromStemPlan(..., defaultCampStemAssemblyPlan(), ...)`.

---

## 4. Parthenogenesis — bind replay

Morphogenesis treats each inherited bind as its own pipeline step (`MorphogenesisKind::Bind`):

```text
cloneParentStructure  →  copy stemAssembly from parent
buildMorphogenesisPlan  →  insert Bind steps before axon/link steps
for each step:
  Bind  →  replayMorphogenesisBindStep()  →  tryStemBindPeripheralToHub (idempotent if link exists)
  Locus / Axon / Link  →  existing Gate 1 + Gate 2
Gate 2 on hub bind links: preserve hubSlot angle from record (world physics), jitter restLength only
```

Telemetry: `ParthenogenesisResult::stemBindStepsReplayed` — expect **3** for faithful `[CAMP]` birth.

**Morphological freaks:** High structural rate (`structuralRateOverride = 1.0`) may duplicate/delete loci and axons while **stem bind geometry** (slot angles vs heading) stays consistent when bind records survive. Non-viable genotypes (`[AMP]`, etc.) may still spawn if `campGenotypeValid` passes — inspector labels **freak**.

---

## 5. HGT dock compatibility

`OrganismHgt.cpp` — `compatibleDockTarget()` requires:

1. **`stemGlueMatches()`** — world glue label on open stem face  
2. **Open-end type memory** — partner neuron type filter when set (preserves prior HGT behaviour)

Stem glue is **not** per-neuron-type genetics; type filtering remains for partner selection only.

---

## 6. Equivalence to prior factory methodology

The old `makeCampNomOrganism()` factory inlined type→angle wiring. The stem plan reproduces the same observable phenotype:

| Invariant | Prior factory | Stem assembly |
|-----------|---------------|---------------|
| Node ids | 1=P, 2=M, 3=C, 4=A | Same |
| Hub | C (id 3) | Same |
| Arm angles | 0°, +120°, −120° vs heading | `hubSocketAngleRad(heading, slot)` |
| Links | 3 hub binds + optional closing edges | 3 `StemBindRecord` + render edges |
| Axons | 12 developmental pairs | `closeStemNeuralGraphAmongLoci()` |
| `isCampNom()` | ✅ | ✅ |
| Marathon viability @ tick 600 | ≥40 camp nom | ✅ (unchanged thresholds) |

Fuel wallet layout is unchanged (`endowCampNodes` / hub-first camp storage). Peripheral stores remain capped at `kNeuronStoreMaxBytes` — total endowment is conserved in the hub.

---

## 7. Tests

| Filter | What it proves |
|--------|----------------|
| `[stembinding]` | Default plan geometry, slot angles vs legacy constants, parthenogenesis bind replay, freak morphology |
| `[parthenogenesis][birth_rub]` | Wealthy parent spawn cost + faithful camp child |
| `[hgt]` | Dock rules with stem glue + open-end type |
| `[marathon][viability]` | 600-tick session retains camp nom morphology |
| `[ultra-marathon]` | 18000-tick energon / spawn luck gates |

---

## 8. Future work

- **Multi-glue world tiles** — extend `kStemGlueLabel` to a small alphabet; still world-owned, not per P/M/C/A  
- **Topology co-evolution** — dup/del/ins on `stemAssembly.loci` and `binds` under Gate 1 ([EVOLUTION.md](EVOLUTION.md) §9)  
- **Cooperative bind threshold** — enforce `kStemBindCooperativeStrength` in crowded assembly  
- **Stem bind payment during morphogenesis** — optional debit per bind step (today: replay without extra bind surcharge beyond step basal)
