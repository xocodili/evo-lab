# Kinematics & physics — layered design

**Status:** Design (2026-08-28)  
**Companion:** [KINEMATICS.md](KINEMATICS.md) (rollout checklist & current behaviour)  
**Principle:** **Base physics ≠ emergent behaviour.** Engine provides forces, constraints, contact, and integration. Evolution discovers phagy, encirclement, scavenging — never hard-coded as heuristics in the engine.

---

## Architectural split

```
┌─────────────────────────────────────────────────────────────────┐
│  §3 Downstream / emergent (evo-lab sim rules + evolution)       │
│  phagy, HGT ecology, mate approach, corpse feast, …             │
│  Uses: contact queries, metabolism, perception — not new physics│
└────────────────────────────▲────────────────────────────────────┘
                             │ reads poses, applies no engine hacks
┌────────────────────────────┴────────────────────────────────────┐
│  §2 Sim — axon bundles as muscles (evolab_sim)                    │
│  NeuronMusculature, stroke billing, CAMP topology                 │
└────────────────────────────▲────────────────────────────────────┘
                             │ joint targets, impulses, muscle params
┌────────────────────────────┴────────────────────────────────────┐
│  §1 Engine — reusable articulated body (evolab_engine)            │
│  FK (done) → dynamics → IK → contact → integration                │
└───────────────────────────────────────────────────────────────────┘
```

| Layer | Namespace / path | Reusable in other games? |
|-------|------------------|---------------------------|
| **§1 Engine** | `engine/include/engine/kinematics/`, new `engine/.../dynamics/` | **Yes** — zero sim deps |
| **§2 Sim muscles** | `src/sim/NeuronMusculature.*`, `OrganismKinematics.*` | evo-lab specific (neurons, axons) |
| **§3 Emergent** | `OrganismPerceptor`, `CellPopulation`, evolution | This simulation only |

---

## Current state (honest baseline)

| Piece | Status | Limitation |
|-------|--------|------------|
| FK tree + joint constraints | ✅ Phases 1–3 | Pose-only; no velocity state |
| `KinematicLocalPose` yaw deltas | ✅ | Set each tick; not integrated |
| `buildCampMusclePose` | ✅ Partial | Tension → visual/posture flex |
| `applyCampBundleStroke` | ✅ Partial | Hub XZ teleport + one-tick flex boost; bypasses FK force path |
| Chemotaxis / heading | ✅ | `organism.heading` decoupled from body-axis geometry |
| IK, dynamics, contact | ❌ | — |

**Today:** flex **looks** real; thrust **partially** moves the hub. Translation and bend are not coupled through a single force graph.

---

# §1 — Core game engine (reusable)

Goal: a **2D articulated-body kit** (XZ plane, Y from height callback) that any top-down or shallow-water sim can reuse — RTS units, critters, ropes, flags — without energon, neurons, or ALife concepts.

## 1.1 Design constraints

- **Planar primary dynamics (XZ).** Y comes from a caller-supplied `heightAtXZ(x,z)` — same hook FK uses today.
- **No gameplay semantics.** Engine knows joints, masses, impulses, contacts — not “mouth”, “food”, or “phagy”.
- **Deterministic fixed step.** Integrator runs at sim tick rate; render may interpolate.
- **Render ≠ physics edges.** Closing triangle edges (CAMP M–A base) stay render-only unless explicitly added as constraints.

## 1.2 Module map (new + existing)

