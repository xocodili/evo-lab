#pragma once

#include "engine/kinematics/KinematicLocalPose.hpp"
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

template <typename NodeRange>
std::size_t nodeIndexById(const NodeRange& nodes, std::uint32_t nodeId) {
  for (std::size_t i = 0; i < nodes.size(); ++i) {
    if (nodes[i].id == nodeId) {
      return i;
    }
  }
  return nodes.size();
}

}  // namespace detail

// Hierarchical forward kinematics: local bind + pose deltas, rootWorldYaw at root only.
template <typename NodeLike, std::size_t Extent, typename HeightAtXZ>
bool solveForwardKinematics(const KinematicSkeleton& skeleton, const KinematicLocalPose& localPose,
                            float rootWorldYaw, std::span<NodeLike, Extent> nodes,
                            HeightAtXZ&& heightAtXZ) {
  if (!skeleton.valid() || localPose.size() != skeleton.jointCount()) {
    return false;
  }

  const std::size_t jointCount = skeleton.jointCount();
  std::vector<float> worldYaw(jointCount, 0.0f);
  std::vector<float> worldX(jointCount, 0.0f);
  std::vector<float> worldZ(jointCount, 0.0f);

  for (std::size_t jointIndex = 0; jointIndex < jointCount; ++jointIndex) {
    const KinematicSkeleton::Joint& joint = skeleton.joint(jointIndex);
    const std::size_t nodeIndex = detail::nodeIndexById(nodes, joint.nodeId);
    if (nodeIndex >= nodes.size()) {
      return false;
    }

    const float localYaw =
        joint.constraint.resolve(joint.bindLocalYaw, localPose.yawDelta(jointIndex));

    if (joint.parentIndex < 0) {
      worldYaw[jointIndex] = normalizeAngle(rootWorldYaw + localYaw);
      worldX[jointIndex] = nodes[nodeIndex].worldX;
      worldZ[jointIndex] = nodes[nodeIndex].worldZ;
    } else {
      const std::size_t parentIndex = static_cast<std::size_t>(joint.parentIndex);
      worldYaw[jointIndex] = normalizeAngle(worldYaw[parentIndex] + localYaw);
      worldX[jointIndex] =
          worldX[parentIndex] + std::sin(worldYaw[jointIndex]) * joint.restLength;
      worldZ[jointIndex] =
          worldZ[parentIndex] + std::cos(worldYaw[jointIndex]) * joint.restLength;
    }

    NodeLike& node = nodes[nodeIndex];
    node.worldX = worldX[jointIndex];
    node.worldZ = worldZ[jointIndex];
    node.worldY = heightAtXZ(worldX[jointIndex], worldZ[jointIndex]);
  }

  return true;
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
                                float heading, HeightAtXZ&& heightAtXZ) {
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
  return solveForwardKinematics(skeleton, localPose, heading, nodes,
                                std::forward<HeightAtXZ>(heightAtXZ));
}

inline bool solveTreeForwardKinematicsFlat(std::span<KinematicNodePose> nodes,
                                           std::span<const KinematicBone> bones,
                                           std::uint32_t rootNodeId, float heading,
                                           float worldY = 0.0f) {
  return solveTreeForwardKinematics(nodes, bones, rootNodeId, heading,
                                    [worldY](float /*x*/, float /*z*/) { return worldY; });
}

}  // namespace evolab::engine::kinematics
