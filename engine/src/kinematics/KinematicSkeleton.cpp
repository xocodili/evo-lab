#include "engine/kinematics/KinematicSkeleton.hpp"

namespace evolab::engine::kinematics {

KinematicSkeleton KinematicSkeleton::buildFromBones(std::span<const KinematicBone> bones,
                                                    std::uint32_t rootNodeId) {
  KinematicSkeleton skeleton;
  skeleton.rootNodeId_ = rootNodeId;

  skeleton.joints_.push_back(Joint{rootNodeId, -1, 0.0f, 0.0f, {}});
  skeleton.idToIndex_[rootNodeId] = 0;

  bool progress = true;
  while (progress) {
    progress = false;
    for (const KinematicBone& bone : bones) {
      const auto parentIt = skeleton.idToIndex_.find(bone.parentNodeId);
      if (parentIt == skeleton.idToIndex_.end()) {
        continue;
      }
      if (skeleton.idToIndex_.find(bone.childNodeId) != skeleton.idToIndex_.end()) {
        continue;
      }

      Joint joint;
      joint.nodeId = bone.childNodeId;
      joint.parentIndex = static_cast<int>(parentIt->second);
      joint.bindLocalYaw = bone.jointAngle;
      joint.restLength = bone.restLength;
      skeleton.idToIndex_[joint.nodeId] = skeleton.joints_.size();
      skeleton.joints_.push_back(joint);
      progress = true;
    }
  }

  if (skeleton.joints_.size() == 1 && !bones.empty()) {
    // Root was listed only as a leaf in malformed input — still valid single-node skeleton.
  }

  return skeleton;
}

std::size_t KinematicSkeleton::jointIndex(std::uint32_t nodeId) const {
  const auto it = idToIndex_.find(nodeId);
  if (it == idToIndex_.end()) {
    return joints_.size();
  }
  return it->second;
}

}  // namespace evolab::engine::kinematics
