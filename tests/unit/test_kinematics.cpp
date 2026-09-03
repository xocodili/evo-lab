#include "engine/kinematics/ForwardKinematics.hpp"
#include "engine/kinematics/ArticulatedDynamics.hpp"
#include "engine/kinematics/BoneDistanceConstraint.hpp"
#include "engine/kinematics/JointConstraint.hpp"
#include "engine/kinematics/KinematicLocalPose.hpp"
#include "engine/kinematics/KinematicSkeleton.hpp"
#include "engine/kinematics/Math.hpp"
#include "engine/kinematics/NodeMediumDrag.hpp"

#include "sim/BarrenWorld.hpp"
#include "sim/CampTopology.hpp"
#include "sim/OrganismKinematicBirth.hpp"
#include "sim/SimConfig.hpp"
#include "sim/WorldConstants.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cmath>
#include <span>
#include <vector>

namespace {

using evolab::engine::kinematics::KinematicBone;
using evolab::engine::kinematics::KinematicLocalPose;
using evolab::engine::kinematics::KinematicNodePose;
using evolab::engine::kinematics::KinematicSkeleton;

float edgeLength(const KinematicNodePose& a, const KinematicNodePose& b) {
  return std::hypot(b.worldX - a.worldX, b.worldZ - a.worldZ);
}

std::vector<KinematicBone> nominalTriangleBones() {
  return {
      {1, 3, 1.0f, 0.0f},
      {1, 2, 1.0f, -1.0471976f},
      {2, 3, 1.0f, 0.0f},
  };
}

std::vector<KinematicBone> colinearChainBones(int linkCount, float segmentLength) {
  std::vector<KinematicBone> bones;
  bones.reserve(static_cast<std::size_t>(linkCount));
  for (int link = 0; link < linkCount; ++link) {
    bones.push_back({static_cast<std::uint32_t>(link + 1), static_cast<std::uint32_t>(link + 2),
                     segmentLength, 0.0f});
  }
  return bones;
}

KinematicSkeleton buildColinearChain(int linkCount, float segmentLength) {
  return KinematicSkeleton::buildFromBones(colinearChainBones(linkCount, segmentLength), 1);
}

void solveChainFk(const KinematicSkeleton& skeleton, std::span<KinematicNodePose> nodes,
                  float rootWorldYaw = 0.0f) {
  const KinematicLocalPose pose = KinematicLocalPose::zeros(skeleton.jointCount());
  REQUIRE(evolab::engine::kinematics::solveForwardKinematics(
      skeleton, pose, rootWorldYaw, nodes, [](float, float) { return 0.0f; }));
}

}  // namespace

TEST_CASE("camp birth layout expands torpedo chain from mouth anchor", "[engine][kinematics][birth]") {
  evolab::BarrenWorld world(42, 128, evolab::makeTideFromConfig(evolab::SimConfig{}));
  evolab::Organism organism =
      evolab::makeCampNomOrganism(1, 0.0f, 0.0f, 1.0f, 100, 0, 1.0f, 0.25f);
  evolab::initializeArticulatedSpawnPose(organism, world, evolab::kWorldCellSize,
                                         evolab::kTerrainHeightScale, 0.25f);

  const evolab::SkeletonNode* mouth = organism.findNode(evolab::kCampMouthId);
  const evolab::SkeletonNode* actuator = organism.findNode(evolab::kCampActuatorId);
  REQUIRE(mouth != nullptr);
  REQUIRE(actuator != nullptr);
  REQUIRE(organism.kinematicsBirthApplied_);
  REQUIRE(organism.bodyDynamics.rootWorldYaw == Catch::Approx(0.25f).margin(1e-4f));
  REQUIRE(organism.heading == Catch::Approx(0.25f).margin(1e-4f));

  const float spread = std::hypot(actuator->worldX - mouth->worldX, actuator->worldZ - mouth->worldZ);
  REQUIRE(spread > 2.5f);
  REQUIRE(organism.bodyDynamics.rootVelX == Catch::Approx(0.0f));
  for (float delta : organism.bodyDynamics.jointYawDelta) {
    REQUIRE(delta == Catch::Approx(0.0f));
  }
}

