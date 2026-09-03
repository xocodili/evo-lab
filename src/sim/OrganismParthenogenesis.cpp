#include "sim/NeuronFuel.hpp"
#include "sim/OrganismParthenogenesis.hpp"
#include "sim/OrganismComputer.hpp"
#include "sim/NeuronCoordinator.hpp"
#include "sim/OrganismFeedbagOracle.hpp"
#include "sim/Organism.hpp"

#include "sim/Chaos.hpp"
#include "sim/BarrenWorld.hpp"
#include "sim/CampTopology.hpp"
#include "sim/CellConstants.hpp"
#include "sim/CloacaSignal.hpp"
#include "sim/NeuralAxon.hpp"
#include "sim/NeuronStem.hpp"
#include "sim/StemBinding.hpp"
#include "sim/OrganismKinematicBirth.hpp"
#include "sim/WaterColumn.hpp"
#include "sim/WorldBinding.hpp"
#include "sim/WorldConstants.hpp"

#include "engine/kinematics/Math.hpp"

#include <algorithm>
#include <iostream>
#include <cmath>
#include <random>
#include <vector>

namespace evolab {

namespace {

using engine::kinematics::normalizeAngle;

enum class StructuralOp : std::uint8_t { Duplication, Deletion, Insertion };

enum class MorphogenesisKind : std::uint8_t { Locus, Axon, Link, Bind };

struct MorphogenesisStep {
  MorphogenesisKind kind = MorphogenesisKind::Locus;
  std::uint32_t nodeId = 0;
  std::uint32_t axonSrcId = 0;
  std::uint32_t axonDstId = 0;
  std::uint32_t linkParentId = 0;
  std::uint32_t linkChildId = 0;
  std::uint8_t hubSlot = 0;
};

float effectiveStructuralRate(const ParthenogenesisPassOptions& options) {
  if (options.structuralRateOverride >= 0.0f && options.structuralRateOverride <= 1.0f) {
    return options.structuralRateOverride;
  }
  return kParthenogenesisStructuralRate;
}

bool parentHasBasalArrears(const Organism& organism) {
  for (const SkeletonNode& node : organism.nodes) {
    if (node.alive && node.basalArrearsTicks >= kNeuronBasalGraceTicks) {
      return true;
    }
  }
  return false;
}

bool consumeHubBytes(Organism& organism, std::uint32_t amount, std::uint32_t& bytesSpent) {
  if (amount == 0) {
    return true;
  }
  SkeletonNode* computer = findComputerHubNode(organism);
  if (computer == nullptr) {
    return false;
  }
  if (computer->store.size() < amount) {
    bytesSpent += static_cast<std::uint32_t>(computer->store.size());
    computer->store.clear();
    return false;
  }
  consumeFuelBack(computer->store, amount);
  bytesSpent += amount;
  return true;
}

void creditHubBytes(Organism& organism, std::uint32_t amount) {
  if (amount == 0) {
    return;
  }
  SkeletonNode* computer = findComputerHubNode(organism);
  if (computer == nullptr) {
    return;
  }
  const std::size_t cap = hubStoreCapBytes(organism);
  const std::size_t room = cap > computer->store.size() ? cap - computer->store.size() : 0;
  const std::size_t credit = std::min<std::size_t>(room, amount);
  if (credit == 0) {
    return;
  }
  computer->store.insert(computer->store.end(), credit, 1);
}

bool canAffordReserveAfterSpend(const Organism& organism, std::uint32_t spent) {
  const std::size_t remaining = computerHubFuelBytes(organism);
  (void)spent;
  return remaining >= kParthenogenesisParentReserveMin;
}

void jitterAxonGate2(NeuralAxon& axon, std::mt19937& rng) {
  for (std::uint16_t& trust : axon.trustBelieveByConfidence) {
    trust = chaosJitterTrust(trust, rng);
  }
  axon.trustFeed = chaosJitterTrust(axon.trustFeed, rng);
  axon.etaSignal = chaosJitterFloat(axon.etaSignal, rng);
  axon.etaEnergy = chaosJitterFloat(axon.etaEnergy, rng);
  axon.pendingSend = {};
  axon.lastReceived = {};
  axon.lastSentByte = 0;
  axon.uncappedNodeId = 0;
  axon.uncappedNeuronTypeRaw = 0;
  axon.transitArrearsTicks = 0;
}

void jitterOrganismGate2(Organism& organism, const Organism& parent, std::mt19937& rng) {
  organism.senseRadiusFactor = chaosJitterFloat(parent.senseRadiusFactor, rng);
  organism.tumbleRateFactor = chaosJitterFloat(parent.tumbleRateFactor, rng);
  organism.tumbleTurnFactor = chaosJitterFloat(parent.tumbleTurnFactor, rng);
  organism.tumbleChiralityBias =
      std::clamp(chaosJitterFloat(parent.tumbleChiralityBias, rng), -kTumbleChiralityBiasMax,
                 kTumbleChiralityBiasMax);
  organism.peripheralStoreCapFactor =
      clampWalletCapFactor(chaosJitterFloat(parent.peripheralStoreCapFactor, rng));
  organism.hubStoreCapFactor =
      clampWalletCapFactor(chaosJitterFloat(parent.hubStoreCapFactor, rng));
  organism.equilibriumExportStartUnit =
      std::clamp(chaosJitterFloat(parent.equilibriumExportStartUnit, rng),
                 kStemEquilibriumExportStartMin, kStemEquilibriumExportStartMax);
  organism.cloacaDistressByte = parent.cloacaDistressByte;
  organism.cloacaBaselineByte = parent.cloacaBaselineByte;
  organism.cloacaMateByte = parent.cloacaMateByte;
  jitterCloacaPaletteBytes(organism, rng);
  organism.heading = chaosJitterHeading(parent.heading, rng);
  organism.bodyDynamics.rootWorldYaw = organism.heading;
  for (SkeletonNode& node : organism.nodes) {
    if (node.neuron != NeuronType::Computer) {
      continue;
    }
    const SkeletonNode* parentNode = parent.findNode(node.id);
    if (parentNode != nullptr && parentNode->neuron == NeuronType::Computer) {
      node.computerRegister = parentNode->computerRegister;
      node.coordinatorRegister = parentNode->coordinatorRegister;
    } else {
      initCoordinatorNodeRegister(node);
      initComputerNodeRegister(node);
    }
    for (std::size_t i = 0; i < 7; ++i) {
      const int jitter = chaosBernoulli(0.5f, rng) ? 1 : -1;
      const int next = static_cast<int>(node.computerRegister[i]) + jitter;
      node.computerRegister[i] =
          static_cast<std::uint8_t>(std::clamp(next, 0, static_cast<int>(kNeuronConfidenceMax)));
    }
    guardComputerNodeRegister(node);
  }
  for (SkeletonLink& link : organism.links) {
    const SkeletonLink* parentLink = nullptr;
    for (const SkeletonLink& candidate : parent.links) {
      if (candidate.parentNodeId == link.parentNodeId &&
          candidate.childNodeId == link.childNodeId) {
        parentLink = &candidate;
        break;
      }
    }
    if (parentLink == nullptr) {
      continue;
    }
    link.restLength = chaosJitterFloat(parentLink->restLength, rng);
    link.energyEta = chaosJitterFloat(parentLink->energyEta, rng);
    bool stemGeometryLink = false;
    for (const StemChainRecord& record : organism.stemAssembly.chains) {
      if (record.parentNodeId == link.parentNodeId &&
          record.childNodeId == link.childNodeId) {
        stemGeometryLink = true;
        link.jointAngle =
            normalizeAngle(organism.bodyDynamics.rootWorldYaw + record.segmentAngleOffset);
        link.muscleBundle = record.muscleBundle;
        break;
      }
    }
    for (const StemBindRecord& record : organism.stemAssembly.binds) {
      if (record.hubNodeId == link.parentNodeId &&
          record.peripheralNodeId == link.childNodeId) {
        stemGeometryLink = true;
        link.jointAngle = hubSocketAngleRad(organism.bodyDynamics.rootWorldYaw, record.hubSlot);
        link.muscleBundle = record.muscleBundle;
        break;
      }
    }
    if (!stemGeometryLink) {
      link.jointAngle = chaosJitterFloat(parentLink->jointAngle, rng);
    }
  }
}

void guardCampComputerRegister(Organism& organism) {
  for (SkeletonNode& node : organism.nodes) {
    if (node.alive && node.neuron == NeuronType::Computer) {
      guardComputerNodeRegister(node);
    }
  }
}

std::uint32_t maxNodeId(const Organism& organism) {
  std::uint32_t maxId = 0;
  for (const SkeletonNode& node : organism.nodes) {
    maxId = std::max(maxId, node.id);
  }
  return maxId;
}

int findNodeIndexById(const Organism& organism, std::uint32_t nodeId) {
  for (std::size_t i = 0; i < organism.nodes.size(); ++i) {
    if (organism.nodes[i].id == nodeId) {
      return static_cast<int>(i);
    }
  }
  return -1;
}

int findAxonIndexByEdge(const Organism& organism, std::uint32_t srcId, std::uint32_t dstId) {
  for (std::size_t i = 0; i < organism.neuralAxons.size(); ++i) {
    const NeuralAxon& axon = organism.neuralAxons[i];
    if (axon.srcNodeId == srcId && axon.dstNodeId == dstId) {
      return static_cast<int>(i);
    }
  }
  return -1;
}

int findLinkIndexByEdge(const Organism& organism, std::uint32_t parentId, std::uint32_t childId) {
  for (std::size_t i = 0; i < organism.links.size(); ++i) {
    const SkeletonLink& link = organism.links[i];
    if (link.parentNodeId == parentId && link.childNodeId == childId) {
      return static_cast<int>(i);
    }
  }
  return -1;
}

int countLiveNeurons(const Organism& organism, NeuronType type) {
  int count = 0;
  for (const SkeletonNode& node : organism.nodes) {
    if (node.alive && node.neuron == type) {
      ++count;
    }
  }
  return count;
}

bool canRemoveNeuronType(const Organism& organism, NeuronType type) {
  return countLiveNeurons(organism, type) > 1;
}

const SkeletonNode* findComputerRoot(const Organism& organism) {
  if (organism.computerNodeId != 0) {
    return organism.findNode(organism.computerNodeId);
  }
  return findNeuronNode(organism, NeuronType::Computer, false);
}

void resetSpawnNodeRuntime(SkeletonNode& node) {
  node.alive = true;
  node.basalArrearsTicks = 0;
  node.ateThisTick = false;
  node.mouthChewFill = 0;
  node.mouthChewPaused = false;
  node.lastEmittedByte = 0;
  node.store.clear();
}

void assignChildCampEndowment(Organism& child) {
  EndowCampOptions options;
  options.clampToWalletCaps = false;
  endowCampNodesFromSplit(child, splitCampStorage(kParthenogenesisChildEndowmentBytes), options);
}

Organism cloneParentStructure(const Organism& parent, std::uint32_t childId, float wx, float wz,
                              float wy, std::uint64_t simTick) {
  Organism child;
  child.id = childId;
  child.createdAtTick = simTick;
  child.alive = true;
  child.rootNodeId = parent.rootNodeId;
  child.computerNodeId = parent.computerNodeId;
  child.bodyDynamics = {};
  child.kinematicsBirthApplied_ = false;
  child.heading = parent.heading;
  child.senseRadiusFactor = parent.senseRadiusFactor;
  child.tumbleRateFactor = parent.tumbleRateFactor;
  child.tumbleTurnFactor = parent.tumbleTurnFactor;
  child.tumbleChiralityBias = parent.tumbleChiralityBias;
  child.peripheralStoreCapFactor = parent.peripheralStoreCapFactor;
  child.hubStoreCapFactor = parent.hubStoreCapFactor;
  child.cloacaDistressByte = parent.cloacaDistressByte;
  child.cloacaBaselineByte = parent.cloacaBaselineByte;
  child.cloacaMateByte = parent.cloacaMateByte;
  child.nodes = parent.nodes;
  child.links = parent.links;
  child.neuralAxons = parent.neuralAxons;
  child.stemAssembly =
      parent.stemAssembly.binds.empty() ? extractStemAssemblyPlan(parent) : parent.stemAssembly;

  ensureCampDevelopmentalAxons(child);

  for (SkeletonNode& node : child.nodes) {
    node.worldX = wx;
    node.worldZ = wz;
    node.worldY = wy;
    resetSpawnNodeRuntime(node);
  }

  assignChildCampEndowment(child);
  return child;
}

std::vector<MorphogenesisStep> buildMorphogenesisPlan(const Organism& child) {
  std::vector<MorphogenesisStep> plan;
  plan.reserve(child.nodes.size() + child.neuralAxons.size() + child.links.size() +
               child.stemAssembly.binds.size() + child.stemAssembly.chains.size());
  for (const SkeletonNode& node : child.nodes) {
    plan.push_back({MorphogenesisKind::Locus, node.id, 0, 0, 0, 0, 0});
  }
  for (const StemChainRecord& chain : child.stemAssembly.chains) {
    plan.push_back({MorphogenesisKind::Bind, 0, 0, 0, chain.parentNodeId, chain.childNodeId, 0});
  }
  for (const StemBindRecord& bind : child.stemAssembly.binds) {
    plan.push_back({MorphogenesisKind::Bind, 0, 0, 0, bind.hubNodeId, bind.peripheralNodeId,
                    bind.hubSlot});
  }
  for (const NeuralAxon& axon : child.neuralAxons) {
    plan.push_back({MorphogenesisKind::Axon, 0, axon.srcNodeId, axon.dstNodeId, 0, 0, 0});
  }
  for (const SkeletonLink& link : child.links) {
    bool coveredByBind = false;
    for (const StemChainRecord& chain : child.stemAssembly.chains) {
      if (chain.parentNodeId == link.parentNodeId && chain.childNodeId == link.childNodeId) {
        coveredByBind = true;
        break;
      }
    }
    for (const StemBindRecord& bind : child.stemAssembly.binds) {
      if (bind.hubNodeId == link.parentNodeId && bind.peripheralNodeId == link.childNodeId) {
        coveredByBind = true;
        break;
      }
    }
    if (coveredByBind) {
      continue;
    }
    plan.push_back({MorphogenesisKind::Link, 0, 0, 0, link.parentNodeId, link.childNodeId, 0});
  }
  return plan;
}

void duplicateAxonMotifForNode(Organism& child, std::uint32_t sourceId, std::uint32_t newId) {
  const std::size_t axonCount = child.neuralAxons.size();
  for (std::size_t i = 0; i < axonCount; ++i) {
    if (child.neuralAxons.size() >= kAxonChannelCapacity) {
      break;
    }
    const NeuralAxon& axon = child.neuralAxons[i];
    if (axon.srcNodeId == sourceId) {
      NeuralAxon copy = axon;
      copy.srcNodeId = newId;
      child.neuralAxons.push_back(copy);
    }
    if (child.neuralAxons.size() >= kAxonChannelCapacity) {
      break;
    }
    if (axon.dstNodeId == sourceId) {
      NeuralAxon copy = axon;
      copy.dstNodeId = newId;
      child.neuralAxons.push_back(copy);
    }
  }
}

bool applyStructuralOpLocus(Organism& child, std::size_t index, StructuralOp op,
                            std::mt19937& rng) {
  if (index >= child.nodes.size()) {
    return false;
  }
  SkeletonNode& source = child.nodes[index];
  if (!source.alive) {
    return false;
  }

  switch (op) {
    case StructuralOp::Duplication: {
      if (child.nodes.size() >= kCampMorphogenesisMaxNeurons) {
        return false;
      }
      const std::uint32_t newId = maxNodeId(child) + 1;
      SkeletonNode copy = source;
      copy.id = newId;
      resetSpawnNodeRuntime(copy);
      if (copy.neuron == NeuronType::Computer) {
        copy.computerRegister = source.computerRegister;
        guardComputerNodeRegister(copy);
      }
      child.nodes.insert(child.nodes.begin() + static_cast<std::ptrdiff_t>(index + 1), copy);

      std::vector<SkeletonLink> addedLinks;
      for (const SkeletonLink& link : child.links) {
        if (link.childNodeId == source.id) {
          SkeletonLink arm = link;
          arm.childNodeId = newId;
          arm.jointAngle += std::uniform_real_distribution<float>(-0.35f, 0.35f)(rng);
          addedLinks.push_back(arm);
        }
      }
      if (addedLinks.empty()) {
        if (const SkeletonNode* root = findComputerRoot(child)) {
          const int slot = nextFreeHubSlot(child, root->id);
          if (slot >= 0) {
            const float restLength = child.links.empty() ? nominalBoneLength(kWorldCellSize)
                                                         : child.links.front().restLength;
            StemBindAttempt attempt;
            attempt.requireEntropy = false;
            attempt.requirePayment = false;
            StemBindResult bound = tryStemBindPeripheralToHub(
                child, root->id, newId, static_cast<std::uint8_t>(slot), restLength, child.heading,
                attempt, rng);
            if (bound.ok) {
              duplicateAxonMotifForNode(child, source.id, newId);
              return true;
            }
          }
        }
      }
      child.links.insert(child.links.end(), addedLinks.begin(), addedLinks.end());
      duplicateAxonMotifForNode(child, source.id, newId);
      return true;
    }
    case StructuralOp::Deletion: {
      if (!canRemoveNeuronType(child, source.neuron)) {
        return false;
      }
      const std::uint32_t doomedId = source.id;
      child.nodes.erase(child.nodes.begin() + static_cast<std::ptrdiff_t>(index));
      child.neuralAxons.erase(
          std::remove_if(child.neuralAxons.begin(), child.neuralAxons.end(),
                         [doomedId](const NeuralAxon& axon) {
                           return axon.srcNodeId == doomedId || axon.dstNodeId == doomedId;
                         }),
          child.neuralAxons.end());
      child.links.erase(
          std::remove_if(child.links.begin(), child.links.end(),
                         [doomedId](const SkeletonLink& link) {
                           return link.parentNodeId == doomedId || link.childNodeId == doomedId;
                         }),
          child.links.end());
      if (child.rootNodeId == doomedId) {
        if (const SkeletonNode* replacement = findPrimaryMouthNode(child)) {
          child.rootNodeId = replacement->id;
        } else if (const SkeletonNode* replacement = findComputerRoot(child)) {
          child.rootNodeId = replacement->id;
          child.computerNodeId = replacement->id;
        } else if (!child.nodes.empty()) {
          child.rootNodeId = child.nodes.front().id;
        }
      } else if (child.computerNodeId == doomedId) {
        if (const SkeletonNode* replacement = findComputerRoot(child)) {
          child.computerNodeId = replacement->id;
        } else {
          child.computerNodeId = 0;
        }
      }
      return true;
    }
    case StructuralOp::Insertion: {
      if (child.nodes.size() >= kCampMorphogenesisMaxNeurons) {
        return false;
      }
      static constexpr NeuronType kPool[] = {NeuronType::Perceptor, NeuronType::Mouth,
                                             NeuronType::Computer, NeuronType::Actuator};
      const NeuronType insertedType = kPool[std::uniform_int_distribution<int>(0, 3)(rng)];
      const std::uint32_t newId = maxNodeId(child) + 1;
      SkeletonNode inserted;
      inserted.id = newId;
      inserted.neuron = insertedType;
      inserted.worldX = source.worldX;
      inserted.worldZ = source.worldZ;
      inserted.worldY = source.worldY;
      resetSpawnNodeRuntime(inserted);
      initCoordinatorNodeRegister(inserted);
      if (inserted.neuron == NeuronType::Computer) {
        initComputerNodeRegister(inserted);
      }
      child.nodes.insert(child.nodes.begin() + static_cast<std::ptrdiff_t>(index + 1), inserted);

      if (const SkeletonNode* root = findComputerRoot(child)) {
        const int slot = nextFreeHubSlot(child, root->id);
        if (slot >= 0) {
          StemBindAttempt attempt;
          attempt.requireEntropy = false;
          attempt.requirePayment = false;
          if (tryStemBindPeripheralToHub(child, root->id, newId, static_cast<std::uint8_t>(slot),
                                         child.links.empty() ? nominalBoneLength(kWorldCellSize)
                                                             : child.links.front().restLength,
                                         child.heading, attempt, rng)
                  .ok) {
            return true;
          }
        }
      }
      return true;
    }
  }
  return false;
}

bool applyStructuralOpLink(Organism& child, std::size_t index, StructuralOp op,
                           std::mt19937& rng) {
  if (index >= child.links.size()) {
    return false;
  }
  switch (op) {
    case StructuralOp::Duplication: {
      child.links.push_back(child.links[index]);
      child.links.back().jointAngle += std::uniform_real_distribution<float>(-0.35f, 0.35f)(rng);
      return true;
    }
    case StructuralOp::Deletion: {
      if (child.links.size() <= 1) {
        return false;
      }
      const SkeletonLink& doomed = child.links[index];
      if (isCampTorpedoChainLinkEdge(doomed.parentNodeId, doomed.childNodeId)) {
        return false;
      }
      if (doomed.parentNodeId == child.rootNodeId) {
        const SkeletonNode* peripheral = child.findNode(doomed.childNodeId);
        if (peripheral != nullptr && peripheral->alive &&
            (peripheral->neuron == NeuronType::Perceptor ||
             peripheral->neuron == NeuronType::Mouth ||
             peripheral->neuron == NeuronType::Actuator)) {
          int hubArmCount = 0;
          for (const SkeletonLink& link : child.links) {
            if (link.parentNodeId != child.rootNodeId ||
                link.childNodeId != doomed.childNodeId) {
              continue;
            }
            const SkeletonNode* node = child.findNode(link.childNodeId);
            if (node != nullptr && node->alive && node->neuron == peripheral->neuron) {
              ++hubArmCount;
            }
          }
          if (hubArmCount <= 1) {
            return false;
          }
        }
      }
      child.links.erase(child.links.begin() + static_cast<std::ptrdiff_t>(index));
      return true;
    }
    case StructuralOp::Insertion: {
      SkeletonLink reversed = child.links[index];
      std::swap(reversed.parentNodeId, reversed.childNodeId);
      child.links.push_back(reversed);
      return true;
    }
  }
  return false;
}

bool isDevelopmentalAxonEdge(std::uint32_t src, std::uint32_t dst) {
  return isCampDevelopmentalAxonEdge(src, dst);
}

bool applyStructuralOpAxon(std::vector<NeuralAxon>& axons, StructuralOp op, std::mt19937& rng,
                           std::size_t indexHint = static_cast<std::size_t>(-1)) {
  if (axons.empty()) {
    return false;
  }
  const std::size_t pickIndex =
      indexHint < axons.size()
          ? indexHint
          : static_cast<std::size_t>(
                std::uniform_int_distribution<int>(0, static_cast<int>(axons.size()) - 1)(rng));
  switch (op) {
    case StructuralOp::Deletion: {
      const NeuralAxon& axon = axons[pickIndex];
      if (isDevelopmentalAxonEdge(axon.srcNodeId, axon.dstNodeId)) {
        return false;
      }
      axons.erase(axons.begin() + static_cast<std::ptrdiff_t>(pickIndex));
      return true;
    }
    case StructuralOp::Duplication: {
      if (axons.size() >= kAxonChannelCapacity) {
        return false;
      }
      axons.push_back(axons[pickIndex]);
      return true;
    }
    case StructuralOp::Insertion: {
      if (axons.size() >= kAxonChannelCapacity) {
        return false;
      }
      NeuralAxon inserted = axons[pickIndex];
      inserted.srcNodeId = axons[pickIndex].dstNodeId;
      inserted.dstNodeId = axons[pickIndex].srcNodeId;
      if (std::any_of(axons.begin(), axons.end(), [&](const NeuralAxon& axon) {
            return axon.srcNodeId == inserted.srcNodeId && axon.dstNodeId == inserted.dstNodeId;
          })) {
        return false;
      }
      axons.push_back(inserted);
      return true;
    }
  }
  return false;
}

bool replayMorphogenesisBindStep(Organism& child, const MorphogenesisStep& step,
                                 std::mt19937& rng) {
  for (const SkeletonLink& link : child.links) {
    if (link.parentNodeId == step.linkParentId && link.childNodeId == step.linkChildId) {
      return true;
    }
  }
  StemBindAttempt attempt;
  attempt.requireEntropy = false;
  attempt.requirePayment = false;
  float restLength = nominalBoneLength(kWorldCellSize);
  float segmentOffset = kCampTorpedoForwardSegmentOffset;
  for (const StemChainRecord& record : child.stemAssembly.chains) {
    if (record.parentNodeId != step.linkParentId || record.childNodeId != step.linkChildId) {
      continue;
    }
    if (record.restLength > 0.0f) {
      restLength = record.restLength;
    }
    segmentOffset = record.segmentAngleOffset;
    return tryStemBindChainSegment(child, step.linkParentId, step.linkChildId, segmentOffset,
                                   restLength, child.heading, attempt, rng)
        .ok;
  }
  for (const StemBindRecord& record : child.stemAssembly.binds) {
    if (record.hubNodeId != step.linkParentId ||
        record.peripheralNodeId != step.linkChildId) {
      continue;
    }
    if (record.restLength > 0.0f) {
      restLength = record.restLength;
    }
    return tryStemBindPeripheralToHub(child, step.linkParentId, step.linkChildId, step.hubSlot,
                                      restLength, child.heading, attempt, rng)
        .ok;
  }
  return tryStemBindPeripheralToHub(child, step.linkParentId, step.linkChildId, step.hubSlot,
                                    restLength, child.heading, attempt, rng)
      .ok;
}

bool applyMorphogenesisStructuralOp(Organism& child, const MorphogenesisStep& step, StructuralOp op,
                                    std::mt19937& rng) {
  switch (step.kind) {
    case MorphogenesisKind::Locus: {
      const int nodeIndex = findNodeIndexById(child, step.nodeId);
      if (nodeIndex < 0) {
        return false;
      }
      return applyStructuralOpLocus(child, static_cast<std::size_t>(nodeIndex), op, rng);
    }
    case MorphogenesisKind::Axon: {
      const int axonIndex = findAxonIndexByEdge(child, step.axonSrcId, step.axonDstId);
      if (axonIndex < 0) {
        return false;
      }
      return applyStructuralOpAxon(child.neuralAxons, op, rng,
                                   static_cast<std::size_t>(axonIndex));
    }
    case MorphogenesisKind::Link: {
      const int linkIndex = findLinkIndexByEdge(child, step.linkParentId, step.linkChildId);
      if (linkIndex < 0) {
        return false;
      }
      return applyStructuralOpLink(child, static_cast<std::size_t>(linkIndex), op, rng);
    }
    case MorphogenesisKind::Bind: {
      const int linkIndex = findLinkIndexByEdge(child, step.linkParentId, step.linkChildId);
      if (linkIndex < 0) {
        return false;
      }
      const bool changed =
          applyStructuralOpLink(child, static_cast<std::size_t>(linkIndex), op, rng);
      child.stemAssembly = extractStemAssemblyPlan(child);
      return changed;
    }
  }
  return false;
}

StructuralOp drawStructuralOp(std::mt19937& rng) {
  const float roll = std::uniform_real_distribution<float>(0.0f, 1.0f)(rng);
  if (roll < 0.50f) {
    return StructuralOp::Deletion;
  }
  if (roll < 0.85f) {
    return StructuralOp::Duplication;
  }
  return StructuralOp::Insertion;
}

std::uint32_t structuralOpSurcharge(StructuralOp op) {
  switch (op) {
    case StructuralOp::Deletion:
      return kParthenogenesisDeletionSurcharge;
    case StructuralOp::Duplication:
      return kParthenogenesisDuplicationSurcharge;
    case StructuralOp::Insertion:
      return kParthenogenesisInsertionSurcharge;
  }
  return 0;
}

int morphogenesisStepCountCamp() {
  return 4 + static_cast<int>(kCampDevelopmentalAxonCount) +
         static_cast<int>(kWorldHubSocketCount);
}

int morphogenesisStepCount(const Organism& child) {
  return static_cast<int>(child.nodes.size() + child.stemAssembly.binds.size() +
                          child.neuralAxons.size() + child.links.size());
}

std::uint32_t pipelineBaseDebit() {
  return kParthenogenesisInitCost +
         static_cast<std::uint32_t>(morphogenesisStepCountCamp()) * kParthenogenesisStepBasalCost;
}

bool trySpawnPoseAtHeading(const Organism& parent, float heading, const BarrenWorld& world,
                           float cellSize, float heightScale, std::uint64_t simTick, float& wx,
                           float& wz, float& wy) {
  const float offset = cellSize * kParthenogenesisSpawnOffsetFactor;
  wx = parent.rootWorldX() + std::sin(heading) * offset;
  wz = parent.rootWorldZ() + std::cos(heading) * offset;
  if (!world.isWetWorld(wx, wz, cellSize)) {
    return false;
  }
  const WaterColumn column = sampleWaterColumn(world, wx, wz, cellSize, heightScale);
  std::mt19937 rng = chaosSpawnRng(simTick, static_cast<std::uint64_t>(parent.id) ^
                                                kChaosSaltParthenogenesis);
  wy = column.surfaceY + chaosJitterFloat(kSpawnSurfaceYOffset, rng);
  return true;
}

bool findSpawnPose(const Organism& parent, const BarrenWorld& world, float cellSize,
                   float heightScale, std::uint64_t simTick, float& wx, float& wz, float& wy) {
  if (trySpawnPoseAtHeading(parent, parent.heading, world, cellSize, heightScale, simTick, wx, wz,
                            wy)) {
    return true;
  }
  if (!parent.feedbagOracle) {
    return false;
  }
  constexpr int kHeadingProbes = 8;
  for (int probe = 1; probe < kHeadingProbes; ++probe) {
    const float heading =
        parent.heading + static_cast<float>(probe) * kTwoPi / static_cast<float>(kHeadingProbes);
    if (trySpawnPoseAtHeading(parent, heading, world, cellSize, heightScale, simTick, wx, wz, wy)) {
      return true;
    }
  }
  wx = parent.rootWorldX();
  wz = parent.rootWorldZ();
  wy = parent.rootWorldY();
  return true;
}

void abortSpend(Organism& parent, ParthenogenesisResult& result, std::uint32_t bytesSpent,
                bool refundSpent = false) {
  if (refundSpent) {
    creditHubBytes(parent, bytesSpent);
    bytesSpent = 0;
  }
  result.aborted = true;
  result.bytesSpent = bytesSpent;
  parent.lastParthenogenesisBytesSpent = bytesSpent;
  parent.lastParthenogenesisSpawned = false;
}

Organism buildCampChildShell(const Organism& parent, std::uint32_t childId, float wx, float wz,
                             float wy, std::uint64_t simTick, float cellSize) {
  (void)cellSize;
  return cloneParentStructure(parent, childId, wx, wz, wy, simTick);
}

void applyGate2Jitter(Organism& child, const Organism& parent, std::mt19937& rng) {
  for (NeuralAxon& axon : child.neuralAxons) {
    jitterAxonGate2(axon, rng);
  }
  jitterOrganismGate2(child, parent, rng);
  guardCampComputerRegister(child);
}

bool runMorphogenesisPipeline(Organism& child, Organism& parent, std::mt19937& rng,
                              float structuralRate, std::uint32_t& bytesSpent,
                              std::uint32_t& structuralExtra, ParthenogenesisResult& result,
                              Organism& abortParent, bool debitParent = true) {
  const std::vector<MorphogenesisStep> plan = buildMorphogenesisPlan(child);
  for (const MorphogenesisStep& step : plan) {
    if (debitParent) {
      if (!consumeHubBytes(abortParent, kParthenogenesisStepBasalCost, bytesSpent)) {
        abortSpend(abortParent, result, bytesSpent);
        return false;
      }
    }
    if (step.kind == MorphogenesisKind::Bind) {
      if (replayMorphogenesisBindStep(child, step, rng)) {
        ++result.stemBindStepsReplayed;
      }
    }
    if (chaosBernoulli(structuralRate, rng)) {
      const StructuralOp op = drawStructuralOp(rng);
      if (applyMorphogenesisStructuralOp(child, step, op, rng)) {
        structuralExtra += structuralOpSurcharge(op);
      }
    }
  }
  reconcileStemAssemblyLinks(child, child.heading, rng);
  child.stemAssembly = extractStemAssemblyPlan(child);
  applyGate2Jitter(child, parent, rng);
  return true;
}

void finalizeCampBirth(Organism& child, const BarrenWorld& world, float cellSize,
                       float heightScale) {
  initializeArticulatedSpawnPose(child, world, cellSize, heightScale, child.heading);
  child.landAdjacent =
      organismLandAdjacent(world, child.rootWorldX(), child.rootWorldZ(), cellSize);
}

}  // namespace

bool axonGraphLegal(const Organism& organism) {
  if (organism.neuralAxons.size() > kAxonChannelCapacity) {
    return false;
  }
  for (const NeuralAxon& axon : organism.neuralAxons) {
    if (axon.uncappedNodeId != 0) {
      continue;
    }
    const SkeletonNode* src = organism.findNode(axon.srcNodeId);
    const SkeletonNode* dst = organism.findNode(axon.dstNodeId);
    if (src == nullptr || dst == nullptr || !src->alive || !dst->alive) {
      return false;
    }
  }
  return true;
}

bool campGenotypeValid(const Organism& organism) {
  return organism.alive && organismHasCampNeuronFloor(organism) && axonGraphLegal(organism);
}

bool campSpawnMorphologyValid(const Organism& organism) {
  return campGenotypeValid(organism) && organismHasCampDevelopmentalAxons(organism) &&
         (organismHasCampTorpedoChain(organism) || organismHasCampHubArms(organism));
}

std::uint32_t estimateParthenogenesisCostCamp() {
  return kParthenogenesisBaselineCampDebit;
}

std::uint32_t estimateParthenogenesisRequiredHubBytes() {
  return kParthenogenesisBaselineCampDebit + kParthenogenesisParentReserveMin;
}

bool eligibleForParthenogenesis(const Organism& organism, const BarrenWorld& world, float cellSize,
                                std::uint64_t simTick) {
  if (!organism.alive || !organismHasCampNeuronFloor(organism)) {
    return false;
  }
  if (simTick < organism.createdAtTick +
                 (organism.feedbagOracle ? kFeedbagOracleParthenogenesisMinAgeTicks
                                        : kParthenogenesisMinAgeTicks)) {
    return false;
  }
  if (!campGenotypeValid(organism)) {
    return false;
  }
  if (parentHasBasalArrears(organism)) {
    return false;
  }
  if (organism.lastParthenogenesisSuccessTick != 0 &&
      simTick < organism.lastParthenogenesisSuccessTick + kParthenogenesisRefractoryTicks) {
    return false;
  }
  if (computerHubFuelBytes(organism) < estimateParthenogenesisRequiredHubBytes()) {
    return false;
  }
  if (!organism.feedbagOracle &&
      !world.isWetWorld(organism.rootWorldX(), organism.rootWorldZ(), cellSize)) {
    return false;
  }
  return true;
}

Organism cloneCampChildFromParent(const Organism& parent, std::uint32_t childId, float wx,
                                  float wz, float wy, std::uint64_t simTick, std::mt19937& rng,
                                  float structuralRate) {
  Organism child = cloneParentStructure(parent, childId, wx, wz, wy, simTick);
  Organism parentScratch = parent;
  std::uint32_t bytesSpent = 0;
  std::uint32_t structuralExtra = 0;
  ParthenogenesisResult scratch;
  runMorphogenesisPipeline(child, parentScratch, rng, structuralRate, bytesSpent, structuralExtra,
                           scratch, parentScratch, false);
  (void)bytesSpent;
  (void)structuralExtra;
  return child;
}

ParthenogenesisResult attemptParthenogenesis(Organism& parent, const BarrenWorld& world,
                                             float cellSize, float heightScale,
                                             std::uint64_t simTick, std::uint32_t& nextOrganismId,
                                             const ParthenogenesisPassOptions& options) {
  ParthenogenesisResult result;
  parent.lastParthenogenesisBytesSpent = 0;
  parent.lastParthenogenesisSpawned = false;

  if (!options.skipEligibilityChecks &&
      !eligibleForParthenogenesis(parent, world, cellSize, simTick)) {
    return result;
  }

  ParthenogenesisPassOptions runOptions = options;

  float spawnX = 0.0f;
  float spawnY = 0.0f;
  float spawnZ = 0.0f;
  if (!findSpawnPose(parent, world, cellSize, heightScale, simTick, spawnX, spawnZ, spawnY)) {
    return result;
  }

  std::mt19937 rng = chaosSpawnRng(simTick, static_cast<std::uint64_t>(parent.id) ^
                                                kChaosSaltParthenogenesis);

  std::uint32_t bytesSpent = 0;
  std::uint32_t structuralExtra = 0;
  const float structuralRate = effectiveStructuralRate(runOptions);

  if (!consumeHubBytes(parent, kParthenogenesisInitCost, bytesSpent)) {
    abortSpend(parent, result, bytesSpent);
    return result;
  }

  const std::uint32_t childId = nextOrganismId;
  Organism child = cloneParentStructure(parent, childId, spawnX, spawnZ, spawnY, simTick);

  if (!runMorphogenesisPipeline(child, parent, rng, structuralRate, bytesSpent, structuralExtra,
                                result, parent)) {
    return result;
  }

  const std::uint32_t pipelineDebit = pipelineBaseDebit();
  const std::uint32_t finalisationDebit =
      kParthenogenesisBaselineCampDebit > pipelineDebit
          ? kParthenogenesisBaselineCampDebit - pipelineDebit
          : 0;

  if (!campSpawnMorphologyValid(child)) {
    if (options.captureRejectedMorphology) {
      result.rejectedMorphology = child;
      result.rejectedMorphologyCaptured = true;
    }
    abortSpend(parent, result, bytesSpent, true);
    return result;
  }

  if (computerHubFuelBytes(parent) < finalisationDebit + structuralExtra +
                                         kParthenogenesisParentReserveMin) {
    abortSpend(parent, result, bytesSpent, true);
    return result;
  }

  if (!consumeHubBytes(parent, finalisationDebit + structuralExtra, bytesSpent)) {
    abortSpend(parent, result, bytesSpent, true);
    return result;
  }

  if (!canAffordReserveAfterSpend(parent, bytesSpent)) {
    abortSpend(parent, result, bytesSpent, true);
    return result;
  }

  nextOrganismId = childId + 1;
  child.alive = true;
  parent.lastParthenogenesisSpawned = true;
  parent.lastParthenogenesisBytesSpent = bytesSpent;
  parent.offspringSpawnedCount += 1;
  parent.lastParthenogenesisSuccessTick = simTick;
  parent.parthenogenesisCelebrationStartTick = simTick;
  parent.parthenogenesisBirthHeading = parent.heading;

  if (parent.feedbagOracle) {
    std::cout << "[parthenogenesis] feedbag oracle id=" << parent.id << " spawned child id="
              << childId << " at tick=" << simTick << " bytesSpent=" << bytesSpent << '\n';
  }

  result.spawned = true;
  result.aborted = false;
  result.bytesSpent = bytesSpent;
  result.childId = childId;
  result.child = std::move(child);
  return result;
}

void tickParthenogenesisPass(std::vector<Organism>& population, const BarrenWorld& world,
                             float cellSize, float heightScale, std::uint64_t simTick,
                             std::uint32_t& nextOrganismId,
                             const ParthenogenesisPassOptions& options) {
  std::vector<Organism> spawned;
  spawned.reserve(8);

  for (Organism& organism : population) {
    if (!organism.alive) {
      continue;
    }
    ParthenogenesisResult result = attemptParthenogenesis(
        organism, world, cellSize, heightScale, simTick, nextOrganismId, options);
    if (result.spawned) {
      finalizeCampBirth(result.child, world, cellSize, heightScale);
      spawned.push_back(std::move(result.child));
    }
  }

  for (Organism& child : spawned) {
    population.push_back(std::move(child));
  }
}

}  // namespace evolab
