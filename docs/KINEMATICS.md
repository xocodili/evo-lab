# Kinematics engine — design & rollout

**Location:** `engine/include/engine/kinematics/`  
**Sim adapter:** `src/sim/OrganismKinematics.cpp` (water-column Y only)  
**Status doc:** use this file when asking “are we on track for kinematics?”  
**Physics & muscle design:** [KINEMATICS-PHYSICS-DESIGN.md](KINEMATICS-PHYSICS-DESIGN.md) — engine vs sim vs emergent layers

---

## Layering

| Layer | Responsibility |
|--------|----------------|
| **`evolab_engine::kinematics`** | Skeleton, FK, articulated dynamics, bone constraints, medium drag — **no sim deps** |
| **`evolab_sim`** | `OrganismKinematics.cpp` + `NeuronMusculature.cpp` — height callback, muscle/impulse adapters |
| **`evolab_game`** | Draw bones/nodes from solved world poses |

Industry pattern we follow: **bind pose → local pose → FK → (future) IK/constraints pass → render**.

---

## Module map

| File | Purpose |
|------|---------|
| `KinematicBone.hpp` | Authoring edge: parent, child, restLength, bind local yaw |
| `ForwardKinematics.hpp` | Hierarchical FK + legacy `solveTreeForwardKinematics` wrapper |
| `KinematicSkeleton.hpp/.cpp` | Runtime tree: parent indices, bind pose, cached joint depth, per-joint constraints |
| `KinematicLocalPose.hpp` | Runtime yaw deltas on bind (0 = rest) |
| `JointConstraint.hpp` | min/max local yaw, stiffness hook |
| `KinematicNodeLookup.hpp` | `NodeSpanIndex` — O(1) node id → span index (built once per tick) |
| `ForwardKinematicsScratch.hpp` | Reusable FK workspace (no per-solve heap alloc on hot path) |
| `ArticulatedBodyState.hpp` | Floating-base state: `rootWorldYaw`, rootVel*, joint yaw deltas/velocities |
| `ArticulatedDynamics.hpp/.inl` | Muscle PD → FK → root integration → medium drag → impulses → constraints |
| `BoneDistanceConstraint.hpp` | Rest-length PBD along tree edges |
| `NodeMediumDrag.hpp` | Non-root ambient flow coupling (S4) |
| `Math.hpp` | `normalizeAngle`, constants |

---

## Kinematic root vs effector (sim policy)

Every articulated tree needs exactly one **kinematic root** for FK/dynamics integration. That is `Organism::rootNodeId` passed to `KinematicSkeleton::buildFromBones`.

That is **not** the same as “where force is applied” or “where the brain lives”:

| Role | Torpedo MPCA (gen-0) | Hub-star (legacy) | Dual-computer test harness |
|------|----------------------|-------------------|----------------------------|
| **Kinematic root** | Mouth M (primary / lowest id) | Mouth M on M-arm | Computer C-forage (id 3) |
| **Stroke effector** | A (tail, not root) | A (arm tip) | A (id 5) — impulse not wired |
| **Advection anchor** | `rootNodeId` (mouth) | `rootNodeId` | `rootNodeId` |
| **Articulated dynamics** | ✅ `usesArticulatedLocomotion()` | ✅ when muscle links + actuator | ❌ no muscle skeleton |

**Engine rule:** one floating-base root integrates linear/yaw velocity at the **mouth** (primary live mouth when duplicates exist). **Any node** may receive an `ExternalImpulse` (stroke at tail); bone constraints pin the **effector** during the stroke solve so thrust propagates toward the nose.

**Sim rule (today):** `queueCampStrokeImpulse(organism, effectorNodeId, …)` — caller passes the paying actuator’s id, not a hard-coded camp constant. `ensureKinematicRootNodeId()` keeps `rootNodeId` on the lowest-id live mouth; if that mouth is deleted, root reassigns to the next mouth on the next kinematics tick / parthenogenesis deletion.

**Multi-actuation:** emergent — engine accepts multiple impulses per tick; sim does not loop actuators explicitly. Duplicate mouths do not create duplicate roots; duplicate actuators bill independently when their stroke fires.

---

## Rollout checklist

Track phases when planning features. Ask “which phase does this use case need?”

| Phase | Feature | Status | Use-case trigger |
|-------|---------|--------|------------------|
| **0** | Flat FK stub (world yaw + heading) | ✅ superseded | — |
| **1** | `KinematicSkeleton` resource (tree from bones, parent indices) | ✅ **done** | Any multi-bone organism |
| **2** | Local pose + hierarchical FK (heading at root only) | ✅ **done** | Segmented chains, flagella bends |
| **3** | Joint constraints (min/max yaw, stiffness field) | ✅ **done** | Prevent impossible bends; soft limits pre-IK |
| **4** | IK pass (FABRIK / two-bone on XZ) | ⬜ planned | Mouth reaches food; actuator aims |
| **5** | Effectors & targets | ⬜ planned | P/M/A analog signals drive joint targets |
| **6** | Animation tracks & blend | ⬜ planned | Rhythmic swim stroke, gait cycles |
| **7** | Skinning / mesh deformation | ⬜ deferred | Meshed creatures replace line bones |

