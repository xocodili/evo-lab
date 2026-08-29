#pragma once

#include "engine/kinematics/ArticulatedBodyState.hpp"
#include "engine/kinematics/KinematicSkeleton.hpp"

#include <cstdint>
#include <span>

namespace evolab::engine::kinematics {

struct MuscleCommand {
  std::size_t jointIndex = 0;
  float targetYawDelta = 0.0f;
  float stiffness = 1.0f;
  float damping = 0.25f;
};

struct ExternalImpulse {
  std::uint32_t nodeId = 0;
  float impulseX = 0.0f;
  float impulseZ = 0.0f;
};

struct ArticulatedStepParams {
  float tideVelX = 0.0f;
  float tideVelZ = 0.0f;
  float linearDrag = 0.12f;
  float yawDamping = 0.15f;
  float invMass = 1.0f;
  float invInertia = 0.35f;
};

template <typename NodeLike, std::size_t Extent, typename HeightAtXZ>
bool stepArticulatedBody(const KinematicSkeleton& skeleton, ArticulatedBodyState& state,
                         float& rootWorldYaw, std::span<NodeLike, Extent> nodes,
                         std::span<const MuscleCommand> muscles,
                         std::span<const ExternalImpulse> impulses,
                         const ArticulatedStepParams& params, HeightAtXZ&& heightAtXZ);

}  // namespace evolab::engine::kinematics

#include "engine/kinematics/ArticulatedDynamics.inl"
