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

const SkeletonLink* findLinkToChild(const Organism& organism, std::uint32_t childId) {
  for (const SkeletonLink& link : organism.links) {
    if (link.childNodeId == childId) {
      return &link;
    }
  }
  return nullptr;
}

}  // namespace

float campAxonBundleTension(const Organism& organism, std::uint32_t parentId,
                            std::uint32_t childId) {
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
    tension += organism.lastActuatorNetDrive * 0.35f;
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

namespace {

float muscleTargetYawDelta(const Organism& organism, std::uint32_t parentId,
                           std::uint32_t childId) {
  return campAxonBundleTension(organism, parentId, childId) * kAxonBundleFlexGain;
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

std::vector<engine::kinematics::MuscleCommand> buildMuscleCommands(
    const Organism& organism, const engine::kinematics::KinematicSkeleton& skeleton) {
  std::vector<engine::kinematics::MuscleCommand> commands;
  if (!organism.usesArticulatedLocomotion()) {
    return commands;
  }

  commands.reserve(skeleton.jointCount());
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

    engine::kinematics::MuscleCommand command;
    command.jointIndex = jointIndex;
    command.targetYawDelta = muscleTargetYawDelta(organism, parentJoint.nodeId, joint.nodeId);
    command.stiffness = kAxonBundleFlexStiffness;
    command.damping = kMusclePdDamping;
    commands.push_back(command);
  }

  return commands;
}

void queueCampStrokeImpulse(Organism& organism, std::uint32_t effectorNodeId, float mechanicalThrust,
                            float intentHeading, float carveScale) {
  const SkeletonNode* effector = organism.findNode(effectorNodeId);
  const SkeletonNode* mouth = findPrimaryMouthNode(organism);

  float spineX = std::sin(intentHeading);
  float spineZ = std::cos(intentHeading);
  if (mouth != nullptr && effector != nullptr && mouth->id != effector->id) {
    spineX = mouth->worldX - effector->worldX;
    spineZ = mouth->worldZ - effector->worldZ;
    const float spineLen = std::hypot(spineX, spineZ);
    if (spineLen > 1.0e-6f) {
      spineX /= spineLen;
      spineZ /= spineLen;
    } else {
      spineX = std::sin(intentHeading);
      spineZ = std::cos(intentHeading);
    }
  }

  const float intentX = std::sin(intentHeading);
  const float intentZ = std::cos(intentHeading);
  const float perpX = -spineZ;
  const float perpZ = spineX;
  const float lateral = intentX * perpX + intentZ * perpZ;
  const float carve = std::clamp(lateral * kCampThrustCarveGain * carveScale, -kCampThrustCarveMax,
                                 kCampThrustCarveMax);

  float dirX = spineX + perpX * carve;
  float dirZ = spineZ + perpZ * carve;
  const float dirLen = std::hypot(dirX, dirZ);
  if (dirLen > 1.0e-6f) {
    dirX /= dirLen;
    dirZ /= dirLen;
  }

  organism.pendingImpulseNodeId = effectorNodeId;
  organism.pendingImpulseX = dirX * mechanicalThrust;
  organism.pendingImpulseZ = dirZ * mechanicalThrust;
}

}  // namespace evolab
