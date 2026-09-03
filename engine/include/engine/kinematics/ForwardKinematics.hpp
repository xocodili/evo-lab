#pragma once

#include "engine/kinematics/ForwardKinematicsScratch.hpp"
#include "engine/kinematics/KinematicLocalPose.hpp"
#include "engine/kinematics/KinematicNodeLookup.hpp"
#include "engine/kinematics/KinematicSkeleton.hpp"
#include "engine/kinematics/Math.hpp"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace evolab::engine::kinematics {

// Minimal pose type for games/tests that do not attach sim-specific node payloads.
struct KinematicNodePose {
  std::uint32_t id = 0;
  float worldX = 0.0f;
  float worldY = 0.0f;
  float worldZ = 0.0f;
};

namespace detail {

template <typename NodeLike, std::size_t Extent, typename HeightAtXZ>
bool solveForwardKinematicsImpl(const KinematicSkeleton& skeleton,
                                std::span<const float> jointYawDeltas, float rootWorldYaw,
                                std::span<NodeLike, Extent> nodes, HeightAtXZ&& heightAtXZ,
                                ForwardKinematicsScratch& scratch,
                                const NodeSpanIndex<NodeLike>& nodeIndex) {
  if (!skeleton.valid() || jointYawDeltas.size() != skeleton.jointCount()) {
    return false;
  }

  const std::size_t jointCount = skeleton.jointCount();
  scratch.ensureJointCount(jointCount);

  for (std::size_t jointIndex = 0; jointIndex < jointCount; ++jointIndex) {
    const KinematicSkeleton::Joint& joint = skeleton.joint(jointIndex);
    const std::size_t nodeIndexInSpan = nodeIndex.indexOf(joint.nodeId);
    if (nodeIndexInSpan == kInvalidNodeSpanIndex) {
      return false;
    }

    const float localYaw =
        joint.constraint.resolve(joint.bindLocalYaw, jointYawDeltas[jointIndex]);

    if (joint.parentIndex < 0) {
      scratch.worldYaw[jointIndex] = normalizeAngle(rootWorldYaw + localYaw);
      scratch.worldX[jointIndex] = nodes[nodeIndexInSpan].worldX;
      scratch.worldZ[jointIndex] = nodes[nodeIndexInSpan].worldZ;
    } else {
      const std::size_t parentIndex = static_cast<std::size_t>(joint.parentIndex);
      scratch.worldYaw[jointIndex] =
          normalizeAngle(scratch.worldYaw[parentIndex] + localYaw);
      scratch.worldX[jointIndex] =
          scratch.worldX[parentIndex] +
          std::sin(scratch.worldYaw[jointIndex]) * joint.restLength;
      scratch.worldZ[jointIndex] =
          scratch.worldZ[parentIndex] +
          std::cos(scratch.worldYaw[jointIndex]) * joint.restLength;
    }

    NodeLike& node = nodes[nodeIndexInSpan];
    node.worldX = scratch.worldX[jointIndex];
    node.worldZ = scratch.worldZ[jointIndex];
    node.worldY = heightAtXZ(scratch.worldX[jointIndex], scratch.worldZ[jointIndex]);
  }

  return true;
}

}  // namespace detail

// Hierarchical forward kinematics: local bind + pose deltas, rootWorldYaw at root only.
template <typename NodeLike, std::size_t Extent, typename HeightAtXZ>
bool solveForwardKinematics(const KinematicSkeleton& skeleton, const KinematicLocalPose& localPose,
                            float rootWorldYaw, std::span<NodeLike, Extent> nodes,
                            HeightAtXZ&& heightAtXZ, ForwardKinematicsScratch& scratch) {
  NodeSpanIndex<NodeLike> nodeIndex(nodes);
  return detail::solveForwardKinematicsImpl(
      skeleton, localPose.deltas(), rootWorldYaw, nodes,
      std::forward<HeightAtXZ>(heightAtXZ), scratch, nodeIndex);
}

template <typename NodeLike, std::size_t Extent, typename HeightAtXZ>
bool solveForwardKinematics(const KinematicSkeleton& skeleton, const KinematicLocalPose& localPose,
                            float rootWorldYaw, std::span<NodeLike, Extent> nodes,
                            HeightAtXZ&& heightAtXZ) {
  ForwardKinematicsScratch scratch;
  return solveForwardKinematics(skeleton, localPose, rootWorldYaw, nodes,
                                std::forward<HeightAtXZ>(heightAtXZ), scratch);
}

template <typename NodeLike, std::size_t Extent, typename HeightAtXZ>
bool solveForwardKinematicsFromDeltas(const KinematicSkeleton& skeleton,
                                        std::span<const float> jointYawDeltas, float rootWorldYaw,
                                        std::span<NodeLike, Extent> nodes, HeightAtXZ&& heightAtXZ,
                                        ForwardKinematicsScratch& scratch,
                                        const NodeSpanIndex<NodeLike>& nodeIndex) {
  return detail::solveForwardKinematicsImpl(
      skeleton, jointYawDeltas, rootWorldYaw, nodes, std::forward<HeightAtXZ>(heightAtXZ),
      scratch, nodeIndex);
}

template <typename NodeLike, std::size_t Extent>
void translateNodesXZ(std::span<NodeLike, Extent> nodes, float dx, float dz) {
  for (NodeLike& node : nodes) {
    node.worldX += dx;
    node.worldZ += dz;
  }
}

// Legacy convenience: build a tree skeleton from bones and solve with zero pose deltas.
template <typename NodeLike, std::size_t Extent, typename BoneLike, typename HeightAtXZ>
bool solveTreeForwardKinematics(std::span<NodeLike, Extent> nodes,
                                std::span<const BoneLike> bones, std::uint32_t rootNodeId,
                                float rootWorldYaw, HeightAtXZ&& heightAtXZ) {
  std::vector<KinematicBone> engineBones;
  engineBones.reserve(bones.size());
  for (const BoneLike& bone : bones) {
    KinematicBone converted;
    converted.parentNodeId = bone.parentNodeId;
    converted.childNodeId = bone.childNodeId;
    converted.restLength = bone.restLength;
    converted.jointAngle = bone.jointAngle;
    engineBones.push_back(converted);
  }

  const KinematicSkeleton skeleton = KinematicSkeleton::buildFromBones(engineBones, rootNodeId);
  if (!skeleton.valid()) {
    return false;
  }

  const KinematicLocalPose localPose = KinematicLocalPose::zeros(skeleton.jointCount());
  return solveForwardKinematics(skeleton, localPose, rootWorldYaw, nodes,
                                std::forward<HeightAtXZ>(heightAtXZ));
}

inline bool solveTreeForwardKinematicsFlat(std::span<KinematicNodePose> nodes,
                                           std::span<const KinematicBone> bones,
                                           std::uint32_t rootNodeId, float rootWorldYaw,
                                           float worldY = 0.0f) {
  return solveTreeForwardKinematics(nodes, bones, rootNodeId, rootWorldYaw,
                                    [worldY](float /*x*/, float /*z*/) { return worldY; });
}

}  // namespace evolab::engine::kinematics
