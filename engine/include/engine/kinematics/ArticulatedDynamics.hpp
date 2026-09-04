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
  float mediumVelX = 0.0f;
  float mediumVelZ = 0.0f;
  float linearDrag = 0.12f;
  // Non-root nodes: ambient medium coupling. 0 = rigid FK followers only.
  float nodeLinearDrag = 0.0f;
  float nodeDragDepthGain = 0.35f;
  float yawDamping = 0.15f;
  float invMass = 1.0f;
  float invInertia = 0.35f;
  bool solveBoneConstraints = true;
  int boneConstraintIterations = 16;
  float boneConstraintStiffness = 1.0f;
  // When false, bone solve can translate the whole chain (tail stroke propulsion).
  bool pinKinematicRootDuringBones = true;
};

// FK from integrated body state (joint deltas + rootWorldYaw).
template <typename NodeLike, std::size_t Extent, typename HeightAtXZ>
bool solveForwardKinematicsFromBodyState(const KinematicSkeleton& skeleton,
                                         ArticulatedBodyState& state,
                                         std::span<NodeLike, Extent> nodes,
                                         HeightAtXZ&& heightAtXZ);

// Unit spine direction at a node (parent→child or child→parent edge). For local impulse axes.
template <typename NodeLike, std::size_t Extent>
bool resolveSpineAxisAtNode(const KinematicSkeleton& skeleton, std::span<NodeLike, Extent> nodes,
                            std::uint32_t nodeId, float& axisX, float& axisZ);

// One fixed-timestep articulated-body integration: muscle PD → FK → root advection →
// medium drag → external impulses → optional bone constraints.
template <typename NodeLike, std::size_t Extent, typename HeightAtXZ>
bool stepArticulatedBody(const KinematicSkeleton& skeleton, ArticulatedBodyState& state,
                         std::span<NodeLike, Extent> nodes, std::span<const MuscleCommand> muscles,
                         std::span<const ExternalImpulse> impulses,
                         const ArticulatedStepParams& params, HeightAtXZ&& heightAtXZ);

}  // namespace evolab::engine::kinematics

#include "engine/kinematics/ArticulatedDynamics.inl"
