# Kinematics engine — design & rollout

**Location:** `engine/include/engine/kinematics/`  
**Sim adapter:** `src/sim/OrganismKinematics.cpp` (water-column Y only)  
**Status doc:** use this file when asking “are we on track for kinematics?”

---

## Layering

| Layer | Responsibility |
|--------|----------------|
| **`evolab_engine::kinematics`** | Skeleton data, local pose, FK, constraints, rigid XZ moves — **no sim deps** |
| **`evolab_sim`** | `Organism::updateKinematics` — terrain/water height callback |
| **`evolab_game`** | Draw bones/nodes from solved world poses |

Industry pattern we follow: **bind pose → local pose → FK → (future) IK/constraints pass → render**.

---

## Module map

| File | Purpose |
|------|---------|
| `KinematicBone.hpp` | Authoring edge: parent, child, restLength, bind local yaw |
| `KinematicSkeleton.hpp/.cpp` | Runtime tree: parent indices, bind pose, per-joint constraints |
| `KinematicLocalPose.hpp` | Runtime yaw deltas on bind (0 = rest) |
| `JointConstraint.hpp` | min/max local yaw, stiffness hook |
| `ForwardKinematics.hpp` | Hierarchical FK + legacy `solveTreeForwardKinematics` wrapper |
| `Math.hpp` | `normalizeAngle`, constants |

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
Organism.heading  →  rootWorldYaw
yawDelta = 0      →  bind configuration (factory joint angles)
WaterColumn       →  heightAtXZ callback (Y only)
```

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

## Related docs

- [DESIGN-NOTES.md](DESIGN-NOTES.md) — organism skeleton vs neural axons, spawn chaos on links
- `src/sim/CellConstants.hpp` — `kCampNomLinkJointAngle`, Computer hub/register constants
