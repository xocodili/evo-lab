#include "sim/CampLocomotionBody.hpp"

#include "engine/kinematics/Math.hpp"

#include "sim/CampTopology.hpp"
#include "sim/CellConstants.hpp"
#include "sim/OrganismNeuron.hpp"

#include <algorithm>
#include <cmath>

namespace evolab {

namespace {

using engine::kinematics::normalizeAngle;

}  // namespace

float campLocomotionBodyYaw(const Organism& organism) {
  if (organism.usesArticulatedLocomotion() || organism.kinematicsBirthApplied_) {
    return organism.bodyDynamics.rootWorldYaw;
  }
  return organism.heading;
}

void seedCampLocomotionBodyYaw(Organism& organism, float worldYaw) {
  organism.bodyDynamics.rootWorldYaw = worldYaw;
  organism.heading = worldYaw;
}

void applyCampBodyYawSteering(Organism& organism, float targetWorldYaw, float turnRateScale) {
  if (!organism.usesArticulatedLocomotion()) {
    const float bearingError = std::abs(normalizeAngle(targetWorldYaw - organism.heading));
    const float adaptScale =
        clamp01(bearingError / kOrganismCampChemotaxisAdaptRad);
    organism.heading =
        turnToward(organism.heading, targetWorldYaw,
                   kOrganismMaxTurnPerTick * turnRateScale * adaptScale);
    return;
  }

  const float current = organism.bodyDynamics.rootWorldYaw;
  const float signedError = normalizeAngle(targetWorldYaw - current);
  const float adaptScale =
      clamp01(std::abs(signedError) / kOrganismCampChemotaxisAdaptRad);
  const float maxRate = kOrganismMaxTurnPerTick * turnRateScale * adaptScale;
  const float targetRate = std::clamp(signedError, -maxRate, maxRate);
  float& rate = organism.bodyDynamics.rootYawRate;
  rate += (targetRate - rate) * kCampYawSteeringBlend;
  rate = std::clamp(rate, -maxRate, maxRate);
}

void applyCampBodyTumble(Organism& organism, float signedTumbleTurn) {
  if (!organism.usesArticulatedLocomotion()) {
    organism.heading = normalizeAngle(organism.heading + signedTumbleTurn);
    return;
  }
  organism.bodyDynamics.rootYawRate =
      normalizeAngle(organism.bodyDynamics.rootYawRate + signedTumbleTurn);
}

}  // namespace evolab
