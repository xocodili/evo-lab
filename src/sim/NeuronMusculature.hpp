#pragma once

#include "engine/kinematics/ArticulatedDynamics.hpp"
#include "engine/kinematics/KinematicSkeleton.hpp"
#include "sim/Organism.hpp"

#include <vector>

namespace evolab {

std::vector<engine::kinematics::MuscleCommand> buildMuscleCommands(
    const Organism& organism, const engine::kinematics::KinematicSkeleton& skeleton);

void applyCampJointFlexLimits(engine::kinematics::KinematicSkeleton& skeleton);

float campAxonBundleTension(const Organism& organism, std::uint32_t parentId,
                            std::uint32_t childId);

void queueCampStrokeImpulse(Organism& organism, std::uint32_t effectorNodeId, float mechanicalThrust,
                            float thrustHeading);

}  // namespace evolab
