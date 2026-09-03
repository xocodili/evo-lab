#include "sim/StemBinding.hpp"

#include "sim/CampLocomotionBody.hpp"
#include "sim/CampTopology.hpp"
#include "sim/CellConstants.hpp"
#include "sim/Chaos.hpp"
#include "sim/NeuralAxon.hpp"
#include "sim/NeuronCoordinator.hpp"
#include "sim/NeuronFuel.hpp"
#include "sim/NeuronStem.hpp"
#include "sim/OrganismComputer.hpp"
#include "sim/OrganismNeuron.hpp"
#include "sim/WorldConstants.hpp"

#include "engine/kinematics/Math.hpp"

#include <algorithm>
#include <cmath>
#include <random>

namespace evolab {

namespace {

using engine::kinematics::normalizeAngle;

void initAllComputerNodeRegisters(Organism& organism) {
  for (SkeletonNode& node : organism.nodes) {
    initCoordinatorNodeRegister(node);
    if (node.neuron == NeuronType::Computer) {
      initComputerNodeRegister(node);
    }
  }
}

SkeletonNode makeLocusNode(std::uint32_t id, NeuronType type, float wx, float wz, float wy) {
  SkeletonNode node;
  node.id = id;
  node.neuron = type;
  node.worldX = wx;
  node.worldZ = wz;
  node.worldY = wy;
  initCoordinatorNodeRegister(node);
  if (type == NeuronType::Computer) {
    initComputerNodeRegister(node);
  }
  return node;
}

const SkeletonNode* findComputerHub(const Organism& organism) {
  if (organism.computerNodeId != 0) {
    return organism.findNode(organism.computerNodeId);
  }
  return findNeuronNode(organism, NeuronType::Computer, false);
}

bool torpedoLinkJointMatchesHeading(float heading, float jointAngle, float headingToleranceRad) {
  const float expected = normalizeAngle(heading + kCampTorpedoForwardSegmentOffset);
  return std::fabs(normalizeAngle(jointAngle - expected)) <= headingToleranceRad;
}

}  // namespace

StemAssemblyPlan defaultCampStemAssemblyPlan() {
  StemAssemblyPlan plan;
  plan.loci = {
      {kCampPerceptorId, NeuronType::Perceptor},
      {kCampMouthId, NeuronType::Mouth},
      {kCampComputerId, NeuronType::Computer},
      {kCampActuatorId, NeuronType::Actuator},
  };
  for (const auto& edge : kCampTorpedoChainLinks) {
    StemChainRecord chain;
    chain.parentNodeId = edge.first;
    chain.childNodeId = edge.second;
    chain.segmentAngleOffset = kCampTorpedoForwardSegmentOffset;
    chain.muscleBundle = true;
    plan.chains.push_back(chain);
  }
  return plan;
}

StemAssemblyPlan extractStemAssemblyPlan(const Organism& organism) {
  StemAssemblyPlan plan;
  for (const SkeletonNode& node : organism.nodes) {
    if (!node.alive || node.neuron == NeuronType::None) {
      continue;
    }
    plan.loci.push_back({node.id, node.neuron});
  }

  if (organismHasCampTorpedoChain(organism)) {
    const float refYaw =
        organism.kinematicsBirthApplied_ ? organism.bodyDynamics.rootWorldYaw : organism.heading;
    for (const auto& edge : kCampTorpedoChainLinks) {
      for (const SkeletonLink& link : organism.links) {
        if (link.parentNodeId != edge.first || link.childNodeId != edge.second) {
          continue;
        }
        StemChainRecord record;
        record.parentNodeId = link.parentNodeId;
        record.childNodeId = link.childNodeId;
        record.segmentAngleOffset =
            normalizeAngle(link.jointAngle - refYaw);
        record.restLength = link.restLength;
        record.muscleBundle = link.muscleBundle;
        plan.chains.push_back(record);
        break;
      }
    }
    return plan;
  }

  const SkeletonNode* hub = findComputerHub(organism);
  if (hub == nullptr) {
    return plan;
  }

  const float refYaw =
      organism.kinematicsBirthApplied_ ? organism.bodyDynamics.rootWorldYaw : organism.heading;

  for (const SkeletonLink& link : organism.links) {
    if (link.parentNodeId != hub->id) {
      continue;
    }
    StemBindRecord record;
    record.hubNodeId = link.parentNodeId;
    record.peripheralNodeId = link.childNodeId;
    record.hubSlot = inferStemHubSlotFromAngle(refYaw, link.jointAngle);
    record.peripheralFace = static_cast<std::uint8_t>(StemFace::North);
    record.restLength = link.restLength;
    record.muscleBundle = link.muscleBundle;
    plan.binds.push_back(record);
  }

  std::sort(plan.binds.begin(), plan.binds.end(),
            [](const StemBindRecord& a, const StemBindRecord& b) { return a.hubSlot < b.hubSlot; });
  return plan;
}

void assignStemAssemblyPlan(Organism& organism, StemAssemblyPlan plan) {
  organism.stemAssembly = std::move(plan);
}

void closeStemNeuralGraphAmongLoci(Organism& organism) {
  std::vector<std::uint32_t> locusIds;
  locusIds.reserve(organism.nodes.size());
  for (const SkeletonNode& node : organism.nodes) {
    if (!node.alive || node.neuron == NeuronType::None) {
      continue;
    }
    locusIds.push_back(node.id);
  }
  for (std::uint32_t src : locusIds) {
    for (std::uint32_t dst : locusIds) {
      if (src == dst) {
        continue;
      }
      if (organism.findNeuralAxon(src, dst) != nullptr) {
        continue;
      }
      if (organism.neuralAxons.size() >= kAxonChannelCapacity) {
        return;
      }
      organism.neuralAxons.push_back(makeDevelopmentalAxon(src, dst));
    }
  }
}

Organism assembleOrganismFromStemPlan(std::uint32_t id, float wx, float wz, float wy,
                                      std::size_t storageBytes, std::uint64_t createdAtTick,
                                      float boneLength, const StemAssemblyPlan& plan,
                                      float heading) {
  Organism organism;
  organism.id = id;
  organism.createdAtTick = createdAtTick;
  seedCampLocomotionBodyYaw(organism, heading);
  organism.stemAssembly = plan;

  std::mt19937 rng =
      chaosSpawnRng(createdAtTick, static_cast<std::uint64_t>(id) ^ kChaosSaltStemCell);

  StemBindAttempt assemblyAttempt;
  assemblyAttempt.requireEntropy = false;
  assemblyAttempt.requirePayment = false;

  std::uint32_t hubId = 0;
  for (const StemLocusSpec& spec : plan.loci) {
    organism.nodes.push_back(makeLocusNode(spec.nodeId, spec.type, wx, wz, wy));
    if (spec.type == NeuronType::Computer && hubId == 0) {
      hubId = spec.nodeId;
      organism.computerNodeId = hubId;
    }
  }

  if (hubId == 0) {
    return organism;
  }

  if (!plan.chains.empty()) {
    organism.rootNodeId = kCampMouthId;
    organism.stemAssembly.chains.clear();
    for (const StemChainRecord& record : plan.chains) {
      const float length = record.restLength > 0.0f ? record.restLength : boneLength;
      tryStemBindChainSegment(organism, record.parentNodeId, record.childNodeId,
                              record.segmentAngleOffset, length, heading, assemblyAttempt, rng);
    }
  } else if (!plan.binds.empty()) {
    organism.rootNodeId = hubId;
    organism.stemAssembly.binds.clear();
    for (const StemBindRecord& record : plan.binds) {
      const float length = record.restLength > 0.0f ? record.restLength : boneLength;
      tryStemBindPeripheralToHub(organism, record.hubNodeId, record.peripheralNodeId,
                                 record.hubSlot, length, heading, assemblyAttempt, rng);
    }
  } else {
    organism.rootNodeId = hubId;
    int slot = 0;
    for (const StemLocusSpec& spec : plan.loci) {
      if (spec.type == NeuronType::Computer || spec.type == NeuronType::None) {
        continue;
      }
      if (slot >= static_cast<int>(kWorldHubSocketCount)) {
        break;
      }
      tryStemBindPeripheralToHub(organism, hubId, spec.nodeId, static_cast<std::uint8_t>(slot),
                                 boneLength, heading, assemblyAttempt, rng);
      ++slot;
    }
  }

  closeStemNeuralGraphAmongLoci(organism);
  endowCampNodes(organism, storageBytes);
  initAllComputerNodeRegisters(organism);
  organism.senseRadiusFactor = kPerceptorSenseRadiusFactor;
  organism.stemAssembly = extractStemAssemblyPlan(organism);
  return organism;
}

bool organismStemBindGeometryMatchesCamp(const Organism& organism, float headingToleranceRad) {
  if (organismHasCampTorpedoChain(organism)) {
    int matched = 0;
    for (const auto& edge : kCampTorpedoChainLinks) {
      for (const SkeletonLink& link : organism.links) {
        if (link.parentNodeId != edge.first || link.childNodeId != edge.second) {
          continue;
        }
        if (torpedoLinkJointMatchesHeading(organism.heading, link.jointAngle,
                                           headingToleranceRad)) {
          ++matched;
        }
        break;
      }
    }
    return matched >= static_cast<int>(kCampTorpedoChainSegmentCount);
  }

  if (!organismHasCampHubArms(organism)) {
    return false;
  }
  const SkeletonNode* hub = findComputerHub(organism);
  if (hub == nullptr) {
    return false;
  }
  int matchedSlots = 0;
  for (const SkeletonLink& link : organism.links) {
    if (link.parentNodeId != hub->id) {
      continue;
    }
    const std::uint8_t slot = inferStemHubSlotFromAngle(organism.heading, link.jointAngle);
    const float expected = hubSocketAngleRad(organism.heading, slot);
    if (std::fabs(normalizeAngle(link.jointAngle - expected)) <= headingToleranceRad) {
      ++matchedSlots;
    }
  }
  return matchedSlots >= 3;
}

bool organismUsesStemBindRecords(const Organism& organism) {
  return !organism.stemAssembly.binds.empty() || !organism.stemAssembly.chains.empty();
}

std::uint32_t stemBindStepCount(const Organism& organism) {
  return static_cast<std::uint32_t>(organism.stemAssembly.binds.size() +
                                    organism.stemAssembly.chains.size());
}

void reconcileStemAssemblyLinks(Organism& organism, float heading, std::mt19937& rng) {
  StemBindAttempt attempt;
  attempt.requireEntropy = false;
  attempt.requirePayment = false;
  for (const StemChainRecord& record : organism.stemAssembly.chains) {
    bool hasLink = false;
    for (const SkeletonLink& link : organism.links) {
      if (link.parentNodeId == record.parentNodeId && link.childNodeId == record.childNodeId) {
        hasLink = true;
        break;
      }
    }
    if (hasLink) {
      continue;
    }
    const float length =
        record.restLength > 0.0f ? record.restLength : nominalBoneLength(kWorldCellSize);
    tryStemBindChainSegment(organism, record.parentNodeId, record.childNodeId,
                            record.segmentAngleOffset, length, heading, attempt, rng);
  }
  for (const StemBindRecord& record : organism.stemAssembly.binds) {
    bool hasLink = false;
    for (const SkeletonLink& link : organism.links) {
      if (link.parentNodeId == record.hubNodeId && link.childNodeId == record.peripheralNodeId) {
        hasLink = true;
        break;
      }
    }
    if (hasLink) {
      continue;
    }
    const float length = record.restLength > 0.0f ? record.restLength : nominalBoneLength(kWorldCellSize);
    tryStemBindPeripheralToHub(organism, record.hubNodeId, record.peripheralNodeId, record.hubSlot,
                               length, heading, attempt, rng);
  }
}

}  // namespace evolab
