#pragma once

#include "engine/kinematics/KinematicNodeLookup.hpp"
#include "engine/kinematics/KinematicSkeleton.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <span>

namespace evolab::engine::kinematics {

namespace node_medium_drag_detail {

template <typename NodeLike, std::size_t Extent>
void applyPerNodeMediumDragIndexed(const KinematicSkeleton& skeleton,
                                   std::span<NodeLike, Extent> nodes,
                                   const NodeSpanIndex<NodeLike>& nodeIndex, float mediumVelX,
                                   float mediumVelZ, float nodeLinearDrag, float depthGain) {
  const float drag = std::clamp(nodeLinearDrag, 0.0f, 1.0f);
  const float gain = std::max(0.0f, depthGain);
  const std::vector<int>& depth = skeleton.jointDepths();

  for (std::size_t jointIndex = 0; jointIndex < skeleton.jointCount(); ++jointIndex) {
    if (depth[jointIndex] <= 0) {
      continue;
    }

    const KinematicSkeleton::Joint& joint = skeleton.joint(jointIndex);
    const std::size_t nodeIndexInSpan = nodeIndex.indexOf(joint.nodeId);
    if (nodeIndexInSpan == kInvalidNodeSpanIndex) {
      continue;
    }

    const float weight = 1.0f + static_cast<float>(depth[jointIndex]) * gain;
    NodeLike& node = nodes[nodeIndexInSpan];
    node.worldX += mediumVelX * drag * weight;
    node.worldZ += mediumVelZ * drag * weight;
  }
}

template <typename NodeLike, std::size_t Extent, typename HeightAtXZ>
void refreshNodeWorldYIndexed(const KinematicSkeleton& skeleton, std::span<NodeLike, Extent> nodes,
                              const NodeSpanIndex<NodeLike>& nodeIndex, HeightAtXZ&& heightAtXZ) {
  for (std::size_t jointIndex = 0; jointIndex < skeleton.jointCount(); ++jointIndex) {
    const KinematicSkeleton::Joint& joint = skeleton.joint(jointIndex);
    const std::size_t nodeIndexInSpan = nodeIndex.indexOf(joint.nodeId);
    if (nodeIndexInSpan == kInvalidNodeSpanIndex) {
      continue;
    }
    NodeLike& node = nodes[nodeIndexInSpan];
    node.worldY = heightAtXZ(node.worldX, node.worldZ);
  }
}

}  // namespace node_medium_drag_detail

// Couple non-root nodes to ambient medium flow. Distal joints (higher depth) feel more drag so
// tide/current weathervanes the chain without named keel logic.
template <typename NodeLike, std::size_t Extent>
void applyPerNodeMediumDrag(const KinematicSkeleton& skeleton, std::span<NodeLike, Extent> nodes,
                            const NodeSpanIndex<NodeLike>& nodeIndex, float mediumVelX,
                            float mediumVelZ, float nodeLinearDrag, float depthGain) {
  if (!skeleton.valid() || nodeLinearDrag <= 0.0f) {
    return;
  }
  if (std::abs(mediumVelX) <= 1.0e-8f && std::abs(mediumVelZ) <= 1.0e-8f) {
    return;
  }

  node_medium_drag_detail::applyPerNodeMediumDragIndexed(
      skeleton, nodes, nodeIndex, mediumVelX, mediumVelZ, nodeLinearDrag, depthGain);
}

template <typename NodeLike, std::size_t Extent>
void applyPerNodeMediumDrag(const KinematicSkeleton& skeleton, std::span<NodeLike, Extent> nodes,
                            float mediumVelX, float mediumVelZ, float nodeLinearDrag,
                            float depthGain) {
  if (!skeleton.valid() || nodeLinearDrag <= 0.0f) {
    return;
  }
  if (std::abs(mediumVelX) <= 1.0e-8f && std::abs(mediumVelZ) <= 1.0e-8f) {
    return;
  }

  const NodeSpanIndex<NodeLike> nodeIndex(nodes);
  node_medium_drag_detail::applyPerNodeMediumDragIndexed(
      skeleton, nodes, nodeIndex, mediumVelX, mediumVelZ, nodeLinearDrag, depthGain);
}

template <typename NodeLike, std::size_t Extent, typename HeightAtXZ>
void refreshNodeWorldY(const KinematicSkeleton& skeleton, std::span<NodeLike, Extent> nodes,
                       const NodeSpanIndex<NodeLike>& nodeIndex, HeightAtXZ&& heightAtXZ) {
  if (!skeleton.valid()) {
    return;
  }

  node_medium_drag_detail::refreshNodeWorldYIndexed(skeleton, nodes, nodeIndex,
                                                    std::forward<HeightAtXZ>(heightAtXZ));
}

template <typename NodeLike, std::size_t Extent, typename HeightAtXZ>
void refreshNodeWorldY(const KinematicSkeleton& skeleton, std::span<NodeLike, Extent> nodes,
                       HeightAtXZ&& heightAtXZ) {
  if (!skeleton.valid()) {
    return;
  }

  const NodeSpanIndex<NodeLike> nodeIndex(nodes);
  node_medium_drag_detail::refreshNodeWorldYIndexed(skeleton, nodes, nodeIndex,
                                                    std::forward<HeightAtXZ>(heightAtXZ));
}

}  // namespace evolab::engine::kinematics
