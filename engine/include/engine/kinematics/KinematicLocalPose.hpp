#pragma once

#include <cstddef>
#include <vector>

namespace evolab::engine::kinematics {

// Runtime pose deltas layered on bind pose (yawDelta = 0 → rest/bind configuration).
class KinematicLocalPose {
public:
  KinematicLocalPose() = default;

  explicit KinematicLocalPose(std::size_t jointCount) : yawDelta_(jointCount, 0.0f) {}

  static KinematicLocalPose zeros(std::size_t jointCount) { return KinematicLocalPose(jointCount); }

  std::size_t size() const { return yawDelta_.size(); }

  float yawDelta(std::size_t jointIndex) const { return yawDelta_.at(jointIndex); }

  float& yawDelta(std::size_t jointIndex) { return yawDelta_.at(jointIndex); }

  const std::vector<float>& deltas() const { return yawDelta_; }

private:
  std::vector<float> yawDelta_;
};

}  // namespace evolab::engine::kinematics
