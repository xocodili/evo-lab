#include "sim/Organism.hpp"

#include "sim/BarrenWorld.hpp"
#include "sim/WaterColumn.hpp"

#include <cmath>
#include <vector>

namespace evolab {
void Organism::updateKinematics(const BarrenWorld& world, float cellSize, float heightScale) {
  SkeletonNode* root = findNode(rootNodeId);
  if (root == nullptr) {
    return;
  }

  const WaterColumn rootColumn =
      sampleWaterColumn(world, root->worldX, root->worldZ, cellSize, heightScale);
  root->worldY = placementY(rootColumn, NomHabitat::Surface);

  std::vector<bool> placed(nodes.size(), false);
  auto nodeIndex = [this](std::uint32_t id) -> std::size_t {
    for (std::size_t i = 0; i < nodes.size(); ++i) {
      if (nodes[i].id == id) {
        return i;
      }
    }
    return nodes.size();
  };
  placed[nodeIndex(rootNodeId)] = true;

  bool progress = true;
  while (progress) {
    progress = false;
    for (const SkeletonLink& link : links) {
      const std::size_t parentIdx = nodeIndex(link.parentNodeId);
      const std::size_t childIdx = nodeIndex(link.childNodeId);
      if (parentIdx >= nodes.size() || childIdx >= nodes.size()) {
        continue;
      }
      if (!placed[parentIdx] || placed[childIdx]) {
        continue;
      }

      const SkeletonNode& parent = nodes[parentIdx];
      SkeletonNode& child = nodes[childIdx];
      const float angle = link.jointAngle + heading;
      child.worldX = parent.worldX + std::sin(angle) * link.restLength;
      child.worldZ = parent.worldZ + std::cos(angle) * link.restLength;
      const WaterColumn childColumn =
          sampleWaterColumn(world, child.worldX, child.worldZ, cellSize, heightScale);
      child.worldY = placementY(childColumn, NomHabitat::Surface);
      placed[childIdx] = true;
      progress = true;
    }
  }
}
}