TEST_CASE("kinematic skeleton caches joint depth from root", "[engine][kinematics]") {
  const KinematicSkeleton skeleton = buildColinearChain(3, 1.0f);
  REQUIRE(skeleton.jointDepths().size() == skeleton.jointCount());
  REQUIRE(skeleton.jointDepth(0) == 0);
  REQUIRE(skeleton.jointDepth(1) == 1);
  REQUIRE(skeleton.jointDepth(2) == 2);
  REQUIRE(skeleton.jointDepth(3) == 3);
}

TEST_CASE("kinematic skeleton builds tree and skips closing bone", "[engine][kinematics]") {
  const KinematicSkeleton skeleton =
      KinematicSkeleton::buildFromBones(nominalTriangleBones(), 1);
  REQUIRE(skeleton.valid());
  REQUIRE(skeleton.jointCount() == 3);
  REQUIRE(skeleton.rootNodeId() == 1);
  REQUIRE(skeleton.hasJoint(2));
  REQUIRE(skeleton.hasJoint(3));
  REQUIRE(skeleton.joint(skeleton.jointIndex(2)).parentIndex == 0);
  REQUIRE(skeleton.joint(skeleton.jointIndex(3)).bindLocalYaw == Catch::Approx(0.0f));
  REQUIRE(skeleton.joint(skeleton.jointIndex(2)).bindLocalYaw ==
          Catch::Approx(-1.0471976f).margin(1e-4f));
}

TEST_CASE("engine forward kinematics places equilateral triangle", "[engine][kinematics]") {
  std::array<KinematicNodePose, 3> nodes = {{
      {1, 0.0f, 0.0f, 0.0f},
      {2, 0.0f, 0.0f, 0.0f},
      {3, 0.0f, 0.0f, 0.0f},
  }};

  REQUIRE(evolab::engine::kinematics::solveTreeForwardKinematicsFlat(
      std::span(nodes), std::span<const KinematicBone>(nominalTriangleBones()), 1, 0.0f));

  const float side = edgeLength(nodes[0], nodes[1]);
  REQUIRE(side == Catch::Approx(1.0f).margin(0.05f));
  REQUIRE(edgeLength(nodes[0], nodes[2]) == Catch::Approx(side).margin(0.05f));
  REQUIRE(edgeLength(nodes[1], nodes[2]) == Catch::Approx(side).margin(0.05f));
}

TEST_CASE("hierarchical FK composes parent yaw along chain", "[engine][kinematics]") {
  const std::vector<KinematicBone> bones = {
      {1, 2, 1.0f, 1.5707963f},
      {2, 3, 1.0f, 0.0f},
  };
  const KinematicSkeleton skeleton = KinematicSkeleton::buildFromBones(bones, 1);
  std::array<KinematicNodePose, 3> nodes = {{
      {1, 0.0f, 0.0f, 0.0f},
      {2, 0.0f, 0.0f, 0.0f},
      {3, 0.0f, 0.0f, 0.0f},
  }};
  const KinematicLocalPose pose = KinematicLocalPose::zeros(skeleton.jointCount());

  REQUIRE(evolab::engine::kinematics::solveForwardKinematics(
      skeleton, pose, 0.0f, std::span(nodes), [](float, float) { return 0.0f; }));

  REQUIRE(nodes[1].worldX == Catch::Approx(1.0f).margin(0.05f));
  REQUIRE(nodes[1].worldZ == Catch::Approx(0.0f).margin(0.05f));
  REQUIRE(nodes[2].worldX == Catch::Approx(2.0f).margin(0.05f));
  REQUIRE(nodes[2].worldZ == Catch::Approx(0.0f).margin(0.05f));
}

