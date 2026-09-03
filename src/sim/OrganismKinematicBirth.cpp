#include "sim/OrganismKinematicBirth.hpp"

#include "engine/kinematics/Math.hpp"
#include "sim/BarrenWorld.hpp"
#include "sim/CampTopology.hpp"
#include "sim/StemBinding.hpp"
#include "sim/WaterColumn.hpp"
#include "sim/WorldBinding.hpp"

#include <cmath>
#include <unordered_set>

namespace evolab {

namespace {

using engine::kinematics::normalizeAngle;

void refreshNodeHeightsFromTerrain(Organism& organism, const BarrenWorld& world, float cellSize,
                                   float heightScale) {
  for (SkeletonNode& node : organism.nodes) {
    if (!node.alive) {
      continue;
    }
    const WaterColumn column = sampleWaterColumn(world, node.worldX, node.worldZ, cellSize, heightScale);
    node.worldY = placementY(column, NomHabitat::Surface);
  }
}

void resetArticulatedBodyStateAtSpawn(Organism& organism, float spawnWorldYaw) {
  const float yaw = normalizeAngle(spawnWorldYaw);
  organism.bodyDynamics.rootWorldYaw = yaw;
  organism.bodyDynamics.rootVelX = 0.0f;
  organism.bodyDynamics.rootVelZ = 0.0f;
  organism.bodyDynamics.rootYawRate = 0.0f;
  organism.heading = yaw;
}

}  // namespace

void reconcileLinkJointAnglesFromSpawnYaw(Organism& organism, float spawnWorldYaw) {
  const float yaw = normalizeAngle(spawnWorldYaw);
  for (SkeletonLink& link : organism.links) {
    bool matched = false;
    for (const StemChainRecord& record : organism.stemAssembly.chains) {
      if (record.parentNodeId != link.parentNodeId || record.childNodeId != link.childNodeId) {
        continue;
      }
      link.jointAngle = normalizeAngle(yaw + record.segmentAngleOffset);
      matched = true;
      break;
    }
    if (matched) {
      continue;
    }
    for (const StemBindRecord& record : organism.stemAssembly.binds) {
      if (record.hubNodeId != link.parentNodeId || record.peripheralNodeId != link.childNodeId) {
        continue;
      }
      link.jointAngle = hubSocketAngleRad(yaw, record.hubSlot);
      break;
    }
  }
}

void layoutNodesFromSkeletonLinks(Organism& organism) {
  SkeletonNode* anchor = organism.findNode(organism.rootNodeId);
  if (anchor == nullptr) {
    return;
  }

  std::unordered_set<std::uint32_t> placed;
  placed.insert(anchor->id);

  auto placeFromChildToParent = [&](const SkeletonLink& link) {
    if (!placed.count(link.childNodeId) || placed.count(link.parentNodeId)) {
      return false;
    }
    const SkeletonNode* child = organism.findNode(link.childNodeId);
    SkeletonNode* parent = organism.findNode(link.parentNodeId);
    if (child == nullptr || parent == nullptr || !child->alive || !parent->alive) {
      return false;
    }
    parent->worldX = child->worldX - std::sin(link.jointAngle) * link.restLength;
    parent->worldZ = child->worldZ - std::cos(link.jointAngle) * link.restLength;
    parent->worldY = child->worldY;
    placed.insert(link.parentNodeId);
    return true;
  };

  auto placeFromParentToChild = [&](const SkeletonLink& link) {
    if (!placed.count(link.parentNodeId) || placed.count(link.childNodeId)) {
      return false;
    }
    const SkeletonNode* parent = organism.findNode(link.parentNodeId);
    SkeletonNode* child = organism.findNode(link.childNodeId);
    if (parent == nullptr || child == nullptr || !parent->alive || !child->alive) {
      return false;
    }
    child->worldX = parent->worldX + std::sin(link.jointAngle) * link.restLength;
    child->worldZ = parent->worldZ + std::cos(link.jointAngle) * link.restLength;
    child->worldY = parent->worldY;
    placed.insert(link.childNodeId);
    return true;
  };

  bool progress = true;
  while (progress) {
    progress = false;
    for (const SkeletonLink& link : organism.links) {
      if (placeFromChildToParent(link) || placeFromParentToChild(link)) {
        progress = true;
      }
    }
  }
}

void initializeArticulatedSpawnPose(Organism& organism, const BarrenWorld& world, float cellSize,
                                    float heightScale, float spawnWorldYaw) {
  ensureKinematicRootNodeId(organism);
  resetArticulatedBodyStateAtSpawn(organism, spawnWorldYaw);
  reconcileLinkJointAnglesFromSpawnYaw(organism, spawnWorldYaw);
  layoutNodesFromSkeletonLinks(organism);
  refreshNodeHeightsFromTerrain(organism, world, cellSize, heightScale);

  organism.bodyDynamics.jointYawDelta.clear();
  organism.bodyDynamics.jointYawVel.clear();
  organism.kinematicsBirthApplied_ = true;

  organism.syncKinematicsPose(world, cellSize, heightScale);
}

}  // namespace evolab
