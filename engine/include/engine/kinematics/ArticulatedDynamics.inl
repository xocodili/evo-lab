#pragma once

#include "engine/kinematics/ForwardKinematics.hpp"
#include "engine/kinematics/BoneDistanceConstraint.hpp"
#include "engine/kinematics/KinematicNodeLookup.hpp"
#include "engine/kinematics/NodeMediumDrag.hpp"
#include "engine/kinematics/WorldPoseSync.hpp"
#include "engine/kinematics/Math.hpp"

#include <algorithm>
#include <cmath>

namespace evolab::engine::kinematics {

namespace articulated_detail {

template <typename NodeLike, std::size_t Extent, typename HeightAtXZ>
bool fkFromState(const KinematicSkeleton& skeleton, ArticulatedBodyState& state,
                 std::span<NodeLike, Extent> nodes, const NodeSpanIndex<NodeLike>& nodeIndex,
                 HeightAtXZ&& heightAtXZ) {
  if (!skeleton.valid() || state.jointYawDelta.size() != skeleton.jointCount()) {
    return false;
  }

  return solveForwardKinematicsFromDeltas(
      skeleton, std::span<const float>(state.jointYawDelta), state.rootWorldYaw, nodes,
      std::forward<HeightAtXZ>(heightAtXZ), state.fkScratch, nodeIndex);
}

}  // namespace articulated_detail

template <typename NodeLike, std::size_t Extent, typename HeightAtXZ>
bool solveForwardKinematicsFromBodyState(const KinematicSkeleton& skeleton,
                                         ArticulatedBodyState& state,
                                         std::span<NodeLike, Extent> nodes,
                                         HeightAtXZ&& heightAtXZ) {
  const NodeSpanIndex<NodeLike> nodeIndex(nodes);
  return articulated_detail::fkFromState(skeleton, state, nodes, nodeIndex,
                                         std::forward<HeightAtXZ>(heightAtXZ));
}

template <typename NodeLike, std::size_t Extent>
bool resolveSpineAxisAtNode(const KinematicSkeleton& skeleton, std::span<NodeLike, Extent> nodes,
                            const NodeSpanIndex<NodeLike>& nodeIndex, std::uint32_t nodeId,
                            float& axisX, float& axisZ) {
  const std::size_t nodeIndexInSpan = nodeIndex.indexOf(nodeId);
  if (nodeIndexInSpan == kInvalidNodeSpanIndex) {
    return false;
  }

  const NodeLike& node = nodes[nodeIndexInSpan];

  for (std::size_t jointIndex = 0; jointIndex < skeleton.jointCount(); ++jointIndex) {
    const KinematicSkeleton::Joint& joint = skeleton.joint(jointIndex);
    if (joint.parentIndex < 0) {
      continue;
    }

    const KinematicSkeleton::Joint& parentJoint =
        skeleton.joint(static_cast<std::size_t>(joint.parentIndex));
    if (parentJoint.nodeId != nodeId) {
      continue;
    }

    const std::size_t childIndex = nodeIndex.indexOf(joint.nodeId);
    if (childIndex == kInvalidNodeSpanIndex) {
      continue;
    }

    const NodeLike& child = nodes[childIndex];
    axisX = child.worldX - node.worldX;
    axisZ = child.worldZ - node.worldZ;
    const float len = std::hypot(axisX, axisZ);
    if (len <= 1.0e-6f) {
      return false;
    }

    axisX /= len;
    axisZ /= len;
    return true;
  }

  for (std::size_t jointIndex = 0; jointIndex < skeleton.jointCount(); ++jointIndex) {
    const KinematicSkeleton::Joint& joint = skeleton.joint(jointIndex);
    if (joint.nodeId != nodeId || joint.parentIndex < 0) {
      continue;
    }

    const KinematicSkeleton::Joint& parentJoint =
        skeleton.joint(static_cast<std::size_t>(joint.parentIndex));
    const std::size_t parentIndex = nodeIndex.indexOf(parentJoint.nodeId);
    if (parentIndex == kInvalidNodeSpanIndex) {
      continue;
    }

    const NodeLike& parent = nodes[parentIndex];
    axisX = node.worldX - parent.worldX;
    axisZ = node.worldZ - parent.worldZ;
    const float len = std::hypot(axisX, axisZ);
    if (len <= 1.0e-6f) {
      return false;
    }

    axisX /= len;
    axisZ /= len;
    return true;
  }

  return false;
}

template <typename NodeLike, std::size_t Extent>
bool resolveSpineAxisAtNode(const KinematicSkeleton& skeleton, std::span<NodeLike, Extent> nodes,
                            std::uint32_t nodeId, float& axisX, float& axisZ) {
  const NodeSpanIndex<NodeLike> nodeIndex(nodes);
  return resolveSpineAxisAtNode(skeleton, nodes, nodeIndex, nodeId, axisX, axisZ);
}

template <typename NodeLike, std::size_t Extent, typename HeightAtXZ>
bool stepArticulatedBody(const KinematicSkeleton& skeleton, ArticulatedBodyState& state,
                         std::span<NodeLike, Extent> nodes, std::span<const MuscleCommand> muscles,
                         std::span<const ExternalImpulse> impulses,
                         const ArticulatedStepParams& params, HeightAtXZ&& heightAtXZ) {
  if (!skeleton.valid()) {
    return false;
  }

  state.ensureJointCount(skeleton.jointCount());

  const std::size_t rootJointIndex = skeleton.jointIndex(skeleton.rootNodeId());
  if (rootJointIndex >= skeleton.jointCount()) {
    return false;
  }

  const NodeSpanIndex<NodeLike> nodeIndex(nodes);
  const std::size_t rootNodeIndex = nodeIndex.indexOf(skeleton.rootNodeId());
  if (rootNodeIndex == kInvalidNodeSpanIndex) {
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

  if (!articulated_detail::fkFromState(skeleton, state, nodes, nodeIndex,
                                       std::forward<HeightAtXZ>(heightAtXZ))) {
    return false;
  }

  NodeLike& rootNode = nodes[rootNodeIndex];

  state.rootVelX += params.mediumVelX;
  state.rootVelZ += params.mediumVelZ;

  const float linearRetention = std::max(0.0f, 1.0f - params.linearDrag);
  const float yawRetention = std::max(0.0f, 1.0f - params.yawDamping);
  state.rootVelX *= linearRetention;
  state.rootVelZ *= linearRetention;
  state.rootYawRate *= yawRetention;

  rootNode.worldX += state.rootVelX;
  rootNode.worldZ += state.rootVelZ;
  state.rootWorldYaw = normalizeAngle(state.rootWorldYaw + state.rootYawRate);

  if (!articulated_detail::fkFromState(skeleton, state, nodes, nodeIndex,
                                       std::forward<HeightAtXZ>(heightAtXZ))) {
    return false;
  }

  bool worldPoseDeformed = false;
  if (params.nodeLinearDrag > 0.0f) {
    applyPerNodeMediumDrag(skeleton, nodes, nodeIndex, params.mediumVelX, params.mediumVelZ,
                           params.nodeLinearDrag, params.nodeDragDepthGain);
    worldPoseDeformed = true;
  }

  const float rootX = rootNode.worldX;
  const float rootZ = rootNode.worldZ;

  for (const ExternalImpulse& impulse : impulses) {
    const std::size_t impulseNodeIndex = nodeIndex.indexOf(impulse.nodeId);
    if (impulseNodeIndex == kInvalidNodeSpanIndex) {
      continue;
    }

    float axisX = 0.0f;
    float axisZ = 0.0f;
    NodeLike& target = nodes[impulseNodeIndex];
    if (resolveSpineAxisAtNode(skeleton, nodes, nodeIndex, impulse.nodeId, axisX, axisZ)) {
      const float axial = impulse.impulseX * axisX + impulse.impulseZ * axisZ;
      const float displacement = axial * params.invMass;
      target.worldX += axisX * displacement;
      target.worldZ += axisZ * displacement;
      worldPoseDeformed = true;

      const float lateralX = impulse.impulseX - axisX * axial;
      const float lateralZ = impulse.impulseZ - axisZ * axial;
      const float leverX = target.worldX - rootX;
      const float leverZ = target.worldZ - rootZ;
      state.rootYawRate += (leverX * lateralZ - leverZ * lateralX) * params.invInertia;
    } else {
      state.rootVelX += impulse.impulseX * params.invMass;
      state.rootVelZ += impulse.impulseZ * params.invMass;
      worldPoseDeformed = true;

      const float leverX = target.worldX - rootX;
      const float leverZ = target.worldZ - rootZ;
      state.rootYawRate +=
          (leverX * impulse.impulseZ - leverZ * impulse.impulseX) * params.invInertia;
    }
  }

  if (params.solveBoneConstraints && skeleton.jointCount() >= 2) {
    BoneDistanceConstraintParams boneParams;
    boneParams.iterationCount = std::max(1, params.boneConstraintIterations);
    boneParams.stiffness = std::clamp(params.boneConstraintStiffness, 0.0f, 1.0f);
    if (params.pinKinematicRootDuringBones) {
      boneParams.pinnedNodeId = skeleton.rootNodeId();
      boneParams.hasPinnedNode = true;
    }
    solveBoneDistanceConstraints(skeleton, nodes, boneParams);
    worldPoseDeformed = true;
  }

  refreshNodeWorldY(skeleton, nodes, nodeIndex, std::forward<HeightAtXZ>(heightAtXZ));
  if (worldPoseDeformed) {
    syncJointYawDeltasFromWorld(skeleton, nodes, state);
  }

  return true;
}

}  // namespace evolab::engine::kinematics
