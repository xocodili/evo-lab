#include "sim/Organism.hpp"

#include "engine/kinematics/ArticulatedDynamics.hpp"
#include "engine/kinematics/ForwardKinematics.hpp"
#include "engine/kinematics/KinematicBone.hpp"
#include "engine/kinematics/KinematicLocalPose.hpp"
#include "engine/kinematics/KinematicSkeleton.hpp"
#include "engine/kinematics/Math.hpp"
#include "engine/kinematics/NodeMediumDrag.hpp"
#include "sim/BarrenWorld.hpp"
#include "sim/CampTopology.hpp"
#include "sim/CellConstants.hpp"
#include "sim/NeuronMusculature.hpp"
#include "sim/TideAdvection.hpp"
#include "sim/WaterColumn.hpp"

#include <algorithm>
#include <cmath>
#include <span>
#include <unordered_map>
#include <utility>
#include <vector>

namespace evolab {

namespace {

bool skeletonHasMuscleBundles(const Organism& organism) {
  for (const SkeletonLink& link : organism.links) {
    if (link.muscleBundle) {
      return true;
    }
  }
  return false;
}

auto makeHeightAtXZ(const BarrenWorld& world, float cellSize, float heightScale) {
  return [&](float x, float z) {
    const WaterColumn column = sampleWaterColumn(world, x, z, cellSize, heightScale);
    return placementY(column, NomHabitat::Surface);
  };
}

std::uint32_t findAuthoredSkeletonTailRoot(const Organism& organism) {
  std::vector<std::uint32_t> childIds;
  childIds.reserve(organism.links.size());
  for (const SkeletonLink& link : organism.links) {
    childIds.push_back(link.childNodeId);
  }
  for (const SkeletonLink& link : organism.links) {
    if (std::find(childIds.begin(), childIds.end(), link.parentNodeId) != childIds.end()) {
      continue;
    }
    const SkeletonNode* parent = organism.findNode(link.parentNodeId);
    if (parent != nullptr && parent->alive) {
      return link.parentNodeId;
    }
  }
  return organism.rootNodeId;
}

std::vector<engine::kinematics::KinematicBone> buildAuthoredBones(const Organism& organism) {
  std::vector<engine::kinematics::KinematicBone> bones;
  bones.reserve(organism.links.size());
  for (const SkeletonLink& link : organism.links) {
    engine::kinematics::KinematicBone bone;
    bone.parentNodeId = link.parentNodeId;
    bone.childNodeId = link.childNodeId;
    bone.restLength = link.restLength;
    bone.jointAngle = link.jointAngle;
    bones.push_back(bone);
  }
  return bones;
}

std::vector<engine::kinematics::KinematicNodePose> computeRestNodePosesAnchoredAtRoot(
    const Organism& organism, float cellSize, float heightScale, const BarrenWorld& world) {
  const std::vector<engine::kinematics::KinematicBone> authored = buildAuthoredBones(organism);
  std::vector<engine::kinematics::KinematicNodePose> poses;
  poses.reserve(organism.nodes.size());
  for (const SkeletonNode& node : organism.nodes) {
    if (!node.alive) {
      continue;
    }
    poses.push_back({node.id, 0.0f, 0.0f, 0.0f});
  }
  if (authored.empty() || poses.empty()) {
    return poses;
  }

  const std::uint32_t tailRoot = findAuthoredSkeletonTailRoot(organism);
  engine::kinematics::KinematicSkeleton refSkeleton =
      engine::kinematics::KinematicSkeleton::buildFromBones(authored, tailRoot);
  if (!refSkeleton.valid()) {
    return poses;
  }
  if (skeletonHasMuscleBundles(organism)) {
    applyCampJointFlexLimits(refSkeleton);
  }

  const auto heightAtXZ = makeHeightAtXZ(world, cellSize, heightScale);
  const engine::kinematics::KinematicLocalPose zeroPose =
      engine::kinematics::KinematicLocalPose::zeros(refSkeleton.jointCount());
  engine::kinematics::solveForwardKinematics(refSkeleton, zeroPose, organism.heading,
                                             std::span(poses), heightAtXZ);

  const SkeletonNode* rootNode = organism.findNode(organism.rootNodeId);
  if (rootNode == nullptr) {
    return poses;
  }

  std::size_t rootPoseIndex = poses.size();
  for (std::size_t i = 0; i < poses.size(); ++i) {
    if (poses[i].id == organism.rootNodeId) {
      rootPoseIndex = i;
      break;
    }
  }
  if (rootPoseIndex >= poses.size()) {
    return poses;
  }

  const float dx = rootNode->worldX - poses[rootPoseIndex].worldX;
  const float dz = rootNode->worldZ - poses[rootPoseIndex].worldZ;
  for (engine::kinematics::KinematicNodePose& pose : poses) {
    pose.worldX += dx;
    pose.worldZ += dz;
  }
  return poses;
}

const engine::kinematics::KinematicNodePose* findRestPose(
    const std::vector<engine::kinematics::KinematicNodePose>& poses, std::uint32_t nodeId) {
  for (const engine::kinematics::KinematicNodePose& pose : poses) {
    if (pose.id == nodeId) {
      return &pose;
    }
  }
  return nullptr;
}

std::vector<engine::kinematics::KinematicBone> buildKinematicBonesFromRoot(
    const Organism& organism, std::uint32_t rootId, float rootWorldYaw,
    const std::vector<engine::kinematics::KinematicNodePose>& restPoses) {
  std::vector<engine::kinematics::KinematicBone> bones;
  if (organism.findNode(rootId) == nullptr) {
    return bones;
  }

  std::unordered_map<std::uint32_t, float> worldYawById;
  worldYawById[rootId] = rootWorldYaw;

  std::vector<std::uint32_t> visited;
  visited.push_back(rootId);
  std::vector<std::uint32_t> queue;
  queue.push_back(rootId);

  while (!queue.empty()) {
    const std::uint32_t current = queue.front();
    queue.erase(queue.begin());

    const float parentWorldYaw = worldYawById[current];
    const engine::kinematics::KinematicNodePose* parentPose = findRestPose(restPoses, current);
    if (parentPose == nullptr) {
      continue;
    }

    for (const SkeletonLink& link : organism.links) {
      std::uint32_t neighbor = 0;
      if (link.parentNodeId == current) {
        neighbor = link.childNodeId;
      } else if (link.childNodeId == current) {
        neighbor = link.parentNodeId;
      } else {
        continue;
      }

      if (std::find(visited.begin(), visited.end(), neighbor) != visited.end()) {
        continue;
      }
      const SkeletonNode* neighborNode = organism.findNode(neighbor);
      const engine::kinematics::KinematicNodePose* neighborPose = findRestPose(restPoses, neighbor);
      if (neighborNode == nullptr || !neighborNode->alive || neighborPose == nullptr) {
        continue;
      }

      const float dx = neighborPose->worldX - parentPose->worldX;
      const float dz = neighborPose->worldZ - parentPose->worldZ;
      const float edgeYaw = std::atan2(dx, dz);
      const float bindLocalYaw =
          engine::kinematics::normalizeAngle(edgeYaw - parentWorldYaw);

      engine::kinematics::KinematicBone bone;
      bone.parentNodeId = current;
      bone.childNodeId = neighbor;
      bone.restLength = link.restLength;
      bone.jointAngle = bindLocalYaw;
      bones.push_back(bone);

      worldYawById[neighbor] =
          engine::kinematics::normalizeAngle(parentWorldYaw + bindLocalYaw);
      visited.push_back(neighbor);
      queue.push_back(neighbor);
    }
  }

  return bones;
}

engine::kinematics::KinematicSkeleton buildEngineSkeleton(Organism& organism,
                                                          const BarrenWorld& world,
                                                          float cellSize, float heightScale) {
  ensureKinematicRootNodeId(organism);

  if (organismUsesMouthKinematicRoot(organism)) {
    const std::vector<engine::kinematics::KinematicNodePose> restPoses =
        computeRestNodePosesAnchoredAtRoot(organism, cellSize, heightScale, world);

    const std::vector<engine::kinematics::KinematicBone> bones =
        buildKinematicBonesFromRoot(organism, organism.rootNodeId, organism.heading, restPoses);
    engine::kinematics::KinematicSkeleton skeleton =
        engine::kinematics::KinematicSkeleton::buildFromBones(bones, organism.rootNodeId);
    if (skeletonHasMuscleBundles(organism)) {
      applyCampJointFlexLimits(skeleton);
    }
    return skeleton;
  }

  const std::vector<engine::kinematics::KinematicBone> authored = buildAuthoredBones(organism);
  engine::kinematics::KinematicSkeleton skeleton =
      engine::kinematics::KinematicSkeleton::buildFromBones(authored, organism.rootNodeId);
  if (skeletonHasMuscleBundles(organism)) {
    applyCampJointFlexLimits(skeleton);
  }
  return skeleton;
}

engine::kinematics::KinematicSkeleton takeKinematicsSkeleton(Organism& organism,
                                                             const BarrenWorld& world,
                                                             float cellSize, float heightScale) {
  if (organism.kinematicsSkeletonScratchValid_) {
    organism.kinematicsSkeletonScratchValid_ = false;
    return std::move(organism.kinematicsSkeletonScratch_);
  }
  return buildEngineSkeleton(organism, world, cellSize, heightScale);
}

void stepCampBodyDynamics(Organism& organism, const engine::kinematics::KinematicSkeleton& skeleton,
                          const BarrenWorld& world, float cellSize, float heightScale) {
  organism.bodyDynamics.ensureJointCount(skeleton.jointCount());

  std::vector<engine::kinematics::MuscleCommand> muscles =
      buildMuscleCommands(organism, skeleton);

  std::vector<engine::kinematics::ExternalImpulse> impulses;
  if (organism.pendingImpulseNodeId != 0 &&
      (organism.pendingImpulseX != 0.0f || organism.pendingImpulseZ != 0.0f)) {
    engine::kinematics::ExternalImpulse impulse;
    impulse.nodeId = organism.pendingImpulseNodeId;
    impulse.impulseX = organism.pendingImpulseX;
    impulse.impulseZ = organism.pendingImpulseZ;
    impulses.push_back(impulse);
  }

  // Bridge: sim heading mirrors body state until camper init owns spawn/root pose (layer 2).
  organism.bodyDynamics.rootWorldYaw = organism.heading;

  engine::kinematics::ArticulatedStepParams params;
  params.mediumVelX = organism.lastTideVelX;
  params.mediumVelZ = organism.lastTideVelZ;
  params.linearDrag = kBodyLinearDrag;
  params.nodeLinearDrag = kBodyNodeLinearDrag;
  params.nodeDragDepthGain = kBodyNodeDragDepthGain;
  params.yawDamping = kBodyYawDamping;
  params.invMass = kBodyInvMass;
  params.invInertia = kBodyInvInertia;
  params.solveBoneConstraints = true;
  params.boneConstraintIterations = kBoneDistanceConstraintIterations;
  params.boneConstraintStiffness = 1.0f;

  const auto heightAtXZ = makeHeightAtXZ(world, cellSize, heightScale);

  engine::kinematics::stepArticulatedBody(skeleton, organism.bodyDynamics,
                                          std::span(organism.nodes), muscles, impulses, params,
                                          heightAtXZ);

  organism.heading = organism.bodyDynamics.rootWorldYaw;

  organism.pendingImpulseNodeId = 0;
  organism.pendingImpulseX = 0.0f;
  organism.pendingImpulseZ = 0.0f;
  organism.lastTideVelX = 0.0f;
  organism.lastTideVelZ = 0.0f;
}

void syncCampKinematicsPose(Organism& organism, const engine::kinematics::KinematicSkeleton& skeleton,
                            const BarrenWorld& world, float cellSize, float heightScale) {
  engine::kinematics::KinematicLocalPose pose =
      engine::kinematics::KinematicLocalPose::zeros(skeleton.jointCount());
  organism.bodyDynamics.ensureJointCount(skeleton.jointCount());
  for (std::size_t jointIndex = 0; jointIndex < skeleton.jointCount(); ++jointIndex) {
    pose.yawDelta(jointIndex) = organism.bodyDynamics.jointYawDelta[jointIndex];
  }

  const auto heightAtXZ = makeHeightAtXZ(world, cellSize, heightScale);

  engine::kinematics::solveForwardKinematics(skeleton, pose, organism.bodyDynamics.rootWorldYaw,
                                             std::span(organism.nodes), heightAtXZ,
                                             organism.bodyDynamics.fkScratch);
}

void runStaticForwardKinematics(Organism& organism,
                                const engine::kinematics::KinematicSkeleton& skeleton,
                                const BarrenWorld& world, float cellSize, float heightScale) {
  engine::kinematics::KinematicLocalPose localPose =
      engine::kinematics::KinematicLocalPose::zeros(skeleton.jointCount());
  const auto heightAtXZ = makeHeightAtXZ(world, cellSize, heightScale);
  engine::kinematics::solveForwardKinematics(skeleton, localPose, organism.heading,
                                             std::span(organism.nodes), heightAtXZ);
}

void refreshNodeHeightsFromTerrain(Organism& organism,
                                   const engine::kinematics::KinematicSkeleton& skeleton,
                                   const BarrenWorld& world, float cellSize, float heightScale) {
  const auto heightAtXZ = makeHeightAtXZ(world, cellSize, heightScale);
  engine::kinematics::refreshNodeWorldY(skeleton, std::span(organism.nodes), heightAtXZ);
}

}  // namespace

bool Organism::usesArticulatedLocomotion() const {
  return organismUsesArticulatedLocomotion(*this);
}

void Organism::finalizeKinematicsBoundary(const BarrenWorld& world, float cellSize,
                                          float heightScale, float halfExtent) {
  SkeletonNode* root = findNode(rootNodeId);
  if (root == nullptr) {
    return;
  }

  const float beforeX = root->worldX;
  const float beforeZ = root->worldZ;
  clampWorldPosition(root->worldX, root->worldZ, halfExtent, cellSize * 0.25f);
  const float deltaX = root->worldX - beforeX;
  const float deltaZ = root->worldZ - beforeZ;

  if (usesArticulatedLocomotion()) {
    if (deltaX != 0.0f || deltaZ != 0.0f) {
      for (SkeletonNode& node : nodes) {
        node.worldX += deltaX;
        node.worldZ += deltaZ;
      }
      engine::kinematics::KinematicSkeleton skeleton =
          takeKinematicsSkeleton(*this, world, cellSize, heightScale);
      if (skeleton.valid()) {
        refreshNodeHeightsFromTerrain(*this, skeleton, world, cellSize, heightScale);
      }
    }
    return;
  }

  syncKinematicsPose(world, cellSize, heightScale);
}

void Organism::syncKinematicsPose(const BarrenWorld& world, float cellSize, float heightScale) {
  engine::kinematics::KinematicSkeleton skeleton =
      takeKinematicsSkeleton(*this, world, cellSize, heightScale);
  if (!skeleton.valid()) {
    return;
  }
  if (usesArticulatedLocomotion()) {
    syncCampKinematicsPose(*this, skeleton, world, cellSize, heightScale);
    return;
  }
  runStaticForwardKinematics(*this, skeleton, world, cellSize, heightScale);
}

void Organism::updateKinematics(const BarrenWorld& world, float cellSize, float heightScale) {
  kinematicsSkeletonScratch_ = buildEngineSkeleton(*this, world, cellSize, heightScale);
  kinematicsSkeletonScratchValid_ = true;
  const engine::kinematics::KinematicSkeleton& skeleton = kinematicsSkeletonScratch_;
  if (!skeleton.valid()) {
    kinematicsSkeletonScratchValid_ = false;
    return;
  }

  if (usesArticulatedLocomotion()) {
    stepCampBodyDynamics(*this, skeleton, world, cellSize, heightScale);
    return;
  }

  runStaticForwardKinematics(*this, skeleton, world, cellSize, heightScale);
}

}  // namespace evolab
