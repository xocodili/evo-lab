#include "sim/Organism.hpp"

#include "engine/kinematics/ArticulatedDynamics.hpp"
#include "engine/kinematics/ForwardKinematics.hpp"
#include "engine/kinematics/KinematicBone.hpp"
#include "engine/kinematics/KinematicLocalPose.hpp"
#include "engine/kinematics/KinematicSkeleton.hpp"
#include "sim/BarrenWorld.hpp"
#include "sim/CellConstants.hpp"
#include "sim/NeuronMusculature.hpp"
#include "sim/WaterColumn.hpp"

#include <span>
#include <utility>
#include <vector>

namespace evolab {

namespace {

engine::kinematics::KinematicSkeleton buildEngineSkeleton(Organism& organism) {
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
  engine::kinematics::KinematicSkeleton skeleton =
      engine::kinematics::KinematicSkeleton::buildFromBones(bones, organism.rootNodeId);
  if (organism.isCampNom()) {
    applyCampJointFlexLimits(skeleton);
  }
  return skeleton;
}

auto makeHeightAtXZ(const BarrenWorld& world, float cellSize, float heightScale) {
  return [&](float x, float z) {
    const WaterColumn column = sampleWaterColumn(world, x, z, cellSize, heightScale);
    return placementY(column, NomHabitat::Surface);
  };
}

engine::kinematics::KinematicSkeleton takeKinematicsSkeleton(Organism& organism) {
  if (organism.kinematicsSkeletonScratchValid_) {
    organism.kinematicsSkeletonScratchValid_ = false;
    return std::move(organism.kinematicsSkeletonScratch_);
  }
  return buildEngineSkeleton(organism);
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

  engine::kinematics::ArticulatedStepParams params;
  params.tideVelX = organism.lastTideVelX;
  params.tideVelZ = organism.lastTideVelZ;
  params.linearDrag = kBodyLinearDrag;
  params.yawDamping = kBodyYawDamping;
  params.invMass = kBodyInvMass;
  params.invInertia = kBodyInvInertia;

  const auto heightAtXZ = makeHeightAtXZ(world, cellSize, heightScale);

  engine::kinematics::stepArticulatedBody(
      skeleton, organism.bodyDynamics, organism.heading, std::span(organism.nodes), muscles,
      impulses, params, heightAtXZ);

  organism.pendingImpulseNodeId = 0;
  organism.pendingImpulseX = 0.0f;
  organism.pendingImpulseZ = 0.0f;
  organism.lastTideVelX = 0.0f;
  organism.lastTideVelZ = 0.0f;
  organism.lastActuatorStrokeFlexBoost = 0.0f;
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

  engine::kinematics::solveForwardKinematics(skeleton, pose, organism.heading,
                                             std::span(organism.nodes), heightAtXZ);
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

}  // namespace

void Organism::syncKinematicsPose(const BarrenWorld& world, float cellSize, float heightScale) {
  engine::kinematics::KinematicSkeleton skeleton = takeKinematicsSkeleton(*this);
  if (!skeleton.valid()) {
    return;
  }
  if (isCampNom()) {
    syncCampKinematicsPose(*this, skeleton, world, cellSize, heightScale);
    return;
  }
  runStaticForwardKinematics(*this, skeleton, world, cellSize, heightScale);
}

void Organism::updateKinematics(const BarrenWorld& world, float cellSize, float heightScale) {
  kinematicsSkeletonScratch_ = buildEngineSkeleton(*this);
  kinematicsSkeletonScratchValid_ = true;
  const engine::kinematics::KinematicSkeleton& skeleton = kinematicsSkeletonScratch_;
  if (!skeleton.valid()) {
    kinematicsSkeletonScratchValid_ = false;
    return;
  }

  if (isCampNom()) {
    stepCampBodyDynamics(*this, skeleton, world, cellSize, heightScale);
    return;
  }

  runStaticForwardKinematics(*this, skeleton, world, cellSize, heightScale);
}

}  // namespace evolab
