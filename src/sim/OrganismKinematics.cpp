#include "sim/Organism.hpp"

#include "engine/kinematics/ForwardKinematics.hpp"
#include "engine/kinematics/KinematicBone.hpp"
#include "engine/kinematics/KinematicLocalPose.hpp"
#include "engine/kinematics/KinematicSkeleton.hpp"
#include "sim/BarrenWorld.hpp"
#include "sim/WaterColumn.hpp"

#include <span>
#include <vector>

namespace evolab {

namespace {

engine::kinematics::KinematicSkeleton buildEngineSkeleton(const Organism& organism) {
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
  return engine::kinematics::KinematicSkeleton::buildFromBones(bones, organism.rootNodeId);
}

}  // namespace

void Organism::updateKinematics(const BarrenWorld& world, float cellSize, float heightScale) {
  const engine::kinematics::KinematicSkeleton skeleton = buildEngineSkeleton(*this);
  if (!skeleton.valid()) {
    return;
  }

  engine::kinematics::KinematicLocalPose localPose =
      engine::kinematics::KinematicLocalPose::zeros(skeleton.jointCount());

  auto heightAtXZ = [&](float x, float z) {
    const WaterColumn column = sampleWaterColumn(world, x, z, cellSize, heightScale);
    return placementY(column, NomHabitat::Surface);
  };

  engine::kinematics::solveForwardKinematics(skeleton, localPose, heading, std::span(nodes),
                                             heightAtXZ);
}

}  // namespace evolab