TEST_CASE("local pose delta rotates joint from bind", "[engine][kinematics]") {
  const std::vector<KinematicBone> bones = {{1, 2, 1.0f, 0.0f}};
  KinematicSkeleton skeleton = KinematicSkeleton::buildFromBones(bones, 1);
  std::array<KinematicNodePose, 2> nodes = {{
      {1, 0.0f, 0.0f, 0.0f},
      {2, 0.0f, 0.0f, 0.0f},
  }};

  KinematicLocalPose pose = KinematicLocalPose::zeros(skeleton.jointCount());
  pose.yawDelta(1) = evolab::engine::kinematics::kPi / 2.0f;

  REQUIRE(evolab::engine::kinematics::solveForwardKinematics(
      skeleton, pose, 0.0f, std::span(nodes), [](float, float) { return 0.0f; }));

  REQUIRE(nodes[1].worldX == Catch::Approx(1.0f).margin(0.05f));
  REQUIRE(nodes[1].worldZ == Catch::Approx(0.0f).margin(0.05f));
}

TEST_CASE("joint constraint clamps local yaw", "[engine][kinematics]") {
  evolab::engine::kinematics::JointConstraint constraint;
  constraint.maxLocalYaw = 0.25f;
  constraint.minLocalYaw = -0.25f;
  REQUIRE(constraint.resolve(0.0f, 2.0f) == Catch::Approx(0.25f));
  REQUIRE(constraint.resolve(0.0f, -2.0f) == Catch::Approx(-0.25f));
  REQUIRE(constraint.resolve(0.0f, 0.1f) == Catch::Approx(0.1f));
}

TEST_CASE("skeleton joint constraint limits solved pose", "[engine][kinematics]") {
  const std::vector<KinematicBone> bones = {{1, 2, 1.0f, 0.0f}};
  KinematicSkeleton skeleton = KinematicSkeleton::buildFromBones(bones, 1);
  skeleton.joint(1).constraint.maxLocalYaw = 0.0f;
  skeleton.joint(1).constraint.minLocalYaw = 0.0f;

  std::array<KinematicNodePose, 2> nodes = {{
      {1, 0.0f, 0.0f, 0.0f},
      {2, 0.0f, 0.0f, 0.0f},
  }};
  KinematicLocalPose pose = KinematicLocalPose::zeros(skeleton.jointCount());
  pose.yawDelta(1) = 1.0f;

  REQUIRE(evolab::engine::kinematics::solveForwardKinematics(
      skeleton, pose, 0.0f, std::span(nodes), [](float, float) { return 0.0f; }));

  REQUIRE(nodes[1].worldX == Catch::Approx(0.0f).margin(0.05f));
  REQUIRE(nodes[1].worldZ == Catch::Approx(1.0f).margin(0.05f));
}

TEST_CASE("engine translateNodesXZ moves every node equally", "[engine][kinematics]") {
  std::array<KinematicNodePose, 2> nodes = {{
      {1, 1.0f, 0.0f, 2.0f},
      {2, 4.0f, 0.0f, -1.0f},
  }};

  evolab::engine::kinematics::translateNodesXZ(std::span(nodes), 0.5f, -1.25f);

  REQUIRE(nodes[0].worldX == Catch::Approx(1.5f));
  REQUIRE(nodes[0].worldZ == Catch::Approx(0.75f));
  REQUIRE(nodes[1].worldX == Catch::Approx(4.5f));
  REQUIRE(nodes[1].worldZ == Catch::Approx(-2.25f));
}