| Module | Path (proposed) | Responsibility |
|--------|-----------------|----------------|
| **KinematicSkeleton** | `kinematics/KinematicSkeleton.hpp` | ✅ Tree topology, bind pose |
| **ForwardKinematics** | `kinematics/ForwardKinematics.hpp` | ✅ FK from local pose |
| **JointConstraint** | `kinematics/JointConstraint.hpp` | ✅ Hard/soft yaw limits |
| **ArticulatedBody** | `dynamics/ArticulatedBody.hpp` | Runtime state: root pose, joint angles, angular velocities, optional root linear velocity |
| **BoneConstraint** | `dynamics/BoneConstraint.hpp` | Fixed rest length; optional stretch compliance |
| **MuscleJointActuator** | `dynamics/MuscleJointActuator.hpp` | **Generic** PD actuator: `targetLocalYaw`, `stiffness`, `damping`, `maxTorque` |
| **ImpulseSolver** | `dynamics/ImpulseSolver.hpp` | One tick: apply external impulses + muscle torques → satisfy constraints → update state |
| **Integrator** | `dynamics/Integrator.hpp` | Semi-implicit Euler or Verlet on XZ + yaw |
| **InverseKinematics** | `kinematics/InverseKinematics.hpp` | FABRIK / analytic two-bone; end-effector target in XZ |
| **ContactManifold** | `dynamics/ContactManifold.hpp` | Segment–segment, circle–segment; outputs penetration + normal |
| **ContactResolver** | `dynamics/ContactResolver.hpp` | Positional + optional velocity correction (Baumgarte) |

All dynamics types live under `evolab::engine::kinematics` or `evolab::engine::dynamics` with **no** includes from `sim/`.

## 1.3 Articulated body state

```cpp
struct ArticulatedBodyState {
  float rootX, rootZ, rootYaw;
  float rootVelX, rootVelZ, rootYawRate;
  std::vector<float> jointLocalYaw;      // resolved angle (bind + delta)
  std::vector<float> jointYawRate;
  // optional per-body-node mass/inertia — default from skeleton authoring
};
```

Authoring data (`KinematicSkeleton`, rest lengths, bind yaws) remains **immutable** at runtime. Pose and velocity are **mutable** state.

## 1.4 Simulation step (engine API)

Single entry point other games call each fixed tick:

```cpp
struct ExternalImpulse {
  std::uint32_t nodeId;  // application point
  float impulseX, impulseZ;
  float torqueYaw;       // optional, at node
};

struct MuscleCommand {
  std::size_t jointIndex;
  float targetLocalYaw;  // PD setpoint
  float stiffness, damping, maxTorque;
};

void stepArticulatedBody(
    const KinematicSkeleton& skeleton,
    ArticulatedBodyState& state,
    std::span<const ExternalImpulse> impulses,
    std::span<const MuscleCommand> muscles,
    std::span<const ContactConstraint> contacts,  // from ContactManifold
    float dt,
    HeightAtXZ heightAtXZ);
```

**Output:** updated `ArticulatedBodyState` + node world poses (via FK pass or direct write).

Games that only need pose targets without full dynamics can still use FK + `MuscleCommand` as **kinematic** PD (current evo-lab path, upgraded).

## 1.5 Rollout phases (engine only)

| Phase | Deliverable | Tests |
|-------|-------------|-------|
| **E1** | `ArticulatedBodyState` + integrate root velocity (no joints) | `test_dynamics_root.cpp` |
| **E2** | Muscle PD on joint yaw + FK sync | `test_dynamics_muscle_pd.cpp` |
| **E3** | External impulse at arbitrary node; propagate to root | `test_dynamics_impulse_tree.cpp` |
| **E4** | FABRIK IK (two/three bone chains) | extend `test_kinematics.cpp` |
| **E5** | Segment contact manifold (no sim rules) | `test_dynamics_contact.cpp` |
| **E6** | Full step: impulses + muscles + contacts + drag | `test_dynamics_step.cpp` |

**Drag / medium coupling:** generic `linearDragCoeff` on root and optionally per-node — sim passes water vs dry coefficients.

## 1.6 What stays out of §1

- Neural axons, trust, confidence bytes
- Energon, mouth bite, metabolism
- “Encircle”, “predator”, “food shadow”
- Parthenogenesis, HGT, cloaca bands
- Any heuristic that mentions another organism’s **intent**

Contact **geometry** belongs here. Contact **consequences** (block bite, steal energon) belong in §3.

---

# §2 — Axon bundles as muscle flex (evo-lab sim)

