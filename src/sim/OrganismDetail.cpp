#include "sim/OrganismInternal.hpp"

#include "engine/kinematics/ForwardKinematics.hpp"

#include "sim/BarrenWorld.hpp"
#include "sim/CellConstants.hpp"
#include "sim/Chaos.hpp"
#include "sim/Energon.hpp"
#include "sim/EnergonString.hpp"
#include "sim/NeuralAxon.hpp"
#include "sim/Organism.hpp"
#include "sim/OrganismPerceptor.hpp"
#include "sim/NeuronSignal.hpp"
#include "sim/PerceptorFocus.hpp"
#include "sim/TideAdvection.hpp"
#include "sim/WorldConstants.hpp"

#include <algorithm>
#include <cmath>
#include <random>
#include <vector>

namespace evolab::organism_detail {

enum class MouthContactKind : std::uint8_t { None = 0, Food, EmptyString };

struct MouthContact {
  MouthContactKind kind = MouthContactKind::None;
  std::uint32_t blobId = 0;
};

void consumeBytes(std::vector<std::uint8_t>& storage, std::uint32_t count) {
  if (count == 0 || storage.empty()) {
    return;
  }
  const std::size_t removeCount = std::min<std::size_t>(storage.size(), count);
  storage.erase(storage.end() - static_cast<std::ptrdiff_t>(removeCount), storage.end());
}

void popFrontBytes(std::vector<std::uint8_t>& storage, std::size_t count) {
  if (count == 0 || storage.empty()) {
    return;
  }
  const std::size_t removeCount = std::min(storage.size(), count);
  storage.erase(storage.begin(), storage.begin() + static_cast<std::ptrdiff_t>(removeCount));
}

void mouthSpitByte(const SkeletonNode& node, EnergonField& field, std::uint8_t byte) {
  EnergonBlob fragment;
  fragment.data = byte;
  fragment.remaining = 1;
  fragment.initialBytes = 1;
  fragment.origin = EnergonOrigin::Fragment;
  fragment.x = node.worldX;
  fragment.z = node.worldZ + kWorldCellSize * kMouthContactRadiusFactor * 0.35f;
  fragment.y = node.worldY;
  fragment.grounded = true;
  fragment.onWet = true;
  fragment.ttl = field.config().ttlWetSeconds;
  energonBlobInitPoint(fragment);
  field.injectBlob(fragment);
}

void releaseBytesAtNode(const SkeletonNode& node, EnergonField& field,
                        std::vector<std::uint8_t>& storage) {
  for (std::uint8_t byte : storage) {
    mouthSpitByte(node, field, byte);
  }
  storage.clear();
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

bool tryPayNeuronBasalCost(Organism& organism, SkeletonNode& node) {
  std::vector<std::uint8_t>* pool = neuronFuelPool(organism, node);
  if (pool == nullptr) {
    return true;
  }
  if (pool->size() >= kStemCellBasalCostPerTick) {
    consumeBytes(*pool, kStemCellBasalCostPerTick);
    return true;
  }
  if (node.neuron == NeuronType::Mouth && !organism.isPmaNom() &&
      !organism.bodyStorage.empty()) {
    consumeBytes(organism.bodyStorage, kStemCellBasalCostPerTick);
    return true;
  }
  return false;
}

void removeNeuralAxonsForNode(Organism& organism, std::uint32_t nodeId) {
  organism.neuralAxons.erase(
      std::remove_if(organism.neuralAxons.begin(), organism.neuralAxons.end(),
                     [nodeId](const NeuralAxon& axon) {
                       return axon.srcNodeId == nodeId || axon.dstNodeId == nodeId;
                     }),
      organism.neuralAxons.end());
}

void releaseOrganismRemainder(Organism& organism, EnergonField& field) {
  const SkeletonNode* anchor = organism.findNode(organism.rootNodeId);
  if (anchor != nullptr && !organism.bodyStorage.empty()) {
    releaseBytesAtNode(*anchor, field, organism.bodyStorage);
  }
  for (SkeletonNode& node : organism.nodes) {
    if (!node.store.empty()) {
      releaseBytesAtNode(node, field, node.store);
    }
  }
}

void killNeuron(Organism& organism, SkeletonNode& node, EnergonField& field) {
  if (!node.alive) {
    return;
  }

  if (std::vector<std::uint8_t>* pool = neuronFuelPool(organism, node)) {
    releaseBytesAtNode(node, field, *pool);
  } else if (!node.store.empty()) {
    releaseBytesAtNode(node, field, node.store);
  }

  removeNeuralAxonsForNode(organism, node.id);
  node.alive = false;
  node.neuron = NeuronType::None;
  node.ateThisTick = false;
  node.lastEmittedByte = 0;

  if (!organism.hasLiveFunctionalNeurons()) {
    releaseOrganismRemainder(organism, field);
    organism.alive = false;
  }
}

void tickNeuronViability(Organism& organism, EnergonField& field) {
  if (!organism.alive) {
    return;
  }

  for (SkeletonNode& node : organism.nodes) {
    if (!node.alive) {
      continue;
    }
    if (node.neuron == NeuronType::None && node.id != organism.rootNodeId) {
      continue;
    }
    if (node.neuron == NeuronType::None && node.id == organism.rootNodeId &&
        organism.nodes.size() > 1) {
      continue;
    }
    if (!tryPayNeuronBasalCost(organism, node)) {
      killNeuron(organism, node, field);
    }
  }

  if (!organism.hasLiveFunctionalNeurons()) {
    releaseOrganismRemainder(organism, field);
    organism.alive = false;
  }
}

SkeletonNode* findActuatorNode(Organism& organism);

void creditMouthStore(SkeletonNode& node, EnergonField& field, std::uint8_t byte,
                      std::uint32_t units) {
  for (std::uint32_t i = 0; i < units; ++i) {
    const std::uint8_t storedByte = (i == 0) ? byte : byte;
    if (node.store.size() < kMouthLocalStoreMaxBytes) {
      node.store.push_back(storedByte);
      continue;
    }
    mouthSpitByte(node, field, storedByte);
  }
}

bool mouthShouldForward(const SkeletonNode& node, std::uint64_t simTick) {
  if (node.store.empty()) {
    return false;
  }
  if (node.store.size() >= kMouthStoreSoftPressureBytes) {
    return true;
  }
  if (node.store.size() >= kMouthLocalStoreMaxBytes) {
    return true;
  }
  return kMouthSignalHeartbeatTicks > 0 && (simTick % kMouthSignalHeartbeatTicks) == 0;
}

struct AxonOutboundPlan {
  std::uint32_t srcNodeId = 0;
  std::uint32_t dstNodeId = 0;
  bool signalValid = false;
  std::uint8_t signalByte = 0;
  std::vector<std::uint8_t> feedBytes;
};

void buildOutboundPlans(const Organism& organism, std::uint64_t simTick,
                        std::vector<AxonOutboundPlan>& plans) {
  plans.clear();
  plans.reserve(organism.neuralAxons.size());

  for (const SkeletonNode& node : organism.nodes) {
    if (!node.alive || node.neuron != NeuronType::Mouth || !mouthShouldForward(node, simTick)) {
      continue;
    }

    for (const NeuralAxon& axon : organism.neuralAxons) {
      if (axon.srcNodeId != node.id) {
        continue;
      }

      AxonOutboundPlan plan;
      plan.srcNodeId = node.id;
      plan.dstNodeId = axon.dstNodeId;

      if (axonSignalGateOpen(axon)) {
        plan.signalValid = true;
        plan.signalByte = kMouthSignalTagShipping;
      }

      const int bandwidth = axonFeedBandwidth(axon);
      // P-M-A v1: axons carry interoception/sense signals only; fuel stays in local stores.
      if (bandwidth > 0 && !organism.isPmaNom()) {
        const std::size_t take =
            std::min(node.store.size(), static_cast<std::size_t>(bandwidth));
        plan.feedBytes.assign(node.store.begin(),
                              node.store.begin() + static_cast<std::ptrdiff_t>(take));
      }

      if (plan.signalValid || !plan.feedBytes.empty()) {
        plans.push_back(std::move(plan));
      }
    }
  }
}

void deliverOutboundPlans(Organism& organism, EnergonField& field, std::uint64_t simTick,
                          const std::vector<AxonOutboundPlan>& plans) {
  for (const AxonOutboundPlan& plan : plans) {
    NeuralAxon* axon = organism.findNeuralAxon(plan.srcNodeId, plan.dstNodeId);
    if (axon == nullptr) {
      continue;
    }

    SkeletonNode* dst = organism.findNode(plan.dstNodeId);
    if (dst == nullptr) {
      continue;
    }

    if (plan.signalValid) {
      axon->pendingSend.valid = true;
      axon->pendingSend.byte = plan.signalByte;
      axon->pendingSend.tick = simTick;
      axon->lastSentByte = plan.signalByte;
      axon->lastReceived.valid = true;
      axon->lastReceived.byte = plan.signalByte;
      axon->lastReceived.tick = simTick;
    } else {
      axon->pendingSend.valid = false;
    }

    for (std::uint8_t byte : plan.feedBytes) {
      if (dst->store.size() < kMouthLocalStoreMaxBytes) {
        dst->store.push_back(byte);
      } else {
        mouthSpitByte(*dst, field, byte);
      }
    }
  }
}

void applyOutboundPlans(Organism& organism, const std::vector<AxonOutboundPlan>& plans) {
  for (const AxonOutboundPlan& plan : plans) {
    if (plan.feedBytes.empty()) {
      continue;
    }
    SkeletonNode* src = organism.findNode(plan.srcNodeId);
    if (src == nullptr) {
      continue;
    }
    popFrontBytes(src->store, plan.feedBytes.size());
  }
}

void spitMouthOverflow(Organism& organism, EnergonField& field) {
  // P-M-A Noms keep fuel in per-neuron stores (mitochondria analogue). Ingestion
  // overflow is handled in creditMouthStore; do not cap the birth/endowment budget.
  if (organism.isPmaNom()) {
    return;
  }

  for (SkeletonNode& node : organism.nodes) {
    if (!node.alive || node.neuron != NeuronType::Mouth) {
      continue;
    }
    if (node.store.size() <= kMouthLocalStoreMaxBytes) {
      continue;
    }
    const std::size_t spillCount = node.store.size() - kMouthLocalStoreMaxBytes;
    for (std::size_t i = 0; i < spillCount; ++i) {
      mouthSpitByte(node, field, node.store[i]);
    }
    popFrontBytes(node.store, spillCount);
  }
}

bool blobInRange(const EnergonBlob& blob, float wx, float wz, float radius) {
  if (!blob.grounded || !blob.onWet || blob.remaining == 0) {
    return false;
  }
  float t = 0.0f;
  const float distSq = energonPointSegmentDistanceSq(wx, wz, blob, t);
  return distSq <= radius * radius;
}

bool emptyBlobInRange(const EnergonBlob& blob, float wx, float wz, float radius) {
  if (!blob.grounded || !blob.onWet || blob.remaining != 0) {
    return false;
  }
  float t = 0.0f;
  const float distSq = energonPointSegmentDistanceSq(wx, wz, blob, t);
  return distSq <= radius * radius;
}

MouthContact findMouthContact(const EnergonField& field, float wx, float wz, float radius) {
  MouthContact best;
  float bestDistSq = radius * radius;
  bool foundFood = false;

  field.forEachBlobNear(wx, wz, radius, [&](const EnergonBlob& blob) {
    const float dx = blob.x - wx;
    const float dz = blob.z - wz;
    const float distSq = dx * dx + dz * dz;
    if (blob.remaining > 0) {
      if (!foundFood || distSq <= bestDistSq) {
        bestDistSq = distSq;
        best.kind = MouthContactKind::Food;
        best.blobId = blob.id;
        foundFood = true;
      }
      return;
    }
    if (!foundFood && distSq <= bestDistSq) {
      bestDistSq = distSq;
      best.kind = MouthContactKind::EmptyString;
      best.blobId = blob.id;
    }
  });

  return best;
}

bool tryPayMouthBiteCost(Organism& organism, SkeletonNode& node) {
  if (std::vector<std::uint8_t>* pool = neuronFuelPool(organism, node)) {
    if (pool->size() >= kBiteCost) {
      consumeBytes(*pool, kBiteCost);
      return true;
    }
  }
  if (!organism.isPmaNom() && !organism.bodyStorage.empty()) {
    consumeBytes(organism.bodyStorage, kBiteCost);
    return true;
  }
  return false;
}

void tickMouthNode(Organism& organism, SkeletonNode& node, EnergonField& field, float radius) {
  if (!organism.alive || !node.alive || node.neuron != NeuronType::Mouth) {
    return;
  }

  const MouthContact contact = findMouthContact(field, node.worldX, node.worldZ, radius);
  if (contact.kind == MouthContactKind::None) {
    return;
  }

  if (contact.kind == MouthContactKind::EmptyString) {
    if (!tryPayMouthBiteCost(organism, node)) {
      killNeuron(organism, node, field);
    }
    return;
  }

  const auto bite = field.biteAt(contact.blobId, node.worldX, node.worldZ);
  if (!bite.tookByte) {
    if (!tryPayMouthBiteCost(organism, node)) {
      killNeuron(organism, node, field);
    }
    return;
  }

  const std::uint32_t gross = kEnergonUnitsPerByte;
  const std::uint32_t net = gross > kBiteCost ? gross - kBiteCost : 0u;
  creditMouthStore(node, field, bite.byte, net);
  node.ateThisTick = true;
}

float normalizeAngle(float radians) {
  constexpr float kTwoPi = 6.2831853f;
  while (radians > 3.14159265f) {
    radians -= kTwoPi;
  }
  while (radians < -3.14159265f) {
    radians += kTwoPi;
  }
  return radians;
}

float lerpAngle(float from, float to, float t) {
  const float clamped = std::clamp(t, 0.0f, 1.0f);
  float delta = normalizeAngle(to - from);
  return normalizeAngle(from + delta * clamped);
}

float turnToward(float current, float target, float maxStep) {
  float delta = normalizeAngle(target - current);
  if (std::abs(delta) <= maxStep) {
    return normalizeAngle(target);
  }
  return normalizeAngle(current + (delta > 0.0f ? maxStep : -maxStep));
}

struct FoodHeadingCue {
  bool found = false;
  float heading = 0.0f;
  float proximity = 0.0f;
};

FoodHeadingCue findNearestFoodHeading(const EnergonField& field, float wx, float wz,
                                      float maxRadius) {
  FoodHeadingCue cue;
  const float maxRadiusSq = maxRadius * maxRadius;
  float bestDistSq = maxRadiusSq;

  field.forEachBlobNear(wx, wz, maxRadius, [&](const EnergonBlob& blob) {
    if (blob.remaining == 0) {
      return;
    }

    float t = 0.0f;
    const float distSq = energonPointSegmentDistanceSq(wx, wz, blob, t);
    if (distSq >= bestDistSq) {
      return;
    }

    bestDistSq = distSq;
    const float closestX = blob.tailX + t * (blob.headX - blob.tailX);
    const float closestZ = blob.tailZ + t * (blob.headZ - blob.tailZ);
    cue.found = true;
    cue.heading = std::atan2(closestX - wx, closestZ - wz);
    cue.proximity = 1.0f - std::sqrt(distSq) / maxRadius;
  });

  return cue;
}

FoodHeadingCue findNearestFoodForOrganism(const Organism& organism, const EnergonField& field,
                                          float senseRadius, float biteRadius) {
  FoodHeadingCue best;
  const float rootX = organism.rootWorldX();
  const float rootZ = organism.rootWorldZ();
  const float senseRadiusSq = senseRadius * senseRadius;
  const float biteRadiusSq = biteRadius * biteRadius;
  const float minHeadingDistSq = (senseRadius * 0.12f) * (senseRadius * 0.12f);
  float bestDistSq = senseRadiusSq;

  struct Probe {
    float x;
    float z;
  };
  std::vector<Probe> probes;
  probes.push_back({rootX, rootZ});
  for (const SkeletonLink& link : organism.links) {
    if (link.parentNodeId != organism.rootNodeId) {
      continue;
    }
    const float angle = link.jointAngle + organism.heading;
    probes.push_back({rootX + std::sin(angle) * link.restLength,
                      rootZ + std::cos(angle) * link.restLength});
  }

  float foodBearing = 0.0f;
  for (const Probe& probe : probes) {
    field.forEachBlobNear(probe.x, probe.z, senseRadius, [&](const EnergonBlob& blob) {
      if (blob.remaining == 0) {
        return;
      }
      float t = 0.0f;
      const float distSq = energonPointSegmentDistanceSq(probe.x, probe.z, blob, t);
      if (distSq >= bestDistSq) {
        return;
      }
      bestDistSq = distSq;
      const float closestX = blob.tailX + t * (blob.headX - blob.tailX);
      const float closestZ = blob.tailZ + t * (blob.headZ - blob.tailZ);
      best.found = true;
      foodBearing = std::atan2(closestX - rootX, closestZ - rootZ);
      best.proximity = 1.0f - std::sqrt(distSq) / senseRadius;
      if (distSq <= biteRadiusSq) {
        best.proximity = 1.0f;
      }
    });
  }

  if (!best.found) {
    return best;
  }

  if (bestDistSq < minHeadingDistSq) {
    // Sitting on food — use string axis so we still slew along the strand.
    bool axisFound = false;
    field.forEachBlobNear(rootX, rootZ, biteRadius, [&](const EnergonBlob& blob) {
      if (blob.remaining == 0 || axisFound) {
        return;
      }
      float t = 0.0f;
      if (energonPointSegmentDistanceSq(rootX, rootZ, blob, t) > biteRadiusSq) {
        return;
      }
      const float sx = blob.headX - blob.tailX;
      const float sz = blob.headZ - blob.tailZ;
      if (sx * sx + sz * sz > 1.0e-4f) {
        best.heading = std::atan2(sx, sz);
        axisFound = true;
      }
    });
    if (!axisFound) {
      best.found = false;
    }
    return best;
  }

  // Aim the mouth that requires the smallest turn to reach the food bearing.
  float bestTurn = 6.2831853f;
  float bestHeading = organism.heading;
  for (const SkeletonLink& link : organism.links) {
    if (link.parentNodeId != organism.rootNodeId) {
      continue;
    }
    const float candidate = normalizeAngle(foodBearing - link.jointAngle);
    const float turn = std::abs(normalizeAngle(candidate - organism.heading));
    if (turn < bestTurn) {
      bestTurn = turn;
      bestHeading = candidate;
    }
  }
  best.heading = bestHeading;
  return best;
}

void updateOrganismHeading(Organism& organism, const AdvectionVelocity& velocity,
                           const EnergonField& energon, float cellSize) {
  if (!organism.hasMouthNeurons()) {
    return;
  }

  const float senseRadius = cellSize * kOrganismFoodSenseRadiusFactor;
  const float biteRadius = cellSize * kMouthContactRadiusFactor;
  const FoodHeadingCue food = findNearestFoodForOrganism(organism, energon, senseRadius, biteRadius);

  bool haveTarget = false;
  float targetHeading = organism.heading;
  float turnRate = kOrganismMaxTurnPerTick;

  if (food.found) {
    targetHeading = food.heading;
    haveTarget = true;
    if (food.proximity >= 0.85f) {
      turnRate *= kOrganismFoodSnapTurnMultiplier;
    }
  } else if (velocity.active) {
    targetHeading = std::atan2(velocity.vx, velocity.vz);
    haveTarget = true;
  }

  if (haveTarget) {
    organism.heading = turnToward(organism.heading, targetHeading, turnRate);
  }
}

bool applyPmaPerceptorReflexHeading(Organism& organism, std::uint64_t simTick) {
  if (!organism.isPmaNom()) {
    return false;
  }

  const NeuralAxon* pToA = organism.findNeuralAxon(1, 3);
  if (pToA == nullptr || !pToA->lastReceived.valid || pToA->lastReceived.tick != simTick ||
      !isNeuronConfidenceByte(pToA->lastReceived.byte)) {
    return false;
  }

  const SkeletonNode* perceptor = nullptr;
  for (const SkeletonNode& node : organism.nodes) {
    if (node.alive && node.neuron == NeuronType::Perceptor) {
      perceptor = &node;
      break;
    }
  }
  if (perceptor == nullptr || !perceptor->focusLocked) {
    return false;
  }

  const float confidence = static_cast<float>(pToA->lastReceived.byte);
  const float valence = (confidence - 3.5f) / 3.5f;
  if (std::abs(valence) < kOrganismPmaReflexMinValence) {
    return false;
  }

  const float fleeOffset = valence < 0.0f ? 3.14159265f : 0.0f;
  const float targetHeading =
      normalizeAngle(perceptor->gazeHeading + perceptor->focusBearing + fleeOffset);
  const float turnRate = kOrganismMaxTurnPerTick * std::abs(valence) *
                         (0.5f + perceptor->focusSalience * 0.5f);
  organism.heading = turnToward(organism.heading, targetHeading, turnRate);
  return true;
}

bool payActuatorStrokeCost(Organism& organism, std::uint32_t& fromBody,
                           std::uint32_t& fromActuatorStore) {
  fromBody = 0;
  fromActuatorStore = 0;

  if (organism.isPmaNom()) {
    SkeletonNode* motor = findActuatorNode(organism);
    if (motor == nullptr || !motor->alive) {
      return false;
    }
    if (motor->store.size() < kActuatorStrokeCostPerTick) {
      return false;
    }
    consumeBytes(motor->store, kActuatorStrokeCostPerTick);
    fromActuatorStore = kActuatorStrokeCostPerTick;
    return true;
  }

  if (!organism.bodyStorage.empty()) {
    const std::uint32_t taken =
        std::min(kActuatorStrokeCostPerTick,
                 static_cast<std::uint32_t>(organism.bodyStorage.size()));
    consumeBytes(organism.bodyStorage, taken);
    fromBody = taken;
    return taken == kActuatorStrokeCostPerTick;
  }
  return false;
}

bool actuatorInhibitedByMouthSignal(const Organism& organism, std::uint64_t simTick) {
  for (const NeuralAxon& axon : organism.neuralAxons) {
    const SkeletonNode* src = organism.findNode(axon.srcNodeId);
    const SkeletonNode* dst = organism.findNode(axon.dstNodeId);
    if (src == nullptr || dst == nullptr || !src->alive || !dst->alive ||
        src->neuron != NeuronType::Mouth || dst->neuron != NeuronType::Actuator) {
      continue;
    }
    if (axon.lastReceived.valid && axon.lastReceived.tick == simTick &&
        isNeuronConfidenceByte(axon.lastReceived.byte) &&
        axon.lastReceived.byte >= kMouthInhibitActuatorConfidence) {
      return true;
    }
  }
  return false;
}

SkeletonNode* findActuatorNode(Organism& organism) {
  for (SkeletonNode& node : organism.nodes) {
    if (node.alive && node.neuron == NeuronType::Actuator) {
      return &node;
    }
  }
  return nullptr;
}

void translateOrganismXZ(Organism& organism, float dx, float dz) {
  engine::kinematics::translateNodesXZ(std::span(organism.nodes), dx, dz);
}

void emitActuatorConfidenceSignals(Organism& organism, std::uint64_t simTick) {
  if (!organism.isPmaNom()) {
    return;
  }

  const SkeletonNode* actuator = nullptr;
  for (const SkeletonNode& node : organism.nodes) {
    if (node.alive && node.neuron == NeuronType::Actuator) {
      actuator = &node;
      break;
    }
  }
  if (actuator == nullptr) {
    return;
  }

  const std::uint8_t confidence =
      actuatorActivityConfidence(organism.lastStrokePaid, organism.lastStrokeBytesPaid);
  organism.lastActuatorOutboundSignal = confidence;

  for (NeuralAxon& axon : organism.neuralAxons) {
    if (axon.srcNodeId != actuator->id) {
      continue;
    }
    const SkeletonNode* dst = organism.findNode(axon.dstNodeId);
    if (dst == nullptr || !dst->alive ||
        (dst->neuron != NeuronType::Mouth && dst->neuron != NeuronType::Perceptor)) {
      continue;
    }
    writeAxonConfidence(axon, confidence, simTick);
  }
}

void emitMouthConfidenceSignals(Organism& organism, std::uint64_t simTick) {
  if (!organism.isPmaNom()) {
    return;
  }

  for (const SkeletonNode& node : organism.nodes) {
    if (!node.alive || node.neuron != NeuronType::Mouth) {
      continue;
    }
    const std::uint8_t confidence = mouthFuelConfidence(node);
    for (NeuralAxon& axon : organism.neuralAxons) {
      if (axon.srcNodeId != node.id) {
        continue;
      }
      const SkeletonNode* dst = organism.findNode(axon.dstNodeId);
      if (dst == nullptr || !dst->alive ||
          (dst->neuron != NeuronType::Perceptor && dst->neuron != NeuronType::Actuator)) {
        continue;
      }
      writeAxonConfidence(axon, confidence, simTick);
    }
  }
}

void tickActuatorOrganism(Organism& organism, const BarrenWorld& world, float cellSize,
                          float halfExtent, std::uint64_t simTick) {
  SkeletonNode* root = organism.findNode(organism.rootNodeId);
  if (root == nullptr) {
    return;
  }

  SkeletonNode* motorNode = findActuatorNode(organism);
  if (motorNode == nullptr) {
    motorNode = root;
  }

  const float startX = root->worldX;
  const float startZ = root->worldZ;

  organism.lastStrokePaid = false;
  organism.lastTumbled = false;
  organism.lastIntendedThrust = 0.0f;
  organism.lastMechanicalThrust = 0.0f;
  organism.lastTranslationEntropyLoss = 0.0f;
  organism.lastStrokeBytesPaid = 0;
  organism.lastStrokeBytesFromBody = 0;
  organism.lastStrokeBytesFromActuatorStore = 0;
  organism.lastActuatorInhibited = false;
  organism.lastActuatorOutboundSignal = 0;
  organism.lastInWater =
      world.isWetWorld(motorNode->worldX, motorNode->worldZ, cellSize);
  organism.lastTideDelta = world.waterLevelDelta();

  const bool inhibited =
      organism.isPmaNom() && actuatorInhibitedByMouthSignal(organism, simTick);
  organism.lastActuatorInhibited = inhibited;

  std::mt19937 rng = chaosSpawnRng(world.tickCount(), static_cast<std::uint64_t>(organism.id) ^
                                                         kChaosSaltActuator);

  if (chaosBernoulli(kActuatorTumbleRate, rng)) {
    const float sign = chaosBernoulli(0.5f, rng) ? 1.0f : -1.0f;
    organism.heading = normalizeAngle(organism.heading + sign * kActuatorTumbleTurn);
    organism.lastTumbled = true;
  }

  if (organism.isPmaNom()) {
    applyPmaPerceptorReflexHeading(organism, simTick);
  }

  if (organism.lastInWater && !inhibited &&
      payActuatorStrokeCost(organism, organism.lastStrokeBytesFromBody,
                            organism.lastStrokeBytesFromActuatorStore)) {
    const float strokeBytes = static_cast<float>(kActuatorStrokeCostPerTick);
    organism.lastStrokePaid = true;
    organism.lastStrokeBytesPaid = kActuatorStrokeCostPerTick;

    const float grossThrust = strokeBytes * kActuatorThrustPerStrokeByte;
    const float mechanicalThrust = grossThrust * kActuatorTranslationEta;
    organism.lastIntendedThrust = grossThrust;
    organism.lastMechanicalThrust = mechanicalThrust;
    organism.lastTranslationEntropyLoss = strokeBytes * (1.0f - kActuatorTranslationEta);

    const float thrustX = std::sin(organism.heading) * mechanicalThrust;
    const float thrustZ = std::cos(organism.heading) * mechanicalThrust;
    translateOrganismXZ(organism, thrustX, thrustZ);
  }

  const AdvectionVelocity velocity =
      shoreAdvection(world, root->worldX, root->worldZ, cellSize, halfExtent);
  if (organism.lastInWater) {
    const float tideStartX = root->worldX;
    const float tideStartZ = root->worldZ;
    applyShoreAdvection(root->worldX, root->worldZ, velocity, halfExtent, cellSize * 0.25f);
    const float tideDx = root->worldX - tideStartX;
    const float tideDz = root->worldZ - tideStartZ;
    if (tideDx != 0.0f || tideDz != 0.0f) {
      for (SkeletonNode& node : organism.nodes) {
        if (&node == root) {
          continue;
        }
        node.worldX += tideDx;
        node.worldZ += tideDz;
      }
    }
  }
  clampWorldPosition(root->worldX, root->worldZ, halfExtent, cellSize * 0.25f);

  emitActuatorConfidenceSignals(organism, simTick);

  const float dx = root->worldX - startX;
  const float dz = root->worldZ - startZ;
  organism.lastDisplacement = std::sqrt(dx * dx + dz * dz);
}

void runMouthSignalPhase(Organism& organism, EnergonField& field, std::uint64_t simTick) {
  emitMouthConfidenceSignals(organism, simTick);
  std::vector<AxonOutboundPlan> plans;
  buildOutboundPlans(organism, simTick, plans);
  deliverOutboundPlans(organism, field, simTick, plans);
  applyOutboundPlans(organism, plans);
  spitMouthOverflow(organism, field);
}

}  // namespace evolab::organism_detail