TEST_CASE("axial impulse at root propagates to distal node with bone constraints",
          "[engine][kinematics][dynamics]") {
  const KinematicSkeleton skeleton = buildColinearChain(3, 1.0f);
  std::array<KinematicNodePose, 4> nodes = {{
      {1, 0.0f, 0.0f, 0.0f},
      {2, 0.0f, 0.0f, 0.0f},
      {3, 0.0f, 0.0f, 0.0f},
      {4, 0.0f, 0.0f, 0.0f},
  }};
  solveChainFk(skeleton, std::span(nodes));

  const float noseZBefore = nodes[3].worldZ;

  evolab::engine::kinematics::ArticulatedBodyState state =
      evolab::engine::kinematics::ArticulatedBodyState::zeros(skeleton.jointCount());
  evolab::engine::kinematics::ExternalImpulse impulse;
  impulse.nodeId = 1;
  impulse.impulseX = 0.0f;
  impulse.impulseZ = 0.05f;

  evolab::engine::kinematics::ArticulatedStepParams params;
  params.linearDrag = 0.0f;
  params.invMass = 1.0f;
  params.solveBoneConstraints = true;
  params.boneConstraintIterations = 16;

  REQUIRE(evolab::engine::kinematics::stepArticulatedBody(
      skeleton, state, std::span(nodes),
      std::span<const evolab::engine::kinematics::MuscleCommand>{},
      std::span<const evolab::engine::kinematics::ExternalImpulse>(&impulse, 1), params,
      [](float, float) { return 0.0f; }));

  REQUIRE(nodes[0].worldZ == Catch::Approx(0.05f).margin(1e-4f));
  REQUIRE(nodes[3].worldZ == Catch::Approx(noseZBefore + 0.05f).margin(0.06f));
  REQUIRE(evolab::engine::kinematics::maxBoneLengthError(skeleton, std::span(nodes)) ==
          Catch::Approx(0.0f).margin(0.02f));
}

TEST_CASE("medium drag without impulse still restores bone lengths when constraints enabled",
          "[engine][kinematics][dynamics]") {
  const KinematicSkeleton skeleton = buildColinearChain(3, 1.0f);
  std::array<KinematicNodePose, 4> nodes = {{
      {1, 0.0f, 0.0f, 0.0f},
      {2, 0.0f, 0.0f, 0.0f},
      {3, 0.0f, 0.0f, 0.0f},
      {4, 0.0f, 0.0f, 0.0f},
  }};
  solveChainFk(skeleton, std::span(nodes));

  evolab::engine::kinematics::ArticulatedBodyState state =
      evolab::engine::kinematics::ArticulatedBodyState::zeros(skeleton.jointCount());

  evolab::engine::kinematics::ArticulatedStepParams params;
  params.mediumVelX = 0.18f;
  params.mediumVelZ = 0.0f;
  params.nodeLinearDrag = 0.45f;
  params.linearDrag = 0.0f;
  params.yawDamping = 0.0f;
  params.solveBoneConstraints = true;
  params.boneConstraintIterations = 16;

  REQUIRE(evolab::engine::kinematics::stepArticulatedBody(
      skeleton, state, std::span(nodes),
      std::span<const evolab::engine::kinematics::MuscleCommand>{},
      std::span<const evolab::engine::kinematics::ExternalImpulse>{}, params,
      [](float, float) { return 0.0f; }));

  REQUIRE(evolab::engine::kinematics::maxBoneLengthError(skeleton, std::span(nodes)) ==
          Catch::Approx(0.0f).margin(0.03f));
}