Goal: map **bidirectional neural axon bundles on skeleton links** to **engine muscle commands**, so stroke energy splits between **translation** and **bend** along the same graph — replacing today’s hub teleport + cosmetic flex.

## 2.1 Conceptual model

Each `SkeletonLink` with `muscleBundle = true` corresponds to a **muscle bundle** — mechanically, the set of directed neural axons that run between the same two nodes (P↔C, C↔A, …).

| Concept | Sim type | Engine mapping |
|---------|----------|----------------|
| Bundle identity | `(parentNodeId, childNodeId)` + `muscleBundle` flag | `MuscleCommand.jointIndex` |
| Neural traffic asymmetry | `campAxonBundleTension()` | `targetLocalYaw` offset from bind |
| Stroke contraction | Actuator paid bytes → mechanical work | `ExternalImpulse` at A node **+** increased A-joint stiffness briefly |
| Bundle stiffness | axon count, mean `trustFeed`, developmental vs foreign | `stiffness`, `maxTorque` |
| Trail / keel | P/M lag when A strokes | negative yaw delta on P/M joints (already sketched) |

**Neural axons** remain the **signalling and energon graph**. **Skeleton muscle bundles** are the **mechanical actuator** view of selected links. One bone, many axons — tension is an aggregate, not a duplicate graph.

## 2.2 Replace cosmetic stroke path

**Today (`applyCampBundleStroke`):**

```
mechanicalThrust → hub.worldX/Z += … ; lastActuatorStrokeFlexBoost = thrust
                 → next updateKinematics applies flex boost to pose
```

**Target:**

```
mechanicalThrust → ExternalImpulse on actuator node along bundle axis
                 → MuscleCommand burst on A joint (high stiffness, short damping)
                 → engine stepArticulatedBody → FK → node poses
```

Chemotaxis continues to set **root yaw rate** or **root target yaw** on `ArticulatedBodyState` — but **body arms** follow from physics, not a post-hoc FK overwrite of hub position.

## 2.3 Sim adapter pipeline (per organism tick)

Order within `OrganismDetail::tickActuatorOrganism` / `updateKinematics`:

```
1. gatherActuatorInteroception / computeCampMotorIntent   (unchanged)
2. buildMuscleCommands(organism, skeleton)                 (replaces buildCampMusclePose output)
      └─ campAxonBundleTension → targetLocalYaw per joint
3. if stroke paid:
      buildStrokeImpulse(organism, motor, hub, mechanicalThrust)
      append stroke MuscleCommand burst on A joint
4. engine.stepArticulatedBody(..., heightAtXZ from WaterColumn)
5. sync organism.nodes from body state
6. record proprioception from Δroot pose vs intended impulse
```

`OrganismKinematics.cpp` becomes a thin wrapper: build skeleton → muscle commands → step → write back.

## 2.4 `buildMuscleCommands` (successor to `buildCampMusclePose`)

```cpp
struct SimMuscleCommand {
  std::size_t jointIndex;
  float targetLocalYaw;
  float stiffness;
  float damping;
  float maxTorque;
};

std::vector<SimMuscleCommand> buildMuscleCommands(
    const Organism& organism,
    const engine::kinematics::KinematicSkeleton& skeleton);
```

Mapping (initial; constants in `CellConstants.hpp`):

| Signal | Effect on targetLocalYaw |
|--------|--------------------------|
| `campAxonBundleTension(parent, child)` | `bindLocalYaw + tension * kAxonBundleFlexGain` |
| Actuator stroke this tick | extra A-joint contraction + trail on P/M |
| Mouth / P saturation | reduce stiffness (flaccid arm) — optional v2 |

Stiffness default: `kAxonBundleFlexStiffness * link.muscleBundle`. Future: scale by count of live axons between the node pair.

## 2.5 Joint limits (already partial)

`applyCampJointFlexLimits` sets ±`kAxonBundleMaxFlexRad` around bind — keep in sim adapter, applied to skeleton before step. Engine `JointConstraint` enforces during PD + integration.