### Phase 1–3 behaviour (current)

- **Tree build:** first parent wins per child; closing bones (e.g. M–A triangle base) stay in sim `links` for render but are **not** FK edges.
- **Bind pose:** `SkeletonLink::jointAngle` = bind local yaw relative to parent; root relative to organism `heading`.
- **Local pose:** `KinematicLocalPose::yawDelta[i]` — zero today; sim can set per joint when actuators animate.
- **Constraints:** `JointConstraint` on each skeleton joint; default ±π (no limit). Set on skeleton at build time (future: from genome).

### Sim mapping today

```
Organism.links  →  KinematicSkeleton::buildFromBones
Organism.heading  →  diagnostic mirror of bodyDynamics.rootWorldYaw (initializeArticulatedSpawnPose)
bodyDynamics      →  ArticulatedBodyState (integrated root pose + joint state; zeroed at birth)
WaterColumn       →  heightAtXZ callback (Y only)
```

**Stem assembly (2026-09-02):** Hub–peripheral `SkeletonLink::jointAngle` values come from **world socket grammar** (`hubSocketAngleRad(heading, hubSlot)`) recorded in `Organism::stemAssembly.binds`. Gen-0 camp and parthenogenesis children use the same physics; see [WORLD-BINDING-GRAMMAR.md](WORLD-BINDING-GRAMMAR.md). Gate 2 morphogenesis **preserves hub slot angles** on bind links and jitters `restLength` only.

---

## API quick reference

```cpp
#include "engine/kinematics/KinematicSkeleton.hpp"
#include "engine/kinematics/KinematicLocalPose.hpp"
#include "engine/kinematics/ForwardKinematics.hpp"

std::vector<KinematicBone> bones = { /* parent, child, length, bindYaw */ };
KinematicSkeleton sk = KinematicSkeleton::buildFromBones(bones, rootId);

KinematicLocalPose pose = KinematicLocalPose::zeros(sk.jointCount());
pose.yawDelta(1) = 0.2f;  // animate joint 1

sk.joint(2).constraint.minLocalYaw = -0.5f;
sk.joint(2).constraint.maxLocalYaw = 0.5f;

solveForwardKinematics(sk, pose, rootWorldYaw, std::span(nodes), heightAtXZ);
translateNodesXZ(std::span(nodes), dx, dz);
```

---

## On-track review questions

When checking progress, ask:

1. **Is new bone logic in `engine/`, not `sim/`?** (except height/physics callbacks)
2. **Does the use case need only FK (phases 1–3) or IK (phase 4+)?**
3. **Are pose changes going through `KinematicLocalPose` rather than mutating bind angles?**
4. **Are render-only edges still separate from the FK tree?**
5. **Do tests exist in `test_kinematics.cpp` without pulling in `BarrenWorld`?**

---

## Axonal bundle kinematic gaps (CAMP camper)

**Current (phases 1–3 + E1/E2 dynamics):** `NeuronMusculature.cpp` drives bundle tension → muscle PD → integrated joint flex. Stroke queues **impulse at A** (actuator node); tide enters as root velocity; `stepArticulatedBody` integrates translation + bend. No full contact solver yet.

| Gap | Today | Needed for keel / flagellum fidelity |
|-----|--------|--------------------------------------|
| **Locomotion coupling** | Impulse at A + muscle PD + per-node medium drag (S4); tide via root velocity | Contact, optional sustained pose holds |
| **Bundle as force path** | Tension is a visual/posture scalar (`campAxonBundleTension`) | Force/torque along muscle-bundle edges; stiff vs flex links; stroke energy split between translation and bend |
| **Heading vs body axis** | Chemotaxis slews `bodyDynamics.rootWorldYaw`; tumble reorients body yaw; stroke uses body yaw; per-step `rootYawRate` cleared (no spin carryover); `organism.heading` diagnostic mirror post-step | **S5:** proprio compares intent bearing vs measured body axis for trust learning |
| **Effector analog (phase 5)** | Discrete 0/1/2-byte stroke per tick | Continuous or phased joint targets from P/M/C/A confidence — rhythmic stroke cycle (phase 6) |
| **Tide / advect consistency** | Per-node medium drag + root tide in `stepArticulatedBody` (S4) | Dry vs wet drag coefficients per habitat |
| **Closing render edges** | M–A triangle base in `links` but not FK tree | Decide: FK edge, constraint-only, or pure render — affects keel triangle stability |

**Implementation touchpoints:** `queueCampStrokeImpulse`, `buildMuscleCommands`, `stepArticulatedBody`, `OrganismKinematics.cpp`, `OrganismDetail::tickActuatorOrganism`. Phases **4–6** in the rollout table below are the planned closure for mouth reach, analog effectors, and swim rhythm.

---

## Related docs

- [DESIGN-NOTES.md](DESIGN-NOTES.md) — organism skeleton vs neural axons, spawn chaos on links
- `src/sim/CellConstants.hpp` — `kCampNomLinkJointAngle`, Computer hub/register constants
