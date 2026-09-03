#pragma once

#include "sim/Organism.hpp"

namespace evolab {

class BarrenWorld;

// Apply local stem records → world link joint angles for a spawn/root yaw.
void reconcileLinkJointAnglesFromSpawnYaw(Organism& organism, float spawnWorldYaw);

// Expand collapsed spawn stacks into rest-length chain / hub layout (XZ; Y unchanged).
void layoutNodesFromSkeletonLinks(Organism& organism);

// Birth pose: bodyDynamics seed, link reconcile, node layout, rest FK (no dynamics step).
void initializeArticulatedSpawnPose(Organism& organism, const BarrenWorld& world, float cellSize,
                                    float heightScale, float spawnWorldYaw);

}  // namespace evolab
