#pragma once

#include "engine/kinematics/ForwardKinematics.hpp"
#include "engine/kinematics/KinematicLocalPose.hpp"

#include <algorithm>
#include <cmath>

namespace evolab::engine::kinematics {

namespace articulated_detail {

template <typename NodeRange>
std::size_t nodeIndexById(const NodeRange& nodes, std::uint32_t nodeId) {
  for (std::size_t i = 0; i < nodes.size(); ++i) {
    if (nodes[i].id == nodeId) {
      return i;
    }
  }
  return nodes.size();
}

template <typename NodeLike, std::size_t Extent, typename HeightAtXZ>
bool fkFromState(const KinematicSkeleton& skeleton, const ArticulatedBodyState& state,
                 float rootWorldYaw, std::span<NodeLike, Extent> nodes,
                 HeightAtXZ&& heightAtXZ) {
  if (!skeleton.valid() || state.jointYawDelta.size() != skeleton.jointCount()) {
    return false;
  }
  KinematicLocalPose pose = KinematicLocalPose::zeros(skeleton.jointCount());
  for (std::size_t jointIndex = 0; jointIndex < skeleton.jointCount(); ++jointIndex) {
    pose.yawDelta(jointIndex) = state.jointYawDelta[jointIndex];
  }
  return solveForwardKinematics(skeleton, pose, rootWorldYaw, nodes,
                                std::forward<HeightAtXZ>(heightAtXZ));
}

}  // namespace articulated_detail

template <typename NodeLike, std::size_t Extent, typename HeightAtXZ>
bool stepArticulatedBody(const KinematicSkeleton& skeleton, ArticulatedBodyState& state,
                         float& rootWorldYaw, std::span<NodeLike, Extent> nodes,
                         std::span<const MuscleCommand> muscles,
                         std::span<const ExternalImpulse> impulses,
                         const ArticulatedStepParams& params, HeightAtXZ&& heightAtXZ) {
  if (!skeleton.valid()) {
    return false;
  }

  state.ensureJointCount(skeleton.jointCount());

  const std::size_t rootIndex = skeleton.jointIndex(skeleton.rootNodeId());
  if (rootIndex >= skeleton.jointCount()) {
    return false;
  }

  for (const MuscleCommand& muscle : muscles) {
    if (muscle.jointIndex >= skeleton.jointCount()) {
      continue;
    }
    const KinematicSkeleton::Joint& joint = skeleton.joint(muscle.jointIndex);
    if (joint.parentIndex < 0) {
      continue;
    }

    float& yaw = state.jointYawDelta[muscle.jointIndex];
    float& yawVel = state.jointYawVel[muscle.jointIndex];
    const float accel =
        muscle.stiffness * (muscle.targetYawDelta - yaw) - muscle.damping * yawVel;
    yawVel += accel;
    yaw += yawVel;

    const float bind = joint.bindLocalYaw;
    const float minDelta = joint.constraint.minLocalYaw - bind;
    const float maxDelta = joint.constraint.maxLocalYaw - bind;
    yaw = std::clamp(yaw, minDelta, maxDelta);
    if (yaw <= minDelta || yaw >= maxDelta) {
      yawVel *= 0.5f;
    }
  }

  if (!articulated_detail::fkFromState(skeleton, state, rootWorldYaw, nodes,
                                       std::forward<HeightAtXZ>(heightAtXZ))) {
    return false;
  }

  NodeLike& rootNode = nodes[articulated_detail::nodeIndexById(nodes, skeleton.rootNodeId())];
  const float rootX = rootNode.worldX;
  const float rootZ = rootNode.worldZ;

  for (const ExternalImpulse& impulse : impulses) {
    const std::size_t nodeIndex = articulated_detail::nodeIndexById(nodes, impulse.nodeId);
    if (nodeIndex >= nodes.size()) {
      continue;
    }
    const NodeLike& target = nodes[nodeIndex];
    const float leverX = target.worldX - rootX;
    const float leverZ = target.worldZ - rootZ;

    state.rootVelX += impulse.impulseX * params.invMass;
    state.rootVelZ += impulse.impulseZ * params.invMass;
    state.rootYawRate +=
        (leverX * impulse.impulseZ - leverZ * impulse.impulseX) * params.invInertia;
  }

  state.rootVelX += params.tideVelX;
  state.rootVelZ += params.tideVelZ;

  const float linearRetention = std::max(0.0f, 1.0f - params.linearDrag);
  const float yawRetention = std::max(0.0f, 1.0f - params.yawDamping);
  state.rootVelX *= linearRetention;
  state.rootVelZ *= linearRetention;
  state.rootYawRate *= yawRetention;

  rootNode.worldX += state.rootVelX;
  rootNode.worldZ += state.rootVelZ;
  rootWorldYaw = normalizeAngle(rootWorldYaw + state.rootYawRate);

  return articulated_detail::fkFromState(skeleton, state, rootWorldYaw, nodes,
                                         std::forward<HeightAtXZ>(heightAtXZ));
}

}  // namespace evolab::engine::kinematics
