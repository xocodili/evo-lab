#pragma once

#include "engine/kinematics/ForwardKinematicsScratch.hpp"

#include <cstddef>
#include <vector>

namespace evolab::engine::kinematics {

// Floating-base articulated body state (planar XZ + root yaw).
// Root translation lives on the root node in the caller's node buffer; rootWorldYaw and
// rootVel* / rootYawRate are integrated here by stepArticulatedBody.
struct ArticulatedBodyState {
  float rootWorldYaw = 0.0f;
  float rootVelX = 0.0f;
  float rootVelZ = 0.0f;
  float rootYawRate = 0.0f;

  std::vector<float> jointYawDelta;
  std::vector<float> jointYawVel;
  mutable ForwardKinematicsScratch fkScratch;

  static ArticulatedBodyState zeros(std::size_t jointCount, float rootWorldYaw = 0.0f) {
    ArticulatedBodyState state;
    state.rootWorldYaw = rootWorldYaw;
    state.ensureJointCount(jointCount);
    return state;
  }

  void ensureJointCount(std::size_t jointCount) {
    if (jointYawDelta.size() != jointCount) {
      jointYawDelta.assign(jointCount, 0.0f);
      jointYawVel.assign(jointCount, 0.0f);
    }
    fkScratch.ensureJointCount(jointCount);
  }
};

}  // namespace evolab::engine::kinematics
