#pragma once

#include "engine/kinematics/KinematicLocalPose.hpp"
#include "engine/kinematics/KinematicSkeleton.hpp"
#include "sim/Organism.hpp"

namespace evolab {

// Build runtime joint flex from bidirectional neural axon bundles on each skeleton link.
engine::kinematics::KinematicLocalPose buildCampMusclePose(const Organism& organism,
                                                           const engine::kinematics::KinematicSkeleton& skeleton);

void applyCampJointFlexLimits(engine::kinematics::KinematicSkeleton& skeleton);

float campAxonBundleTension(const Organism& organism, std::uint32_t parentId,
                            std::uint32_t childId);

float campKeelYawTorque(const Organism& organism);

void applyCampBundleStroke(Organism& organism, SkeletonNode& motor, SkeletonNode& hub,
                           float mechanicalThrust);

}  // namespace evolab
