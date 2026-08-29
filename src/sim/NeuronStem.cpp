#include "sim/NeuronStem.hpp"

#include "sim/CampTopology.hpp"
#include "sim/CellConstants.hpp"
#include "sim/Chaos.hpp"
#include "sim/EnergonString.hpp"
#include "sim/NeuronFuel.hpp"
#include "sim/NeuronSignal.hpp"
#include "sim/OrganismNeuron.hpp"
#include "sim/WorldConstants.hpp"

#include <algorithm>
#include <random>

namespace evolab {

SkeletonNode* findNeuronNode(Organism& organism, NeuronType type, bool requireAlive) {
  for (SkeletonNode& node : organism.nodes) {
    if (node.neuron != type) {
      continue;
    }
    if (requireAlive && !node.alive) {
      continue;
    }
    return &node;
  }
  return nullptr;
}

const SkeletonNode* findNeuronNode(const Organism& organism, NeuronType type,
                                   bool requireAlive) {
  return findNeuronNode(const_cast<Organism&>(organism), type, requireAlive);
}

std::vector<std::uint8_t>* neuronFuelPool(Organism& organism, SkeletonNode& node) {
  if (!node.alive) {
    return nullptr;
  }
  if (node.neuron == NeuronType::Computer) {
    return &organism.bodyStorage;
  }
  if (node.neuron == NeuronType::Actuator && organism.actuatorCount() == 1 &&
      !organism.hasMouthNeurons()) {
    return &organism.bodyStorage;
  }
  if (node.neuron == NeuronType::None && node.id == organism.rootNodeId) {
    return &organism.bodyStorage;
  }
  if (node.neuron == NeuronType::None) {
    return nullptr;
  }
  return &node.store;
}

const std::vector<std::uint8_t>* neuronFuelPool(const Organism& organism,
                                                const SkeletonNode& node) {
  return neuronFuelPool(const_cast<Organism&>(organism), const_cast<SkeletonNode&>(node));
}

void consumeFuelBack(std::vector<std::uint8_t>& storage, std::size_t count) {
  if (count == 0 || storage.empty()) {
    return;
  }
  const std::size_t removeCount = std::min(storage.size(), count);
  storage.erase(storage.end() - static_cast<std::ptrdiff_t>(removeCount), storage.end());
}

bool tryPayNeuronBasalCost(Organism& organism, SkeletonNode& node) {
  std::vector<std::uint8_t>* pool = neuronFuelPool(organism, node);
  if (pool == nullptr) {
    return true;
  }
  if (pool->size() >= kStemCellBasalCostPerTick) {
    if (pool == &node.store) {
      neuronConsumeBack(node, kStemCellBasalCostPerTick);
    } else {
      consumeFuelBack(*pool, kStemCellBasalCostPerTick);
    }
    return true;
  }
  if (organism.feedbagOracle && !organism.bodyStorage.empty()) {
    consumeFuelBack(organism.bodyStorage, kStemCellBasalCostPerTick);
    return true;
  }
  if (node.neuron == NeuronType::Mouth && !organism.isCampNom() &&
      !organism.bodyStorage.empty()) {
    consumeFuelBack(organism.bodyStorage, kStemCellBasalCostPerTick);
    return true;
  }
  return false;
}

void expelByteAtNode(const SkeletonNode& node, EnergonField& field, std::uint8_t byte,
                     EnergonOrigin origin, float ttlScale, float zOffsetFactor) {
  EnergonBlob fragment;
  fragment.data = byte;
  fragment.remaining = 1;
  fragment.initialBytes = 1;
  fragment.origin = origin;
  fragment.x = node.worldX;
  fragment.z = node.worldZ + kWorldCellSize * zOffsetFactor;
  fragment.y = node.worldY;
  fragment.grounded = true;
  fragment.onWet = true;
  energonAssignGroundedTtl(fragment, field.config(), true, ttlScale);
  energonBlobInitPoint(fragment);
  field.injectBlob(fragment);
}

void releaseFuelAtNode(const SkeletonNode& node, EnergonField& field,
                       std::vector<std::uint8_t>& storage, EnergonOrigin origin,
                       float ttlScale) {
  const float zOffset =
      origin == EnergonOrigin::Fragment ? kMouthContactRadiusFactor * 0.35f : 0.0f;
  std::mt19937 rng(static_cast<std::uint32_t>(node.id * 2246822519u ^
                                              static_cast<std::uint32_t>(storage.size())));

  while (!storage.empty()) {
    const int chunk = std::min(static_cast<int>(storage.size()), kEnergonMaxBytesPerBlob);
    EnergonBlob blob;
    blob.data = energonPackRawBytes(storage.data(), chunk);
    blob.remaining = static_cast<std::uint16_t>(chunk);
    blob.initialBytes = static_cast<std::uint8_t>(chunk);
    blob.origin = origin;
    blob.x = node.worldX;
    blob.z = node.worldZ + kWorldCellSize * zOffset;
    blob.y = node.worldY;
    blob.grounded = true;
    blob.onWet = true;
    energonAssignGroundedTtl(blob, field.config(), true, ttlScale);

    if (chunk > 1) {
      std::uniform_real_distribution<float> headingDist(0.0f, kTwoPi);
      energonBlobLayoutSegment(blob, kWorldCellSize, headingDist(rng));
    } else {
      energonBlobInitPoint(blob);
    }

    field.injectBlob(blob);
    storage.erase(storage.begin(), storage.begin() + chunk);
  }
}

void emitCampPreAdvectSignals(Organism& organism, std::uint64_t simTick) {
  if (!organismUsesCampNeuronPhases(organism)) {
    return;
  }

  static constexpr NeuronType kMouthAllowedDst[] = {NeuronType::Perceptor, NeuronType::Actuator,
                                                   NeuronType::Computer};
  for (const SkeletonNode& node : organism.nodes) {
    if (!node.alive || node.neuron != NeuronType::Mouth) {
      continue;
    }
    emitOutboundConfidence(organism, node.id, mouthOutboundConfidence(node), simTick,
                           kMouthAllowedDst, std::size(kMouthAllowedDst));
  }

  const SkeletonNode* computer = findNeuronNode(organism, NeuronType::Computer);
  if (computer == nullptr) {
    return;
  }
  static constexpr NeuronType kComputerAllowedDst[] = {NeuronType::Perceptor, NeuronType::Actuator,
                                                       NeuronType::Mouth};
  emitOutboundConfidence(organism, computer->id,
                         hubFuelConfidence(organism.bodyStorage.size()), simTick,
                         kComputerAllowedDst, std::size(kComputerAllowedDst));
}

void emitCampActuatorSignals(Organism& organism, std::uint64_t simTick) {
  if (!organismUsesCampNeuronPhases(organism)) {
    return;
  }

  const SkeletonNode* actuator = findNeuronNode(organism, NeuronType::Actuator);
  if (actuator == nullptr) {
    return;
  }

  const std::uint8_t confidence =
      actuatorActivityConfidence(organism.lastStrokePaid, organism.lastStrokeBytesPaid);
  organism.lastActuatorOutboundSignal = confidence;

  static constexpr NeuronType kAllowedDst[] = {NeuronType::Mouth, NeuronType::Perceptor};
  emitOutboundConfidence(organism, actuator->id, confidence, simTick, kAllowedDst,
                         std::size(kAllowedDst));
}

}  // namespace evolab
