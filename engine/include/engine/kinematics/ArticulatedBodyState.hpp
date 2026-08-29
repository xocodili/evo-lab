#pragma once

#include <cstddef>
#include <vector>

namespace evolab::engine::kinematics {

// Runtime dynamics state for an articulated tree (XZ plane + root yaw).
// Joint yaw deltas layer on bind pose during FK; integrated by muscle PD each tick.
struct ArticulatedBodyState {
  float rootVelX = 0.0f;
  float rootVelZ = 0.0f;
  float rootYawRate = 0.0f;

  std::vector<float> jointYawDelta;
  std::vector<float> jointYawVel;

  static ArticulatedBodyState zeros(std::size_t jointCount) {
    ArticulatedBodyState state;
    state.jointYawDelta.assign(jointCount, 0.0f);
    state.jointYawVel.assign(jointCount, 0.0f);
    return state;
  }

  void ensureJointCount(std::size_t jointCount) {
    if (jointYawDelta.size() != jointCount) {
      jointYawDelta.assign(jointCount, 0.0f);
      jointYawVel.assign(jointCount, 0.0f);
    }
  }
};

}  // namespace evolab::engine::kinematics
