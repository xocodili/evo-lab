#include "sim/NeuronMusculature.hpp"

#include "sim/CampTopology.hpp"
#include "sim/CellConstants.hpp"
#include "sim/NeuralAxon.hpp"
#include "sim/NeuronSignal.hpp"

#include <algorithm>
#include <cmath>

namespace evolab {

namespace {

float outboundNeuronDrive(const Organism& organism, const SkeletonNode& node) {
  if (!node.alive) {
    return 0.0f;
  }
  return confidenceToUnit(encodeNeuronOutboundConfidence(organism, node.neuron, node));
}

float inboundAxonDrive(const Organism& organism, const NeuralAxon& axon, std::uint32_t listenerId) {
  if (axon.dstNodeId != listenerId || !axon.lastReceived.valid ||
      !isNeuronConfidenceByte(axon.lastReceived.byte) || !axonSignalGateOpen(axon)) {
    return 0.0f;
  }
  const float unit = confidenceToUnit(axon.lastReceived.byte);
  return unit * axonTrustScale(axonBelieveTrustForByte(axon, axon.lastReceived.byte));
}

// Bundle tension: outbound drives on parent vs child plus believe-traffic asymmetry on both axons.
float axonBundleTension(const Organism& organism, std::uint32_t parentId, std::uint32_t childId) {
  const SkeletonNode* parent = organism.findNode(parentId);
  const SkeletonNode* child = organism.findNode(childId);
  if (parent == nullptr || child == nullptr || !parent->alive || !child->alive) {
    return 0.0f;
  }

  float tension = outboundNeuronDrive(organism, *child) - outboundNeuronDrive(organism, *parent);

  if (const NeuralAxon* parentToChild = organism.findNeuralAxon(parentId, childId)) {
    tension += inboundAxonDrive(organism, *parentToChild, childId) * 0.35f;
  }
  if (const NeuralAxon* childToParent = organism.findNeuralAxon(childId, parentId)) {
    tension -= inboundAxonDrive(organism, *childToParent, parentId) * 0.35f;
  }

  if (child->neuron == NeuronType::Actuator) {
    tension += organism.lastStrokePaid ? 0.55f : -0.08f;
    tension += organism.lastActuatorNetDrive * 0.25f;
  } else if (child->neuron == NeuronType::Mouth) {
    tension += (mouthFuelConfidence(*child) / static_cast<float>(kNeuronConfidenceMax) - 0.5f) * 0.45f;
  } else if (child->neuron == NeuronType::Perceptor) {
    tension += (static_cast<float>(child->perceptConfidence) /
                    static_cast<float>(kNeuronConfidenceMax) -
                0.5f) *
               0.35f;
  }

  return std::clamp(tension, -1.0f, 1.0f);
}

const SkeletonLink* findLinkToChild(const Organism& organism, std::uint32_t childId) {
  for (const SkeletonLink& link : organism.links) {
    if (link.childNodeId == childId) {
      return &link;
    }
  }
  return nullptr;
}

}  // namespace

void applyCampJointFlexLimits(engine::kinematics::KinematicSkeleton& skeleton) {
  for (std::size_t jointIndex = 0; jointIndex < skeleton.jointCount(); ++jointIndex) {
    engine::kinematics::KinematicSkeleton::Joint& joint = skeleton.joint(jointIndex);
    if (joint.parentIndex < 0) {
      continue;
    }
    joint.constraint.minLocalYaw = joint.bindLocalYaw - kAxonBundleMaxFlexRad;
    joint.constraint.maxLocalYaw = joint.bindLocalYaw + kAxonBundleMaxFlexRad;
    joint.constraint.stiffness = kAxonBundleFlexStiffness;
  }
}

engine::kinematics::KinematicLocalPose buildCampMusclePose(
    const Organism& organism, const engine::kinematics::KinematicSkeleton& skeleton) {
  engine::kinematics::KinematicLocalPose pose =
      engine::kinematics::KinematicLocalPose::zeros(skeleton.jointCount());

  if (!organism.isCampNom()) {
    return pose;
  }

  for (std::size_t jointIndex = 0; jointIndex < skeleton.jointCount(); ++jointIndex) {
    const engine::kinematics::KinematicSkeleton::Joint& joint = skeleton.joint(jointIndex);
    if (joint.parentIndex < 0) {
      continue;
    }

    const engine::kinematics::KinematicSkeleton::Joint& parentJoint =
        skeleton.joint(static_cast<std::size_t>(joint.parentIndex));
    const SkeletonLink* link = findLinkToChild(organism, joint.nodeId);
    if (link == nullptr || !link->muscleBundle) {
      continue;
    }

    const float tension = axonBundleTension(organism, parentJoint.nodeId, joint.nodeId);
    pose.yawDelta(jointIndex) = tension * kAxonBundleFlexGain;
  }

  return pose;
}

}  // namespace evolab
