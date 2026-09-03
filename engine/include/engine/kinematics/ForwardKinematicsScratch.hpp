#pragma once

#include <cstddef>
#include <vector>

namespace evolab::engine::kinematics {

// Reusable FK workspace — avoids per-solve heap churn on hot articulated-body ticks.
struct ForwardKinematicsScratch {
  std::vector<float> worldYaw;
  std::vector<float> worldX;
  std::vector<float> worldZ;

  void ensureJointCount(std::size_t jointCount) {
    if (worldYaw.size() != jointCount) {
      worldYaw.assign(jointCount, 0.0f);
      worldX.assign(jointCount, 0.0f);
      worldZ.assign(jointCount, 0.0f);
    }
  }
};

}  // namespace evolab::engine::kinematics
