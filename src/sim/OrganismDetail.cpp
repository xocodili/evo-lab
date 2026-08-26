#include "sim/OrganismInternal.hpp"

#include "engine/kinematics/ForwardKinematics.hpp"

#include "sim/BarrenWorld.hpp"
#include "sim/CellConstants.hpp"
#include "sim/Chaos.hpp"
#include "sim/Energon.hpp"
#include "sim/EnergonString.hpp"
#include "sim/NeuralAxon.hpp"
#include "sim/Organism.hpp"
#include "sim/OrganismActuator.hpp"
#include "sim/OrganismMouth.hpp"
#include "sim/OrganismNeuron.hpp"
#include "sim/OrganismPerceptor.hpp"
#include "sim/NeuronTrust.hpp"
#include "sim/NeuronFuel.hpp"
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
    if (pool == &node.store) {
      neuronConsumeBack(node, kStemCellBasalCostPerTick);
    } else {
      consumeBytes(*pool, kStemCellBasalCostPerTick);
    }
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
  (void)field;
  for (std::uint32_t i = 0; i < units; ++i) {
    neuronStorePush(node, byte);
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
      if (pool == &node.store) {
        neuronConsumeBack(node, kBiteCost);
      } else {
        consumeBytes(*pool, kBiteCost);
      }
      return true;
    }
  }
  if (!organism.isPmaNom() && !organism.bodyStorage.empty()) {
    consumeBytes(organism.bodyStorage, kBiteCost);
    return true;
  }
  return false;
}

void tickMouthNode(Organism& organism, SkeletonNode& node, EnergonField& field, float radius,
                   std::uint64_t simTick, const FeedIntent* pmaFeedIntent) {
  (void)simTick;
  if (!organism.alive || !node.alive || node.neuron != NeuronType::Mouth) {
    return;
  }

  const MouthContact contact = findMouthContact(field, node.worldX, node.worldZ, radius);
  if (contact.kind == MouthContactKind::None) {
    return;
  }
  organism.lastMouthHadFoodContact = (contact.kind == MouthContactKind::Food);

  if (pmaFeedIntent != nullptr && !pmaFeedIntent->allowFoodBite) {
    organism.lastMouthFeedSuppressed = true;
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

bool payActuatorStrokeCost(Organism& organism, std::uint32_t bytesRequested,
                           std::uint32_t& fromBody, std::uint32_t& fromActuatorStore) {
  fromBody = 0;
  fromActuatorStore = 0;
  if (bytesRequested == 0) {
    return false;
  }

  if (organism.isPmaNom()) {
    SkeletonNode* motor = findActuatorNode(organism);
    if (motor == nullptr || !motor->alive) {
      return false;
    }
    if (motor->store.size() < bytesRequested) {
      return false;
    }
    neuronConsumeBack(*motor, bytesRequested);
    fromActuatorStore = bytesRequested;
    return true;
  }

  if (bytesRequested != kActuatorStrokeCostPerTick) {
    return false;
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

  const SkeletonNode* actuator = findFirstNeuronNode(organism, NeuronType::Actuator);
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

void emitMouthConfidenceSignals(Organism& organism, std::uint64_t simTick) {
  if (!organism.isPmaNom()) {
    return;
  }

  static constexpr NeuronType kAllowedDst[] = {NeuronType::Perceptor, NeuronType::Actuator};
  for (const SkeletonNode& node : organism.nodes) {
    if (!node.alive || node.neuron != NeuronType::Mouth) {
      continue;
    }
    const std::uint8_t confidence = mouthFuelConfidence(node);
    emitOutboundConfidence(organism, node.id, confidence, simTick, kAllowedDst,
                           std::size(kAllowedDst));
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
  organism.lastActuatorNetDrive = 0.0f;
  organism.lastActuatorOutboundSignal = 0;
  organism.lastInWater =
      world.isWetWorld(motorNode->worldX, motorNode->worldZ, cellSize);
  organism.lastTideDelta = world.waterLevelDelta();

  std::mt19937 rng = chaosSpawnRng(world.tickCount(), static_cast<std::uint64_t>(organism.id) ^
                                                         kChaosSaltActuator);

  MotorIntent motorIntent{};
  ActuatorInteroception interoception{};
  if (organism.isPmaNom()) {
    interoception = gatherActuatorInteroception(organism, motorNode->id, simTick);
    motorIntent = computePmaMotorIntent(
        interoception, static_cast<std::uint32_t>(motorNode->store.size()));
    organism.lastActuatorNetDrive = motorIntent.netDrive;
    organism.lastActuatorInhibited = motorIntent.motorSuppressed;

    const float effectiveTumbleRate = kActuatorTumbleRate * motorIntent.tumbleRateScale;
    if (chaosBernoulli(effectiveTumbleRate, rng)) {
      const float sign = chaosBernoulli(0.5f, rng) ? 1.0f : -1.0f;
      organism.heading = normalizeAngle(organism.heading + sign * kActuatorTumbleTurn);
      organism.lastTumbled = true;
    }

    applyPmaChemotaxisHeading(organism, interoception, motorIntent);
  } else if (chaosBernoulli(kActuatorTumbleRate, rng)) {
    const float sign = chaosBernoulli(0.5f, rng) ? 1.0f : -1.0f;
    organism.heading = normalizeAngle(organism.heading + sign * kActuatorTumbleTurn);
    organism.lastTumbled = true;
  }

  const std::uint32_t strokeRequest =
      organism.isPmaNom() ? motorIntent.strokeBytes : kActuatorStrokeCostPerTick;
  if (organism.lastInWater &&
      payActuatorStrokeCost(organism, strokeRequest, organism.lastStrokeBytesFromBody,
                            organism.lastStrokeBytesFromActuatorStore)) {
    const float strokeBytes = static_cast<float>(strokeRequest);
    organism.lastStrokePaid = true;
    organism.lastStrokeBytesPaid = strokeRequest;

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

  if (organism.isPmaNom()) {
    applyPmaActuatorTrustLearning(organism, motorNode->id, interoception, motorIntent,
                                  organism.lastDisplacement, simTick);
  }
}

void runMouthSignalPhase(Organism& organism, EnergonField& field, std::uint64_t simTick) {
  (void)field;
  // PMA mouths emit during pre-advect (before A reads M→A satiation); skip duplicate here.
  if (organism.isPmaNom()) {
    return;
  }
  emitMouthConfidenceSignals(organism, simTick);
}

}  // namespace evolab::organism_detail
