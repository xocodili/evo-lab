#pragma once

#include "engine/kinematics/JointConstraint.hpp"
#include "engine/kinematics/KinematicBone.hpp"

#include <cstdint>
#include <span>
#include <unordered_map>
#include <vector>

namespace evolab::engine::kinematics {

// Tree skeleton built from directed bones. Each child appears once; extra bones (e.g.
// closing render edges) are ignored when the child already has a parent in the tree.
class KinematicSkeleton {
public:
  struct Joint {
    std::uint32_t nodeId = 0;
    int parentIndex = -1;
    float bindLocalYaw = 0.0f;
    float restLength = 0.0f;
    JointConstraint constraint;
  };

  KinematicSkeleton() = default;

  static KinematicSkeleton buildFromBones(std::span<const KinematicBone> bones,
                                          std::uint32_t rootNodeId);

  bool valid() const { return !joints_.empty(); }

  std::size_t jointCount() const { return joints_.size(); }

  std::uint32_t rootNodeId() const { return rootNodeId_; }

  const Joint& joint(std::size_t jointIndex) const { return joints_.at(jointIndex); }

  Joint& joint(std::size_t jointIndex) { return joints_.at(jointIndex); }

  bool hasJoint(std::uint32_t nodeId) const {
    return idToIndex_.find(nodeId) != idToIndex_.end();
  }

  std::size_t jointIndex(std::uint32_t nodeId) const;

  // Tree depth from root joint (0 at root). Cached at build time for medium drag.
  int jointDepth(std::size_t jointIndex) const { return jointDepthFromRoot_.at(jointIndex); }

  const std::vector<int>& jointDepths() const { return jointDepthFromRoot_; }

private:
  void finalizeTopology();

  std::uint32_t rootNodeId_ = 0;
  std::vector<Joint> joints_;
  std::vector<int> jointDepthFromRoot_;
  std::unordered_map<std::uint32_t, std::size_t> idToIndex_;
};

}  // namespace evolab::engine::kinematics
