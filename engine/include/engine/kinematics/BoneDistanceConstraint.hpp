#pragma once

#include "engine/kinematics/KinematicNodeLookup.hpp"
#include "engine/kinematics/KinematicSkeleton.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <span>

namespace evolab::engine::kinematics {

struct BoneDistanceConstraintParams {
  int iterationCount = 12;
  // 1 = hard rest-length correction each iteration; lower values soften the chain.
  float stiffness = 1.0f;
  // When set, this node stays fixed while child links relax toward rest length.
  std::uint32_t pinnedNodeId = 0;
  bool hasPinnedNode = false;
};

namespace bone_distance_detail {

inline float nodeWeight(std::uint32_t nodeId, const BoneDistanceConstraintParams& params) {
  if (params.hasPinnedNode && nodeId == params.pinnedNodeId) {
    return 0.0f;
  }
  return 1.0f;
}

}  // namespace bone_distance_detail

// Position-based rest-length constraints along skeleton tree edges.
// Run after moving a node (e.g. tail stroke) so downstream nodes follow through the chain.
template <typename NodeLike, std::size_t Extent>
bool solveBoneDistanceConstraints(const KinematicSkeleton& skeleton,
                                  std::span<NodeLike, Extent> nodes,
                                  const BoneDistanceConstraintParams& params = {}) {
  if (!skeleton.valid()) {
    return false;
  }

  const NodeSpanIndex<NodeLike> nodeIndex(nodes);
  const int iterations = std::max(1, params.iterationCount);
  const float stiffness = std::clamp(params.stiffness, 0.0f, 1.0f);

  for (int iteration = 0; iteration < iterations; ++iteration) {
    for (std::size_t jointIndex = 0; jointIndex < skeleton.jointCount(); ++jointIndex) {
      const KinematicSkeleton::Joint& joint = skeleton.joint(jointIndex);
      if (joint.parentIndex < 0) {
        continue;
      }

      const KinematicSkeleton::Joint& parentJoint =
          skeleton.joint(static_cast<std::size_t>(joint.parentIndex));
      const std::size_t parentNodeIndex = nodeIndex.indexOf(parentJoint.nodeId);
      const std::size_t childNodeIndex = nodeIndex.indexOf(joint.nodeId);
      if (parentNodeIndex == kInvalidNodeSpanIndex || childNodeIndex == kInvalidNodeSpanIndex) {
        return false;
      }

      NodeLike& parentNode = nodes[parentNodeIndex];
      NodeLike& childNode = nodes[childNodeIndex];

      const float dx = childNode.worldX - parentNode.worldX;
      const float dz = childNode.worldZ - parentNode.worldZ;
      const float dist = std::hypot(dx, dz);
      if (dist <= 1.0e-6f || joint.restLength <= 0.0f) {
        continue;
      }

      const float error = dist - joint.restLength;
      const float correction = error * stiffness;
      const float nx = dx / dist;
      const float nz = dz / dist;

      const float parentWeight = bone_distance_detail::nodeWeight(parentJoint.nodeId, params);
      const float childWeight = bone_distance_detail::nodeWeight(joint.nodeId, params);
      const float weightSum = parentWeight + childWeight;
      if (weightSum <= 1.0e-6f) {
        continue;
      }

      const float parentScale = parentWeight / weightSum;
      const float childScale = childWeight / weightSum;

      parentNode.worldX += nx * correction * parentScale;
      parentNode.worldZ += nz * correction * parentScale;
      childNode.worldX -= nx * correction * childScale;
      childNode.worldZ -= nz * correction * childScale;
    }
  }

  return true;
}

template <typename NodeLike, std::size_t Extent>
float maxBoneLengthError(const KinematicSkeleton& skeleton, std::span<NodeLike, Extent> nodes) {
  if (!skeleton.valid()) {
    return 0.0f;
  }

  const NodeSpanIndex<NodeLike> nodeIndex(nodes);
  float maxError = 0.0f;
  for (std::size_t jointIndex = 0; jointIndex < skeleton.jointCount(); ++jointIndex) {
    const KinematicSkeleton::Joint& joint = skeleton.joint(jointIndex);
    if (joint.parentIndex < 0) {
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
    const float dist =
        std::hypot(childNode.worldX - parentNode.worldX, childNode.worldZ - parentNode.worldZ);
    maxError = std::max(maxError, std::abs(dist - joint.restLength));
  }
  return maxError;
}

}  // namespace evolab::engine::kinematics