TEST_CASE("world pose sync keeps camp chain stable across consecutive drag ticks",
          "[engine][kinematics][dynamics]") {
  const KinematicSkeleton skeleton = buildColinearChain(3, 1.0f);
  std::array<KinematicNodePose, 4> nodes = {{
      {1, 0.0f, 0.0f, 0.0f},
      {2, 0.0f, 0.0f, 0.0f},
      {3, 0.0f, 0.0f, 0.0f},
      {4, 0.0f, 0.0f, 0.0f},
  }};
  solveChainFk(skeleton, std::span(nodes));

  evolab::engine::kinematics::ArticulatedBodyState state =
      evolab::engine::kinematics::ArticulatedBodyState::zeros(skeleton.jointCount());

  evolab::engine::kinematics::ArticulatedStepParams params;
  params.mediumVelX = 0.12f;
  params.mediumVelZ = -0.04f;
  params.nodeLinearDrag = 0.35f;
  params.linearDrag = 0.0f;
  params.yawDamping = 0.0f;
  params.solveBoneConstraints = true;
  params.boneConstraintIterations = 16;

  const float noseZBefore = nodes[3].worldZ;
  float prevNoseZ = noseZBefore;
  float maxTickJump = 0.0f;
  for (int tick = 0; tick < 8; ++tick) {
    REQUIRE(evolab::engine::kinematics::stepArticulatedBody(
        skeleton, state, std::span(nodes),
        std::span<const evolab::engine::kinematics::MuscleCommand>{},
        std::span<const evolab::engine::kinematics::ExternalImpulse>{}, params,
        [](float, float) { return 0.0f; }));
    maxTickJump = std::max(maxTickJump, std::abs(nodes[3].worldZ - prevNoseZ));
    prevNoseZ = nodes[3].worldZ;
  }

  REQUIRE(maxTickJump < 0.35f);
  REQUIRE(evolab::engine::kinematics::maxBoneLengthError(skeleton, std::span(nodes)) ==
          Catch::Approx(0.0f).margin(0.04f));
}

TEST_CASE("muscle PD bends joint toward target yaw delta", "[engine][kinematics][dynamics]") {
  const std::vector<KinematicBone> bones = {{1, 2, 1.0f, 0.0f}};
  KinematicSkeleton skeleton = KinematicSkeleton::buildFromBones(bones, 1);
  std::array<KinematicNodePose, 2> nodes = {{
      {1, 0.0f, 0.0f, 0.0f},
      {2, 0.0f, 0.0f, 0.0f},
  }};

  evolab::engine::kinematics::ArticulatedBodyState state =
      evolab::engine::kinematics::ArticulatedBodyState::zeros(skeleton.jointCount());
  evolab::engine::kinematics::MuscleCommand muscle;
  muscle.jointIndex = 1;
  muscle.targetYawDelta = 0.35f;
  muscle.stiffness = 0.45f;
  muscle.damping = 0.1f;

  for (int tick = 0; tick < 40; ++tick) {
    REQUIRE(evolab::engine::kinematics::stepArticulatedBody(
        skeleton, state, std::span(nodes),
        std::span<const evolab::engine::kinematics::MuscleCommand>(&muscle, 1),
        std::span<const evolab::engine::kinematics::ExternalImpulse>{},
        evolab::engine::kinematics::ArticulatedStepParams{}, [](float, float) { return 0.0f; }));
  }

  REQUIRE(state.jointYawDelta[1] == Catch::Approx(0.35f).margin(0.08f));
  REQUIRE(nodes[1].worldX == Catch::Approx(std::sin(0.35f)).margin(0.12f));
}

TEST_CASE("rootWorldYaw lives in body state and integrates from rootYawRate",
          "[engine][kinematics][dynamics]") {
  const KinematicSkeleton skeleton = buildColinearChain(2, 1.0f);
  std::array<KinematicNodePose, 3> nodes = {{
      {1, 0.0f, 0.0f, 0.0f},
      {2, 0.0f, 0.0f, 0.0f},
      {3, 0.0f, 0.0f, 0.0f},
  }};
  solveChainFk(skeleton, std::span(nodes));

  evolab::engine::kinematics::ArticulatedBodyState state =
      evolab::engine::kinematics::ArticulatedBodyState::zeros(skeleton.jointCount());
  state.rootYawRate = 0.05f;

  evolab::engine::kinematics::ArticulatedStepParams params;
  params.linearDrag = 0.0f;
  params.yawDamping = 0.0f;

  REQUIRE(evolab::engine::kinematics::stepArticulatedBody(
      skeleton, state, std::span(nodes),
      std::span<const evolab::engine::kinematics::MuscleCommand>{},
      std::span<const evolab::engine::kinematics::ExternalImpulse>{}, params,
      [](float, float) { return 0.0f; }));

  REQUIRE(state.rootWorldYaw == Catch::Approx(0.05f).margin(1e-4f));

  REQUIRE(evolab::engine::kinematics::solveForwardKinematicsFromBodyState(
      skeleton, state, std::span(nodes), [](float, float) { return 0.0f; }));
  REQUIRE(nodes[2].worldZ == Catch::Approx(2.0f).margin(0.08f));
}

