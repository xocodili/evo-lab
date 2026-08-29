#include "sim/OrganismParthenogenesis.hpp"
#include "sim/OrganismComputer.hpp"
#include "sim/OrganismFeedbagOracle.hpp"

#include "sim/BarrenWorld.hpp"
#include "sim/CampTopology.hpp"
#include "sim/CellConstants.hpp"
#include "sim/Chaos.hpp"
#include "sim/NeuralAxon.hpp"
#include "sim/NeuronStem.hpp"
#include "sim/WaterColumn.hpp"
#include "sim/WorldConstants.hpp"

#include <algorithm>
#include <cmath>
#include <random>
#include <vector>

namespace evolab {

namespace {

enum class StructuralOp : std::uint8_t { Duplication, Deletion, Insertion };

enum class MorphogenesisKind : std::uint8_t { Locus, Axon, Link };

struct MorphogenesisStep {
  MorphogenesisKind kind = MorphogenesisKind::Locus;
  std::uint32_t nodeId = 0;
  std::size_t index = 0;
};

void splitCampEndowment(std::size_t total, std::size_t& hubBytes, std::size_t& perceptorBytes,
                        std::size_t& mouthBytes, std::size_t& actuatorBytes) {
  hubBytes = total / 2;
  const std::size_t peripheral = total - hubBytes;
  perceptorBytes = peripheral / 3;
  mouthBytes = peripheral / 3;
  actuatorBytes = peripheral - perceptorBytes - mouthBytes;
}

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
  if (organism.bodyStorage.size() < amount) {
    bytesSpent += static_cast<std::uint32_t>(organism.bodyStorage.size());
    organism.bodyStorage.clear();
    return false;
  }
  consumeFuelBack(organism.bodyStorage, amount);
  bytesSpent += amount;
  return true;
}

bool canAffordReserveAfterSpend(const Organism& organism, std::uint32_t spent) {
  const std::size_t remaining = organism.bodyStorage.size();
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
  organism.heading = chaosJitterHeading(parent.heading, rng);
  for (SkeletonNode& node : organism.nodes) {
    if (node.neuron != NeuronType::Computer) {
      continue;
    }
    const SkeletonNode* parentNode = parent.findNode(node.id);
    if (parentNode != nullptr && parentNode->neuron == NeuronType::Computer) {
      node.computerRegister = parentNode->computerRegister;
    } else {
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
    link.jointAngle = chaosJitterFloat(parentLink->jointAngle, rng);
    link.energyEta = chaosJitterFloat(parentLink->energyEta, rng);
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
  std::size_t hubBytes = 0;
  std::size_t perceptorBytes = 0;
  std::size_t mouthBytes = 0;
  std::size_t actuatorBytes = 0;
  splitCampEndowment(kParthenogenesisChildEndowmentBytes, hubBytes, perceptorBytes, mouthBytes,
                     actuatorBytes);

  int perceptorCount = 0;
  int mouthCount = 0;
  int actuatorCount = 0;
  for (const SkeletonNode& node : child.nodes) {
    if (!node.alive) {
      continue;
    }
    switch (node.neuron) {
      case NeuronType::Perceptor:
        ++perceptorCount;
        break;
      case NeuronType::Mouth:
        ++mouthCount;
        break;
      case NeuronType::Actuator:
        ++actuatorCount;
        break;
      default:
        break;
    }
  }

  child.bodyStorage.assign(hubBytes + mouthBytes, 0);
  for (SkeletonNode& node : child.nodes) {
    if (!node.alive) {
      continue;
    }
    if (node.neuron == NeuronType::Perceptor) {
      const std::size_t share =
          perceptorBytes / static_cast<std::size_t>(std::max(1, perceptorCount));
      node.store.assign(share, 0);
    } else if (node.neuron == NeuronType::Mouth) {
      node.store.clear();
    } else if (node.neuron == NeuronType::Actuator) {
      const std::size_t share = actuatorBytes / static_cast<std::size_t>(std::max(1, actuatorCount));
      node.store.assign(share, 0);
    }
  }
}

Organism cloneParentStructure(const Organism& parent, std::uint32_t childId, float wx, float wz,
                              float wy, std::uint64_t simTick) {
  Organism child;
  child.id = childId;
  child.createdAtTick = simTick;
  child.alive = true;
  child.rootNodeId = parent.rootNodeId;
  child.computerNodeId = parent.computerNodeId;
  child.heading = parent.heading;
  child.senseRadiusFactor = parent.senseRadiusFactor;
  child.nodes = parent.nodes;
  child.links = parent.links;
  child.neuralAxons = parent.neuralAxons;

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
  plan.reserve(child.nodes.size() + child.neuralAxons.size() + child.links.size());
  for (const SkeletonNode& node : child.nodes) {
    plan.push_back({MorphogenesisKind::Locus, node.id, 0});
  }
  for (std::size_t i = 0; i < child.neuralAxons.size(); ++i) {
    plan.push_back({MorphogenesisKind::Axon, 0, i});
  }
  for (std::size_t i = 0; i < child.links.size(); ++i) {
    plan.push_back({MorphogenesisKind::Link, 0, i});
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
          SkeletonLink arm;
          arm.parentNodeId = root->id;
          arm.childNodeId = newId;
          arm.restLength = child.links.empty() ? nominalBoneLength(kWorldCellSize)
                                               : child.links.front().restLength;
          arm.jointAngle = std::uniform_real_distribution<float>(-kTwoPi, kTwoPi)(rng);
          arm.energyEta = 0.0f;
          arm.muscleBundle = true;
          addedLinks.push_back(arm);
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
        if (const SkeletonNode* replacement = findComputerRoot(child)) {
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
      if (inserted.neuron == NeuronType::Computer) {
        initComputerNodeRegister(inserted);
      }
      child.nodes.insert(child.nodes.begin() + static_cast<std::ptrdiff_t>(index + 1), inserted);

      if (const SkeletonNode* root = findComputerRoot(child)) {
        SkeletonLink arm;
        arm.parentNodeId = root->id;
        arm.childNodeId = newId;
        arm.restLength = child.links.empty() ? nominalBoneLength(kWorldCellSize)
                                             : child.links.front().restLength;
        arm.jointAngle = std::uniform_real_distribution<float>(-kTwoPi, kTwoPi)(rng);
        arm.energyEta = 0.0f;
        arm.muscleBundle = true;
        child.links.push_back(arm);
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
  for (const auto& edge : kCampDevelopmentalAxons) {
    if (edge.first == src && edge.second == dst) {
      return true;
    }
  }
  return false;
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
    case MorphogenesisKind::Axon:
      if (step.index >= child.neuralAxons.size()) {
        return false;
      }
      return applyStructuralOpAxon(child.neuralAxons, op, rng, step.index);
    case MorphogenesisKind::Link:
      if (step.index >= child.links.size()) {
        return false;
      }
      return applyStructuralOpLink(child, step.index, op, rng);
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
  return 4 + static_cast<int>(kCampDevelopmentalAxonCount) + 3;
}

int morphogenesisStepCount(const Organism& child) {
  return static_cast<int>(child.nodes.size() + child.neuralAxons.size() + child.links.size());
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

void abortSpend(Organism& parent, ParthenogenesisResult& result, std::uint32_t bytesSpent) {
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
    if (chaosBernoulli(structuralRate, rng)) {
      const StructuralOp op = drawStructuralOp(rng);
      if (applyMorphogenesisStructuralOp(child, step, op, rng)) {
        structuralExtra += structuralOpSurcharge(op);
      }
    }
  }
  applyGate2Jitter(child, parent, rng);
  return true;
}

void finalizeCampBirth(Organism& child, const BarrenWorld& world, float cellSize,
                       float heightScale) {
  child.updateKinematics(world, cellSize, heightScale);
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
  if (organism.bodyStorage.size() < estimateParthenogenesisRequiredHubBytes()) {
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
  const float structuralRate = effectiveStructuralRate(options);

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

  if (parent.bodyStorage.size() < finalisationDebit + structuralExtra +
                                         kParthenogenesisParentReserveMin) {
    abortSpend(parent, result, bytesSpent);
    return result;
  }

  if (!campGenotypeValid(child)) {
    abortSpend(parent, result, bytesSpent);
    return result;
  }

  if (!consumeHubBytes(parent, finalisationDebit + structuralExtra, bytesSpent)) {
    abortSpend(parent, result, bytesSpent);
    return result;
  }

  if (!canAffordReserveAfterSpend(parent, bytesSpent)) {
    abortSpend(parent, result, bytesSpent);
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
