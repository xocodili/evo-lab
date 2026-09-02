#include "sim/NeuronStem.hpp"

#include "sim/CampTopology.hpp"
#include "sim/CellConstants.hpp"
#include "sim/Chaos.hpp"
#include "sim/EnergonString.hpp"
#include "sim/NeuronFuel.hpp"
#include "sim/NeuronSignal.hpp"
#include "sim/OrganismNeuron.hpp"
#include "sim/PerceptorFocus.hpp"
#include "sim/WorldConstants.hpp"

#include "engine/kinematics/Math.hpp"

#include <algorithm>
#include <cmath>
#include <random>
#include <cstdint>

namespace evolab {

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

bool campCoordinatorSkipsBasalPayment(const Organism& organism, const SkeletonNode& node) {
  if (!organism.isCampNom() || node.neuron == NeuronType::None) {
    return false;
  }
  std::vector<std::uint8_t>* pool = neuronFuelPool(const_cast<Organism&>(organism),
                                                   const_cast<SkeletonNode&>(node));
  if (pool == nullptr || pool->size() < kStemCellBasalCostPerTick) {
    return false;
  }

  std::mt19937 rng(static_cast<std::uint32_t>(node.id * 2654435761u ^
                                              static_cast<std::uint32_t>(
                                                  organism.createdAtTick + node.basalArrearsTicks)));
  float payProb = 1.0f;
  if (node.coordinatorDutyScale < kCoordinatorMaxDutyScale - 1.0e-4f) {
    payProb = std::min(payProb, clamp01(node.coordinatorDutyScale));
  }
  if (organism.famineUnit > kCoordinatorFamineBasalSkipThreshold) {
    payProb =
        std::min(payProb, clamp01(1.0f - organism.famineUnit * kCoordinatorFamineBasalSkipGain));
  }
  return payProb < 1.0f - 1.0e-4f && !chaosBernoulli(payProb, rng);
}

void assignStemSurplusExportScales(Organism& organism) {
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

float effectiveStemBindRate(const StemBindAttempt& attempt) {
  if (attempt.bindRateOverride >= 0.0f && attempt.bindRateOverride <= 1.0f) {
    return attempt.bindRateOverride;
  }
  return kStemBindRateDefault;
}

bool tryPayStemBindCost(Organism& organism, std::uint32_t cost) {
  if (cost == 0) {
    return true;
  }
  SkeletonNode* hub = findComputerHubNode(organism);
  if (hub == nullptr || hub->store.size() < cost) {
    return false;
  }
  consumeFuelBack(hub->store, cost);
  return true;
}

}  // namespace

CampStorageSplit splitCampStorage(std::size_t totalBytes) {
  CampStorageSplit split;
  split.hubBytes = totalBytes / 2;
  const std::size_t peripheral = totalBytes - split.hubBytes;
  split.perceptorBytes = peripheral / 3;
  split.mouthBytes = peripheral / 3;
  split.actuatorBytes = peripheral - split.perceptorBytes - split.mouthBytes;
  return split;
}

void endowCampNodesFromSplit(Organism& organism, const CampStorageSplit& split,
                             EndowCampOptions options) {
  int perceptorCount = 0;
  int actuatorCount = 0;
  for (const SkeletonNode& node : organism.nodes) {
    if (!node.alive) {
      continue;
    }
    if (node.neuron == NeuronType::Perceptor) {
      ++perceptorCount;
    } else if (node.neuron == NeuronType::Actuator) {
      ++actuatorCount;
    }
  }
  const std::size_t perceptorShare =
      split.perceptorBytes / static_cast<std::size_t>(std::max(1, perceptorCount));
  const std::size_t actuatorShare =
      split.actuatorBytes / static_cast<std::size_t>(std::max(1, actuatorCount));
  const std::size_t hubEndowment = split.hubBytes + split.mouthBytes;

  for (SkeletonNode& node : organism.nodes) {
    if (!node.alive) {
      continue;
    }
    if (node.neuron == NeuronType::Computer) {
      if (options.clampToWalletCaps) {
        initComputerHubStore(node, hubEndowment, organism);
      } else {
        node.store.assign(hubEndowment, 0);
      }
    } else if (node.neuron == NeuronType::Perceptor) {
      if (options.clampToWalletCaps) {
        initPeripheralNodeStore(node, perceptorShare, organism);
      } else {
        node.store.assign(perceptorShare, 0);
      }
    } else if (node.neuron == NeuronType::Mouth) {
      node.store.clear();
    } else if (node.neuron == NeuronType::Actuator) {
      if (options.clampToWalletCaps) {
        initPeripheralNodeStore(node, actuatorShare, organism);
      } else {
        node.store.assign(actuatorShare, 0);
      }
    }
  }
}

void endowCampNodes(Organism& organism, std::size_t storageBytes) {
  endowCampNodesFromSplit(organism, splitCampStorage(storageBytes));
}

std::uint32_t campLocomotionFuelBytes(const Organism& organism) {
  std::size_t total = 0;
  if (const SkeletonNode* actuator = findNeuronNode(organism, NeuronType::Actuator, true)) {
    total += actuator->store.size();
  }
  const std::size_t hubAvail = computerHubFuelBytes(organism);
  const std::size_t vitalFloor = stemHubVitalSpendFloorBytes();
  if (hubAvail > vitalFloor) {
    total += hubAvail - vitalFloor;
  }
  return static_cast<std::uint32_t>(std::min(total, static_cast<std::size_t>(UINT32_MAX)));
}

namespace {

bool campBodyDistress(const Organism& organism) {
  if (computerHubFuelBytes(organism) < kComputerHubReserveBytes) {
    return true;
  }
  for (const SkeletonNode& node : organism.nodes) {
    if (node.alive && node.basalArrearsTicks > 0) {
      return true;
    }
  }
  return hubFuelConfidence(computerHubFuelBytes(organism)) < 3u;
}

bool campBodyMateReady(const Organism& organism, std::uint64_t simTick) {
  if (!organism.isCampNom() || !organism.alive) {
    return false;
  }
  if (hubFuelConfidence(computerHubFuelBytes(organism)) < kComputerSatiationConfidence) {
    return false;
  }
  const SkeletonNode* mouth = findNeuronNode(organism, NeuronType::Mouth);
  if (mouth == nullptr ||
      mouth->store.size() < static_cast<std::size_t>(kMouthInhibitActuatorConfidence)) {
    return false;
  }
  const SkeletonNode* perceptor = findNeuronNode(organism, NeuronType::Perceptor);
  const SkeletonNode* actuator = findNeuronNode(organism, NeuronType::Actuator);
  if (perceptor == nullptr || actuator == nullptr) {
    return false;
  }
  if (perceptor->store.size() <
      static_cast<std::size_t>(kPerceptorScanCostPerTick + kPerceptorTransductionCostPerTick)) {
    return false;
  }
  if (campLocomotionFuelBytes(organism) < kActuatorStrokeCostPerTick) {
    return false;
  }
  if (organism.lastPerceptFocusKind == PerceptFocusKind::Threat &&
      organism.lastPerceptConfidence < kNeuronConfidenceNeutral) {
    return false;
  }
  const std::uint64_t age =
      simTick > organism.createdAtTick ? simTick - organism.createdAtTick : 0u;
  if (age < kMateMinAgeTicks) {
    return false;
  }
  return computerHubFuelBytes(organism) >= kComputerHubReserveBytes + kCloacaVentCostMate;
}

}  // namespace

void gatherCampBodyInteroception(const Organism& organism, std::uint64_t simTick,
                                 CampBodyInteroception& out) {
  out.distress = campBodyDistress(organism);
  out.mateReady = campBodyMateReady(organism, simTick);
}

bool stemGlueMatches(std::uint8_t glueA, std::uint8_t glueB) {
  return glueA == kStemGlueLabel && glueB == kStemGlueLabel;
}

std::uint8_t inferStemHubSlotFromAngle(float heading, float jointAngle) {
  using engine::kinematics::normalizeAngle;
  float bestDelta = 1e9f;
  std::uint8_t bestSlot = 0;
  for (std::uint8_t slot = 0; slot < kWorldHubSocketCount; ++slot) {
    const float expected = hubSocketAngleRad(heading, slot);
    const float delta = std::fabs(normalizeAngle(jointAngle - expected));
    if (delta < bestDelta) {
      bestDelta = delta;
      bestSlot = slot;
    }
  }
  return bestSlot;
}

bool hubSlotTaken(const Organism& organism, std::uint32_t hubNodeId, std::uint8_t slot) {
  for (const SkeletonLink& link : organism.links) {
    if (link.parentNodeId != hubNodeId) {
      continue;
    }
    if (inferStemHubSlotFromAngle(organism.heading, link.jointAngle) == slot) {
      return true;
    }
  }
  return false;
}

int nextFreeHubSlot(const Organism& organism, std::uint32_t hubNodeId) {
  for (std::uint8_t slot = 0; slot < kWorldHubSocketCount; ++slot) {
    if (!hubSlotTaken(organism, hubNodeId, slot)) {
      return static_cast<int>(slot);
    }
  }
  return -1;
}

StemBindResult tryStemBindPeripheralToHub(Organism& organism, std::uint32_t hubNodeId,
                                          std::uint32_t peripheralNodeId, std::uint8_t hubSlot,
                                          float restLength, float heading,
                                          const StemBindAttempt& attempt, std::mt19937& rng,
                                          Organism* payer) {
  StemBindResult result;
  if (!stemGlueMatches(kStemGlueLabel, kStemGlueLabel)) {
    return result;
  }
  if (hubSlot >= kWorldHubSocketCount || hubSlotTaken(organism, hubNodeId, hubSlot)) {
    return result;
  }
  const SkeletonNode* hub = organism.findNode(hubNodeId);
  const SkeletonNode* peripheral = organism.findNode(peripheralNodeId);
  if (hub == nullptr || peripheral == nullptr || !hub->alive || !peripheral->alive) {
    return result;
  }
  if (hub->id == peripheral->id) {
    return result;
  }
  for (const SkeletonLink& link : organism.links) {
    if (link.parentNodeId == hubNodeId && link.childNodeId == peripheralNodeId) {
      return result;
    }
  }

  if (attempt.requireEntropy && !chaosBernoulli(effectiveStemBindRate(attempt), rng)) {
    return result;
  }
  if (attempt.requirePayment && payer != nullptr &&
      !tryPayStemBindCost(*payer, attempt.bindCostBytes)) {
    return result;
  }

  SkeletonLink link;
  link.parentNodeId = hubNodeId;
  link.childNodeId = peripheralNodeId;
  link.restLength = restLength;
  link.jointAngle = hubSocketAngleRad(heading, hubSlot);
  link.energyEta = 0.0f;
  link.muscleBundle = true;
  organism.links.push_back(link);

  StemBindRecord record;
  record.hubNodeId = hubNodeId;
  record.peripheralNodeId = peripheralNodeId;
  record.hubSlot = hubSlot;
  record.peripheralFace = static_cast<std::uint8_t>(StemFace::North);
  record.restLength = restLength;
  record.muscleBundle = true;
  organism.stemAssembly.binds.push_back(record);

  result.ok = true;
  result.record = record;
  return result;
}

StemChainBindResult tryStemBindChainSegment(Organism& organism, std::uint32_t parentNodeId,
                                            std::uint32_t childNodeId, float segmentAngleOffset,
                                            float restLength, float heading,
                                            const StemBindAttempt& attempt, std::mt19937& rng,
                                            Organism* payer) {
  StemChainBindResult result;
  if (!stemGlueMatches(kStemGlueLabel, kStemGlueLabel)) {
    return result;
  }
  const SkeletonNode* parent = organism.findNode(parentNodeId);
  const SkeletonNode* child = organism.findNode(childNodeId);
  if (parent == nullptr || child == nullptr || !parent->alive || !child->alive) {
    return result;
  }
  if (parent->id == child->id) {
    return result;
  }
  for (const SkeletonLink& link : organism.links) {
    if (link.parentNodeId == parentNodeId && link.childNodeId == childNodeId) {
      result.ok = true;
      return result;
    }
  }

  if (attempt.requireEntropy && !chaosBernoulli(effectiveStemBindRate(attempt), rng)) {
    return result;
  }
  if (attempt.requirePayment && payer != nullptr &&
      !tryPayStemBindCost(*payer, attempt.bindCostBytes)) {
    return result;
  }

  using engine::kinematics::normalizeAngle;
  SkeletonLink link;
  link.parentNodeId = parentNodeId;
  link.childNodeId = childNodeId;
  link.restLength = restLength;
  link.jointAngle = normalizeAngle(heading + segmentAngleOffset);
  link.energyEta = 0.0f;
  link.muscleBundle = true;
  organism.links.push_back(link);

  StemChainRecord record;
  record.parentNodeId = parentNodeId;
  record.childNodeId = childNodeId;
  record.segmentAngleOffset = segmentAngleOffset;
  record.restLength = restLength;
  record.muscleBundle = true;
  organism.stemAssembly.chains.push_back(record);

  result.ok = true;
  result.record = record;
  return result;
}

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

std::size_t stemHubVitalSpendFloorBytes() {
  return kComputerHubReserveBytes + kComputerHubConservationSlackBytes;
}

bool stemHubHasVitalSpendRoom(const Organism& organism, std::size_t bytes) {
  return computerHubFuelBytes(organism) >= stemHubVitalSpendFloorBytes() + bytes;
}

bool tryConsumeNodeFuel(SkeletonNode& node, std::size_t bytes) {
  if (node.store.size() < bytes) {
    return false;
  }
  neuronConsumeBack(node, bytes);
  return true;
}

bool tryConsumeHubVitalFuel(Organism& organism, std::size_t bytes) {
  if (!stemHubHasVitalSpendRoom(organism, bytes)) {
    return false;
  }
  return hubStoreConsumeBack(organism, bytes);
}

bool tryPayStemBasalCost(Organism& organism, SkeletonNode& node) {
  if (campCoordinatorSkipsBasalPayment(organism, node)) {
    return true;
  }

  std::vector<std::uint8_t>* pool = neuronFuelPool(organism, node);
  if (pool == nullptr) {
    return true;
  }

  if (tryConsumeNodeFuel(node, kStemCellBasalCostPerTick)) {
    return true;
  }

  if (organism.isCampNom() && node.neuron != NeuronType::Computer &&
      node.neuron != NeuronType::None &&
      tryConsumeHubVitalFuel(organism, kStemCellBasalCostPerTick)) {
    return true;
  }

  if (organism.feedbagOracle) {
    SkeletonNode* root = organism.findNode(organism.rootNodeId);
    if (root != nullptr && tryConsumeNodeFuel(*root, kStemCellBasalCostPerTick)) {
      return true;
    }
  }

  if (node.neuron == NeuronType::Mouth && !organism.isCampNom() &&
      computerHubFuelBytes(organism) >= kStemCellBasalCostPerTick) {
    hubStoreConsumeBack(organism, kStemCellBasalCostPerTick);
    return true;
  }

  return false;
}

bool tryPayStemOperationalCost(Organism& organism, SkeletonNode& node, std::size_t bytes) {
  if (tryConsumeNodeFuel(node, bytes)) {
    return true;
  }
  return tryConsumeHubVitalFuel(organism, bytes);
}

void creditStemFreshEnergon(Organism& organism, SkeletonNode& node, std::uint8_t byte,
                            std::uint32_t units) {
  for (std::uint32_t i = 0; i < units; ++i) {
    if (node.neuron == NeuronType::Mouth && organismUsesCampNeuronPhases(organism)) {
      if (hubStoreAcceptanceRemaining(organism) > 0) {
        hubStorePush(organism, byte);
      } else if (node.store.size() < peripheralStoreCapBytes(organism)) {
        neuronStorePush(organism, node, byte);
      }
      continue;
    }
    neuronStorePush(organism, node, byte);
  }
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

void refreshStemSurplusExportScales(Organism& organism, StemSurplusRefreshPoint point) {
  (void)point;
  if (!organismUsesCampNeuronPhases(organism)) {
    return;
  }
  assignStemSurplusExportScales(organism);
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

}  // namespace evolab