TEST_CASE("engine forward kinematics uses height callback per node", "[engine][kinematics]") {
  std::array<KinematicNodePose, 2> nodes = {{
      {1, 0.0f, 0.0f, 0.0f},
      {2, 0.0f, 0.0f, 0.0f},
  }};
  const std::vector<KinematicBone> bones = {{1, 2, 2.0f, 0.0f}};

  const bool ok = evolab::engine::kinematics::solveTreeForwardKinematics(
      std::span(nodes), std::span<const KinematicBone>(bones), 1, 0.0f,
      [](float x, float z) { return (x + z) * 10.0f; });
  REQUIRE(ok);
  REQUIRE(nodes[0].worldY == Catch::Approx(0.0f));
  REQUIRE(nodes[1].worldZ == Catch::Approx(2.0f));
  REQUIRE(nodes[1].worldY == Catch::Approx(20.0f));
}

TEST_CASE("bone distance constraints restore link lengths after root push on straight chain",
          "[engine][kinematics][bone-constraint]") {
  constexpr int kLinkCount = 3;
  constexpr float kSegment = 1.0f;
  const KinematicSkeleton skeleton = buildColinearChain(kLinkCount, kSegment);

  std::array<KinematicNodePose, 4> nodes = {{
      {1, 0.0f, 0.0f, 0.0f},
      {2, 0.0f, 0.0f, 0.0f},
      {3, 0.0f, 0.0f, 0.0f},
      {4, 0.0f, 0.0f, 0.0f},
  }};
  solveChainFk(skeleton, std::span(nodes));

  const float noseZBefore = nodes[3].worldZ;
  constexpr float kPush = 0.45f;
  nodes[0].worldZ += kPush;

  evolab::engine::kinematics::BoneDistanceConstraintParams params;
  params.pinnedNodeId = 1;
  params.hasPinnedNode = true;
  params.iterationCount = 16;
  REQUIRE(evolab::engine::kinematics::solveBoneDistanceConstraints(skeleton, std::span(nodes),
                                                                   params));

  REQUIRE(evolab::engine::kinematics::maxBoneLengthError(skeleton, std::span(nodes)) ==
          Catch::Approx(0.0f).margin(0.02f));
  REQUIRE(nodes[3].worldZ == Catch::Approx(noseZBefore + kPush).margin(0.06f));
  REQUIRE(nodes[0].worldZ == Catch::Approx(kPush).margin(1e-4f));
}

TEST_CASE("bone distance constraints propagate tail push to nose on bent four-link chain",
          "[engine][kinematics][bone-constraint]") {
  const std::vector<KinematicBone> bones = {
      {1, 2, 1.0f, 0.0f},
      {2, 3, 1.0f, 0.0f},
      {3, 4, 1.0f, 0.0f},
  };
  KinematicSkeleton skeleton = KinematicSkeleton::buildFromBones(bones, 1);
  skeleton.joint(skeleton.jointIndex(2)).constraint.maxLocalYaw = 0.55f;
  skeleton.joint(skeleton.jointIndex(2)).constraint.minLocalYaw = -0.55f;
  skeleton.joint(skeleton.jointIndex(3)).constraint.maxLocalYaw = 0.55f;
  skeleton.joint(skeleton.jointIndex(3)).constraint.minLocalYaw = -0.55f;

  std::array<KinematicNodePose, 4> nodes = {{
      {1, 0.0f, 0.0f, 0.0f},
      {2, 0.0f, 0.0f, 0.0f},
      {3, 0.0f, 0.0f, 0.0f},
      {4, 0.0f, 0.0f, 0.0f},
  }};

  KinematicLocalPose pose = KinematicLocalPose::zeros(skeleton.jointCount());
  pose.yawDelta(2) = 0.35f;
  pose.yawDelta(3) = -0.25f;
  REQUIRE(evolab::engine::kinematics::solveForwardKinematics(
      skeleton, pose, 0.0f, std::span(nodes), [](float, float) { return 0.0f; }));

  const float noseZBefore = nodes[3].worldZ;
  const float noseXBefore = nodes[3].worldX;
  constexpr float kPush = 0.35f;
  nodes[0].worldZ += kPush;

  evolab::engine::kinematics::BoneDistanceConstraintParams params;
  params.pinnedNodeId = 1;
  params.hasPinnedNode = true;
  params.iterationCount = 24;
  REQUIRE(evolab::engine::kinematics::solveBoneDistanceConstraints(skeleton, std::span(nodes),
                                                                   params));

  REQUIRE(evolab::engine::kinematics::maxBoneLengthError(skeleton, std::span(nodes)) ==
          Catch::Approx(0.0f).margin(0.03f));
  const float noseDisp =
      std::hypot(nodes[3].worldX - noseXBefore, nodes[3].worldZ - noseZBefore);
  REQUIRE(noseDisp > kPush * 0.3f);
  REQUIRE(nodes[3].worldZ > noseZBefore);
}