## 2.6 Stroke energy accounting (unchanged semantics)

- Fuel debited from A wallet (and hub if needed) — **sim** rules.
- `kActuatorTranslationEta` fraction becomes **impulse magnitude**, not direct `translateOrganismXZ`.
- `lastDisplacement`, `lastMechanicalThrust` derived from **actual** Δpose after engine step — proprioception stays honest.

## 2.7 Rollout phases (sim only)

| Phase | Deliverable | Depends on |
|-------|-------------|------------|
| **S1** | `buildMuscleCommands` + kinematic PD only (E2) | Engine E2 |
| **S2** | Stroke → `ExternalImpulse` + A-joint burst (S1) | Engine E3 |
| **S3** | Remove hub teleport from `applyCampBundleStroke` | S2 |
| **S4** | IK aim: actuator end-effector toward `heading + focusBearing` before stroke | Engine E4 |
| **S5** | Bundle stiffness from axon count / trust (evolution-visible) | S1 |

Tests: extend `test_nom.cpp` musculature cases to assert **impulse → displacement** correlation, not just FK yaw delta.

## 2.8 Render-only vs muscle edges

| Edge | FK tree | Muscle | Physics contact |
|------|---------|--------|-----------------|
| C→P, C→M, C→A | ✅ | ✅ | optional v2 |
| M–A closing base | render | ❌ | optional shell |

Document per-link flags on `SkeletonLink`: `muscleBundle`, `physicsCollider` (future).

---

# §3 — Downstream emergent behaviour (out of scope for physics)

**Not designed here.** Listed so we do not smuggle behaviour into §1 or §2.

## 3.1 Example: phagy / suffocation / corpse feast

**Fantasy:** one camper constrains another until starvation, then consumes spilled energon.

**Required base physics (§1):** segment contact, positional blocking, sustained muscle hold (generic PD setpoint — not “encircle detector”).

**Required sim metabolism (existing):** basal drain, mouth bite, corpse `Fragment` spill, HGT dock.

**Required perception (existing):** P focus, threat/food competition — learns association, not scripted.

**Explicitly NOT in engine:**

- `isEncircling(victim, predator)` heuristic
- Food query penalties based on neighbour count without geometric occlusion
- Forced death when “surrounded”

Emergence path: if contact blocks mouth–energon line-of-sight and basal > intake, victim dies; scavenger M bites `Fragment` spill. Selection favours bundles/strokes that keep contact — **no phagy module**.

## 3.2 Other downstream behaviours (same pattern)

| Behaviour | Physics needed | Sim/emergence |
|-----------|----------------|---------------|
| Mate approach | contact optional | P mate focus + chemotaxis (shipped) |
| HGT dock | segment proximity | R0 insertion pass (shipped) |
| Corpse archaeology | none | packed Fragment TTL (shipped) |
| Flagellar rhythm | phase E6 animation track | evolution selects stroke phase vs tide |

---

## Migration notes

- **KINEMATICS.md phases 4–6** map to engine E4 + E6 + sim S4–S5.
- Research log “yawDelta = 0 everywhere” is obsolete; replace with “pose-only flex until E2/S1”.
- First implementation milestone: **Engine E2 + Sim S1** (muscle PD without full contact) — smallest step that makes flex **mechanically coupled** to stroke.

---

## Open questions

1. **Root vs hub:** CAMP hub is FK root — confirm all thrust impulses applied at root vs A node for v1.
2. **Closing M–A edge:** pure render or collision shell for “body blocking”?
3. **2D vs 3D:** stay XZ + height callback until meshed creatures (phase 7).

---

## Related docs

- [KINEMATICS.md](KINEMATICS.md) — phase checklist, API quick reference
- [DESIGN-NOTES.md](DESIGN-NOTES.md) §2 — neuron types, stroke IMF analog
- [HGT-INSERTION.md](HGT-INSERTION.md) — anti-clump; contact ≠ conjugation
- [EVOLUTION.md](EVOLUTION.md) — structural ops at birth
