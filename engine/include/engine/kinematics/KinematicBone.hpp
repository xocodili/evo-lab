#pragma once

#include "engine/kinematics/KinematicBone.hpp"

#include <cstdint>

namespace evolab::engine::kinematics {

// Directed bone in a tree-structured skeleton (parent → child).
struct KinematicBone {
  std::uint32_t parentNodeId = 0;
  std::uint32_t childNodeId = 0;
  float restLength = 0.0f;
  // Bind local yaw (radians) relative to parent facing; root joint is relative to rootWorldYaw.
  float jointAngle = 0.0f;
};

}  // namespace evolab::engine::kinematics
