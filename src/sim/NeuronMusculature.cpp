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

namespace {

float muscleTargetYawDelta(const Organism& organism, std::uint32_t parentId,
                           std::uint32_t childId) {
  const float tension = campAxonBundleTension(organism, parentId, childId);
  float yawDelta = tension * kAxonBundleFlexGain;
  if (organism.lastActuatorStrokeFlexBoost > 0.0f) {
    if (childId == kCampActuatorId) {
      yawDelta += organism.lastActuatorStrokeFlexBoost * kActuatorStrokeFlexGain;
    } else if (childId == kCampPerceptorId || childId == kCampMouthId) {
      yawDelta -= organism.lastActuatorStrokeFlexBoost * kAxonBundleTrailFlexGain;
    }
  }
  return yawDelta;
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
  if (!organism.isCampNom()) {
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
    if (organism.lastStrokePaid && joint.nodeId == kCampActuatorId &&
        organism.lastActuatorStrokeFlexBoost > 0.0f) {
      command.stiffness *= kStrokeMuscleStiffnessBoost;
    }
    commands.push_back(command);
  }

  return commands;
}

float campKeelYawTorque(const Organism& organism) {
  if (!organism.isCampNom()) {
    return 0.0f;
  }
  const SkeletonNode* hub = organism.findNode(kCampRootNodeId);
  if (hub == nullptr) {
    return 0.0f;
  }
  const float tensionP = campAxonBundleTension(organism, hub->id, kCampPerceptorId);
  const float tensionM = campAxonBundleTension(organism, hub->id, kCampMouthId);
  const float tensionA = campAxonBundleTension(organism, hub->id, kCampActuatorId);
  return (tensionP - tensionM) + (tensionA - (tensionP + tensionM) * 0.5f) * 0.35f;
}

void queueCampStrokeImpulse(Organism& organism, float mechanicalThrust, float thrustHeading) {
  const float dirX = std::sin(thrustHeading);
  const float dirZ = std::cos(thrustHeading);

  organism.pendingImpulseNodeId = kCampActuatorId;
  organism.pendingImpulseX = dirX * mechanicalThrust;
  organism.pendingImpulseZ = dirZ * mechanicalThrust;
  organism.lastActuatorStrokeFlexBoost = mechanicalThrust;

  const float keelTorque = campKeelYawTorque(organism) * mechanicalThrust * kAxonBundleKeelYawGain;
  organism.bodyDynamics.rootYawRate += keelTorque * kBodyInvInertia;
}

}  // namespace evolab
