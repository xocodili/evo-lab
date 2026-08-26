#pragma once

#include "engine/kinematics/Math.hpp"

#include <algorithm>

namespace evolab::engine::kinematics {

// Per-joint local yaw limits (radians, relative to parent bone direction).
struct JointConstraint {
  float minLocalYaw = -kPi;
  float maxLocalYaw = kPi;
  // 1 = hard clamp to [min, max]; lower values pull toward bind pose (reserved for soft IK).
  float stiffness = 1.0f;

  float clampLocalYaw(float localYaw) const {
    return std::clamp(localYaw, minLocalYaw, maxLocalYaw);
  }

  float resolve(float bindLocalYaw, float yawDelta) const {
    const float target = bindLocalYaw + yawDelta;
    const float clamped = clampLocalYaw(target);
    if (stiffness >= 1.0f) {
      return clamped;
    }
    if (stiffness <= 0.0f) {
      return bindLocalYaw;
    }
    return bindLocalYaw + (clamped - bindLocalYaw) * stiffness;
  }
};

}  // namespace evolab::engine::kinematics
