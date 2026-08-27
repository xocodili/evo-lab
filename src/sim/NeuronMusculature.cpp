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

    const float tension = campAxonBundleTension(organism, parentJoint.nodeId, joint.nodeId);
    float yawDelta = tension * kAxonBundleFlexGain;
    if (organism.lastActuatorStrokeFlexBoost > 0.0f) {
      if (joint.nodeId == kCampActuatorId) {
        yawDelta += organism.lastActuatorStrokeFlexBoost * kActuatorStrokeFlexGain;
      } else if (joint.nodeId == kCampPerceptorId || joint.nodeId == kCampMouthId) {
        yawDelta -= organism.lastActuatorStrokeFlexBoost * kAxonBundleTrailFlexGain;
      }
    }
    pose.yawDelta(jointIndex) = yawDelta;
  }

  return pose;
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

namespace {

float normalizeHeading(float radians) {
  constexpr float kTwoPi = 6.2831853f;
  while (radians > 3.14159265f) {
    radians -= kTwoPi;
  }
  while (radians < -3.14159265f) {
    radians += kTwoPi;
  }
  return radians;
}

}  // namespace

void applyCampBundleStroke(Organism& organism, SkeletonNode& motor, SkeletonNode& hub,
                           float mechanicalThrust) {
  const float armDx = motor.worldX - hub.worldX;
  const float armDz = motor.worldZ - hub.worldZ;
  const float armLen = std::hypot(armDx, armDz);
  float thrustX = std::sin(organism.heading);
  float thrustZ = std::cos(organism.heading);
  if (armLen > 1.0e-5f) {
    const float alongX = armDx / armLen;
    const float alongZ = armDz / armLen;
    thrustX = alongX * 0.78f + thrustX * 0.22f;
    thrustZ = alongZ * 0.78f + thrustZ * 0.22f;
    const float thrustLen = std::hypot(thrustX, thrustZ);
    if (thrustLen > 1.0e-5f) {
      thrustX /= thrustLen;
      thrustZ /= thrustLen;
    }
  }

  const float hubMove = mechanicalThrust * kActuatorHubThrustShare;
  hub.worldX += thrustX * hubMove;
  hub.worldZ += thrustZ * hubMove;

  organism.lastActuatorStrokeFlexBoost = mechanicalThrust;
  const float keelTorque = campKeelYawTorque(organism) * mechanicalThrust * kAxonBundleKeelYawGain;
  organism.heading = normalizeHeading(organism.heading + keelTorque);
}

}  // namespace evolab