TEST_CASE("per-node medium drag displaces nose more than mid chain on cross flow",
          "[engine][kinematics][medium-drag]") {
  constexpr int kLinkCount = 3;
  constexpr float kSegment = 1.0f;
  const KinematicSkeleton skeleton = buildColinearChain(kLinkCount, kSegment);

  std::array<KinematicNodePose, 4> nodes = {{
      {1, 0.0f, 0.0f, 0.0f},
      {2, 0.0f, 0.0f, 0.0f},
      {3, 0.0f, 0.0f, 0.0f},
      {4, 0.0f, 0.0f, 0.0f},
  }};
  solveChainFk(skeleton, std::span(nodes));

  const float midXBefore = nodes[1].worldX;
  const float noseXBefore = nodes[3].worldX;

  evolab::engine::kinematics::applyPerNodeMediumDrag(skeleton, std::span(nodes), 0.25f, 0.0f, 0.5f,
                                                     0.35f);

  evolab::engine::kinematics::BoneDistanceConstraintParams params;
  params.pinnedNodeId = 1;
  params.hasPinnedNode = true;
  params.iterationCount = 16;
  REQUIRE(evolab::engine::kinematics::solveBoneDistanceConstraints(skeleton, std::span(nodes),
                                                                   params));

  const float midDisp = std::abs(nodes[1].worldX - midXBefore);
  const float noseDisp = std::abs(nodes[3].worldX - noseXBefore);
  REQUIRE(noseDisp > midDisp);
  REQUIRE(nodes[0].worldX == Catch::Approx(0.0f).margin(1e-4f));
}

TEST_CASE("bone distance constraints work on minimal two-link chain",
          "[engine][kinematics][bone-constraint]") {
  const KinematicSkeleton skeleton = buildColinearChain(1, 1.25f);
  std::array<KinematicNodePose, 2> nodes = {{
      {1, 0.0f, 0.0f, 0.0f},
      {2, 0.0f, 0.0f, 0.0f},
  }};
  solveChainFk(skeleton, std::span(nodes));

  nodes[0].worldZ += 0.2f;
  evolab::engine::kinematics::BoneDistanceConstraintParams params;
  params.pinnedNodeId = 1;
  params.hasPinnedNode = true;
  REQUIRE(evolab::engine::kinematics::solveBoneDistanceConstraints(skeleton, std::span(nodes),
                                                                   params));

  REQUIRE(evolab::engine::kinematics::maxBoneLengthError(skeleton, std::span(nodes)) ==
          Catch::Approx(0.0f).margin(0.02f));
  REQUIRE(edgeLength(nodes[0], nodes[1]) == Catch::Approx(1.25f).margin(0.02f));
  REQUIRE(nodes[1].worldZ == Catch::Approx(0.2f + 1.25f).margin(0.05f));
}
