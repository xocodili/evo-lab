#include "engine/kinematics/ForwardKinematics.hpp"
#include "engine/kinematics/JointConstraint.hpp"
#include "engine/kinematics/KinematicLocalPose.hpp"
#include "engine/kinematics/KinematicSkeleton.hpp"
#include "engine/kinematics/Math.hpp"

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

std::vector<KinematicBone> pmaNomTriangleBones() {
  return {
      {1, 3, 1.0f, 0.0f},
      {1, 2, 1.0f, -1.0471976f},
      {2, 3, 1.0f, 0.0f},
  };
}

}  // namespace

TEST_CASE("kinematic skeleton builds tree and skips closing bone", "[engine][kinematics]") {
  const KinematicSkeleton skeleton =
      KinematicSkeleton::buildFromBones(pmaNomTriangleBones(), 1);
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
      std::span(nodes), std::span<const KinematicBone>(pmaNomTriangleBones()), 1, 0.0f));

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
