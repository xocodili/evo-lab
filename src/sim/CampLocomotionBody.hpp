#pragma once

#include "sim/Organism.hpp"

namespace evolab {

// Integrated body yaw for articulated campers; diagnostic heading fallback otherwise.
float campLocomotionBodyYaw(const Organism& organism);

// Seed both body state and diagnostic heading (spawn / test harness only).
void seedCampLocomotionBodyYaw(Organism& organism, float worldYaw);

// Chemotaxis / taste steering: articulated → rootYawRate; legacy → heading slew.
void applyCampBodyYawSteering(Organism& organism, float targetWorldYaw, float turnRateScale);

// Run-and-tumble reorientation event on the integrated body.
void applyCampBodyTumble(Organism& organism, float signedTumbleTurn);

}  // namespace evolab
