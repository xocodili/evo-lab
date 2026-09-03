#pragma once

#include "engine/kinematics/ArticulatedBodyState.hpp"
#include "engine/kinematics/KinematicNodeLookup.hpp"
#include "engine/kinematics/KinematicSkeleton.hpp"
#include "engine/kinematics/Math.hpp"

#include <cmath>
#include <cstddef>
#include <span>
#include <vector>

namespace evolab::engine::kinematics {

namespace world_pose_sync_detail {

inline float segmentWorldYaw(float parentX, float parentZ, float childX, float childZ) {
  const float dx = childX - parentX;
  const float dz = childZ - parentZ;
  if (dx * dx + dz * dz <= 1.0e-10f) {
    return 0.0f;
  }
  return std::atan2(dx, dz);
}

}  // namespace world_pose_sync_detail

// After world-space drag/constraints, write measured bone directions back into joint yaw deltas
// so the next tick's FK starts from the displayed pose instead of snapping back.
template <typename NodeLike, std::size_t Extent>
void syncJointYawDeltasFromWorld(const KinematicSkeleton& skeleton, std::span<NodeLike, Extent> nodes,
                                 ArticulatedBodyState& state) {
  if (!skeleton.valid() || state.jointYawDelta.size() != skeleton.jointCount()) {
    return;
  }

  const NodeSpanIndex<NodeLike> nodeIndex(nodes);
  const std::size_t jointCount = skeleton.jointCount();
  std::vector<float> jointWorldYaw(jointCount, state.rootWorldYaw);

  for (std::size_t jointIndex = 0; jointIndex < jointCount; ++jointIndex) {
    const KinematicSkeleton::Joint& joint = skeleton.joint(jointIndex);
    if (joint.parentIndex < 0) {
      jointWorldYaw[jointIndex] =
          normalizeAngle(state.rootWorldYaw +
                         joint.constraint.resolve(joint.bindLocalYaw,
                                                  state.jointYawDelta[jointIndex]));
      continue;
    }

    const KinematicSkeleton::Joint& parentJoint =
        skeleton.joint(static_cast<std::size_t>(joint.parentIndex));
    const std::size_t parentNodeIndex = nodeIndex.indexOf(parentJoint.nodeId);
    const std::size_t childNodeIndex = nodeIndex.indexOf(joint.nodeId);
    if (parentNodeIndex == kInvalidNodeSpanIndex || childNodeIndex == kInvalidNodeSpanIndex) {
      continue;
    }

    const NodeLike& parentNode = nodes[parentNodeIndex];
    const NodeLike& childNode = nodes[childNodeIndex];
    const float segmentYaw = world_pose_sync_detail::segmentWorldYaw(
        parentNode.worldX, parentNode.worldZ, childNode.worldX, childNode.worldZ);
    const float parentYaw = jointWorldYaw[static_cast<std::size_t>(joint.parentIndex)];
    const float measuredLocal = normalizeAngle(segmentYaw - parentYaw);
    const float clampedLocal = joint.constraint.clampLocalYaw(measuredLocal);
    state.jointYawDelta[jointIndex] = clampedLocal - joint.bindLocalYaw;
    jointWorldYaw[jointIndex] = segmentYaw;
  }
}

}  // namespace evolab::engine::kinematics
