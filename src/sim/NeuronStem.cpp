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
  if (node.neuron == NeuronType::None && node.id != organism.rootNodeId) {
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
  if (pool != nullptr && organism.isCampNom() && node.neuron != NeuronType::None &&
      pool->size() >= kStemCellBasalCostPerTick) {
    std::mt19937 rng(static_cast<std::uint32_t>(node.id * 2654435761u ^
                                                static_cast<std::uint32_t>(
                                                    organism.createdAtTick + node.basalArrearsTicks)));
    float payProb = 1.0f;
    if (node.coordinatorDutyScale < kCoordinatorMaxDutyScale - 1.0e-4f) {
      payProb = std::min(payProb, clamp01(node.coordinatorDutyScale));
    }
    if (organism.famineUnit > kCoordinatorFamineBasalSkipThreshold) {
      payProb = std::min(payProb,
                         clamp01(1.0f - organism.famineUnit * kCoordinatorFamineBasalSkipGain));
    }
    if (payProb < 1.0f - 1.0e-4f && !chaosBernoulli(payProb, rng)) {
      return true;
    }
  }
  if (pool == nullptr) {
    return true;
  }
  if (pool->size() >= kStemCellBasalCostPerTick) {
    neuronConsumeBack(node, kStemCellBasalCostPerTick);
    return true;
  }
  if (organism.isCampNom() && node.neuron != NeuronType::Computer &&
      node.neuron != NeuronType::None) {
    const std::size_t hubBytes = computerHubFuelBytes(organism);
    const std::size_t hubVitalFloor =
        kComputerHubReserveBytes + kComputerHubConservationSlackBytes + kStemCellBasalCostPerTick;
    if (hubBytes >= hubVitalFloor && hubStoreConsumeBack(organism, kStemCellBasalCostPerTick)) {
      return true;
    }
  }
  if (organism.feedbagOracle) {
    SkeletonNode* root = organism.findNode(organism.rootNodeId);
    if (root != nullptr && !root->store.empty()) {
      neuronConsumeBack(*root, kStemCellBasalCostPerTick);
      return true;
    }
  }
  if (node.neuron == NeuronType::Mouth && !organism.isCampNom()) {
    const std::size_t hubBytes = computerHubFuelBytes(organism);
    if (hubBytes >= kStemCellBasalCostPerTick) {
      hubStoreConsumeBack(organism, kStemCellBasalCostPerTick);
      return true;
    }
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
  for (SkeletonNode& node : organism.nodes) {
    if (!node.alive || node.neuron != NeuronType::Mouth) {
      continue;
    }
    for (NeuralAxon& axon : organism.neuralAxons) {
      if (axon.srcNodeId != node.id) {
        continue;
      }
      const SkeletonNode* dst = organism.findNode(axon.dstNodeId);
      if (dst == nullptr || !dst->alive || axon.uncappedNodeId == axon.dstNodeId ||
          axon.uncappedNodeId == axon.srcNodeId) {
        continue;
      }
      bool allowed = false;
      for (NeuronType allowedDst : kMouthAllowedDst) {
        if (dst->neuron == allowedDst) {
          allowed = true;
          break;
        }
      }
      if (!allowed) {
        continue;
      }
      const std::uint8_t confidence = mouthOutboundConfidenceForDst(node, dst->neuron);
      if (!isNeuronConfidenceByte(confidence)) {
        continue;
      }
      writeAxonConfidence(axon, confidence, simTick);
      node.lastEmittedByte = confidence;
    }
  }

  const SkeletonNode* computer = findNeuronNode(organism, NeuronType::Computer);
  if (computer == nullptr) {
    return;
  }
  static constexpr NeuronType kComputerAllowedDst[] = {NeuronType::Perceptor, NeuronType::Actuator,
                                                       NeuronType::Mouth};
  emitOutboundConfidence(organism, computer->id,
                         hubFuelConfidence(computerHubFuelBytes(organism)), simTick,
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

namespace {

std::size_t stemNodeEquilibriumReserve(const SkeletonNode& node) {
  if (node.neuron == NeuronType::Computer) {
    return kComputerHubReserveBytes;
  }
  if (node.neuron == NeuronType::Mouth) {
    return kMouthConveyReserveBytes;
  }
  return 0;
}

std::size_t stemNodeEquilibriumSlack(const SkeletonNode& node) {
  if (node.neuron == NeuronType::Computer) {
    return kComputerHubConservationSlackBytes;
  }
  return 0;
}

}  // namespace

bool campMouthAteThisTick(const Organism& organism) {
  for (const SkeletonNode& mouth : organism.nodes) {
    if (mouth.alive && mouth.neuron == NeuronType::Mouth && mouth.ateThisTick) {
      return true;
    }
  }
  return false;
}

float stemEquilibriumExportScale(const StemEquilibriumParams& params) {
  if (params.cap == 0) {
    return 0.0f;
  }
  const float fillUnit =
      clamp01(static_cast<float>(params.currentBytes) / static_cast<float>(params.cap));

  if (params.currentBytes <= params.reserveBytes + params.slackBytes) {
    return 0.0f;
  }

  if (params.priorBytes > 0 &&
      params.currentBytes + params.drainToleranceBytes < params.priorBytes) {
    return 0.0f;
  }

  const float reserveUnit =
      clamp01(static_cast<float>(params.reserveBytes + params.slackBytes) /
              static_cast<float>(params.cap));
  const float knee = clamp01(params.exportStartUnit);
  const float full = std::max(knee + 1.0e-4f, params.exportFullUnit);
  const float minScale = kStemEquilibriumMinExportScale;

  if (fillUnit <= knee) {
    if (knee <= reserveUnit + 1.0e-4f) {
      return minScale;
    }
    const float t = clamp01((fillUnit - reserveUnit) / (knee - reserveUnit));
    return minScale + t * minScale;
  }
  if (fillUnit >= full) {
    return 1.0f;
  }
  const float t = clamp01((fillUnit - knee) / (full - knee));
  return minScale * 2.0f + t * (1.0f - minScale * 2.0f);
}

float stemHubDispatchExportScale(const StemEquilibriumParams& params) {
  if (params.cap == 0) {
    return 0.0f;
  }

  if (params.currentBytes <= params.reserveBytes + params.slackBytes) {
    return 0.0f;
  }

  if (params.priorBytes > 0 &&
      params.currentBytes + params.drainToleranceBytes < params.priorBytes) {
    return 0.0f;
  }

  // Operational dispatch uses the same ramp as peripheral nodes (minimum floor below knee).
  // Bite-tick hub silence is enforced in stemNodeEquilibriumExportScale for Computer nodes.
  return stemEquilibriumExportScale(params);
}

float stemNodeEquilibriumExportScale(const Organism& organism, const SkeletonNode& node) {
  if (!node.alive || node.neuron == NeuronType::None) {
    return 0.0f;
  }

  StemEquilibriumParams params;
  params.currentBytes = node.store.size();
  params.priorBytes = node.storeBytesPriorTick;
  params.cap = nodeStoreNominalCap(organism, node);
  params.reserveBytes = stemNodeEquilibriumReserve(node);
  params.slackBytes = stemNodeEquilibriumSlack(node);
  params.drainToleranceBytes = kStemEquilibriumDrainToleranceBytes;
  params.exportStartUnit = organism.equilibriumExportStartUnit;
  params.exportFullUnit = confidenceToUnit(kComputerSatiationConfidence);

  if (node.neuron == NeuronType::Computer) {
    if (campMouthAteThisTick(organism)) {
      return 0.0f;
    }
    return stemHubDispatchExportScale(params);
  }

  return stemEquilibriumExportScale(params);
}

void refreshCampEquilibriumExportScales(Organism& organism) {
  if (!organismUsesCampNeuronPhases(organism)) {
    return;
  }

  const SkeletonNode* hubComputer = findComputerHubNode(organism);
  organism.hubConservationExportScale =
      hubComputer != nullptr ? stemNodeEquilibriumExportScale(organism, *hubComputer) : 0.0f;

  for (SkeletonNode& node : organism.nodes) {
    if (!node.alive || node.neuron == NeuronType::None) {
      continue;
    }
    node.equilibriumExportScale = stemNodeEquilibriumExportScale(organism, node);
  }
}

}  // namespace evolab
