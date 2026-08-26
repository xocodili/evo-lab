#pragma once

#include <cmath>

namespace evolab::engine::kinematics {

inline constexpr float kPi = 3.14159265358979323846f;
inline constexpr float kTwoPi = kPi * 2.0f;

inline float normalizeAngle(float radians) {
  float angle = std::fmod(radians + kPi, kTwoPi);
  if (angle < 0.0f) {
    angle += kTwoPi;
  }
  return angle - kPi;
}

}  // namespace evolab::engine::kinematics
