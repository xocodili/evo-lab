#include "sim/OrganismInternal.hpp"

#include "engine/kinematics/ForwardKinematics.hpp"

#include "sim/BarrenWorld.hpp"
#include "sim/CampTopology.hpp"
#include "sim/CellConstants.hpp"
#include "sim/Chaos.hpp"
#include "sim/CloacaSignal.hpp"
#include "sim/Energon.hpp"
#include "sim/EnergonString.hpp"
#include "sim/NeuralAxon.hpp"
#include "sim/NeuronStem.hpp"
#include "sim/Organism.hpp"
#include "sim/OrganismActuator.hpp"
#include "sim/OrganismMouth.hpp"
#include "sim/OrganismNeuron.hpp"
#include "sim/OrganismPerceptor.hpp"
#include "sim/NeuronTrust.hpp"
#include "sim/NeuronFuel.hpp"
#include "sim/NeuronMusculature.hpp"
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
  EnergonOrigin origin = EnergonOrigin::Sunfall;
  CloacaBand cloacaBand = CloacaBand::None;
};

void consumeBytes(std::vector<std::uint8_t>& storage, std::uint32_t count) {
  consumeFuelBack(storage, count);
}

void releaseBytesAtNode(const SkeletonNode& node, EnergonField& field,
                        std::vector<std::uint8_t>& storage) {
  releaseFuelAtNode(node, field, storage, EnergonOrigin::Fragment, 1.0f);
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

  if (std::vector<std::uint8_t>* pool = evolab::neuronFuelPool(organism, node)) {
    releaseBytesAtNode(node, field, *pool);
  } else if (!node.store.empty()) {
    releaseBytesAtNode(node, field, node.store);
  }

  transitionAxonsOnNeuronDeath(organism, node);
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
    if (!evolab::tryPayNeuronBasalCost(organism, node)) {
      if (node.basalArrearsTicks < kNeuronBasalGraceTicks) {
        ++node.basalArrearsTicks;
      } else {
        killNeuron(organism, node, field);
      }
    } else {
      node.basalArrearsTicks = 0;
    }
  }

  if (!organism.hasLiveFunctionalNeurons()) {
    releaseOrganismRemainder(organism, field);
    organism.alive = false;
  }
}

SkeletonNode* findActuatorNode(Organism& organism);

void creditMouthStore(Organism& organism, SkeletonNode& node, EnergonField& field,
                      std::uint8_t byte, std::uint32_t units) {
  (void)field;
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
    if (!blob.grounded || !blob.onWet) {
      return;
    }

    float t = 0.0f;
    const float distSq = energonPointSegmentDistanceSq(wx, wz, blob, t);
    if (distSq > bestDistSq) {
      return;
    }

    if (blob.remaining > 0) {
      bestDistSq = distSq;
      best.kind = MouthContactKind::Food;
      best.blobId = blob.id;
      best.origin = blob.origin;
      best.cloacaBand = cloacaBandFromBlob(blob);
      foundFood = true;
      return;
    }

    if (!foundFood) {
      bestDistSq = distSq;
      best.kind = MouthContactKind::EmptyString;
      best.blobId = blob.id;
    }
  });

  return best;
}

bool tryPayMouthBiteCost(Organism& organism, SkeletonNode& node) {
  if (node.store.size() >= kBiteCost) {
    neuronConsumeBack(node, kBiteCost);
    return true;
  }
  if (hubStoreConsumeBack(organism, kBiteCost)) {
    return true;
  }
  return false;
}

void tickMouthNode(Organism& organism, SkeletonNode& node, EnergonField& field, float contactWx,
                   float contactWz, float radius, std::uint64_t simTick,
                   const FeedIntent* pmaFeedIntent) {
  (void)simTick;
  if (!organism.alive || !node.alive || node.neuron != NeuronType::Mouth) {
    return;
  }

  const MouthContact contact = findMouthContact(field, contactWx, contactWz, radius);
  if (contact.kind == MouthContactKind::None) {
    return;
  }
  organism.lastMouthHadFoodContact = (contact.kind == MouthContactKind::Food);
  field.setMouthAnchor(contact.blobId, organism.id, node.id, node.worldX, node.worldZ);

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
    return;
  }
  field.setMouthAnchor(contact.blobId, organism.id, node.id, node.worldX, node.worldZ);

  recordMouthDietBite(node, contact.origin, contact.cloacaBand);

  const std::uint32_t gross = kEnergonUnitsPerByte;
  const std::uint32_t net = gross > kBiteCost ? gross - kBiteCost : 0u;
  creditMouthStore(organism, node, field, bite.byte, net);
  creditMouthChew(node, net);
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

  if (organism.isCampNom()) {
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
  SkeletonNode* loneActuator = findActuatorNode(organism);
  if (loneActuator != nullptr && loneActuator->store.size() >= bytesRequested) {
    neuronConsumeBack(*loneActuator, bytesRequested);
    fromBody = bytesRequested;
    return true;
  }
  return false;
}

SkeletonNode* findActuatorNode(Organism& organism) {
  return evolab::findNeuronNode(organism, NeuronType::Actuator);
}

void translateOrganismXZ(Organism& organism, float dx, float dz) {
  engine::kinematics::translateNodesXZ(std::span(organism.nodes), dx, dz);
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
  if (organism.isCampNom()) {
    organism.campAdvectStartX = startX;
    organism.campAdvectStartZ = startZ;
    organism.campActuatorProprio.pending = false;
  }

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
  organism.lastActuatorInteroception = {};
  organism.lastMotorIntent = {};
  organism.lastActuatorOutboundSignal = 0;
  organism.lastActuatorStrokeFlexBoost = 0.0f;
  organism.lastInWater =
      world.isWetWorld(motorNode->worldX, motorNode->worldZ, cellSize);
  organism.lastTideDelta = world.waterLevelDelta();

  std::mt19937 rng = chaosSpawnRng(world.tickCount(), static_cast<std::uint64_t>(organism.id) ^
                                                         kChaosSaltActuator);

  MotorIntent motorIntent{};
  ActuatorInteroception interoception{};
  if (organism.isCampNom()) {
    interoception = gatherActuatorInteroception(organism, motorNode->id, simTick);
    motorIntent = computeCampMotorIntent(
        interoception, static_cast<std::uint32_t>(motorNode->store.size()),
        motorNode->coordinatorDutyScale);
    organism.lastActuatorInteroception = interoception;
    organism.lastMotorIntent = motorIntent;
    organism.lastActuatorNetDrive = motorIntent.netDrive;
    organism.lastActuatorInhibited = motorIntent.motorSuppressed;

    if (!organism.disableNurseryLocomotion) {
      applyCampChemotaxisHeading(organism, interoception, motorIntent);

      const bool anchored = campLocomotionAnchored(interoception);
      const float effectiveTumbleRate =
          organism.tumbleRateFactor * kActuatorTumbleRate * motorIntent.tumbleRateScale;
      const float effectiveTumbleTurn = organism.tumbleTurnFactor * kActuatorTumbleTurn;
      if (!anchored && chaosBernoulli(effectiveTumbleRate, rng)) {
        const float rightProb =
            0.5f + 0.5f * std::clamp(organism.tumbleChiralityBias, -kTumbleChiralityBiasMax,
                                     kTumbleChiralityBiasMax);
        const float sign = chaosBernoulli(rightProb, rng) ? 1.0f : -1.0f;
        organism.heading = normalizeAngle(organism.heading + sign * effectiveTumbleTurn);
        organism.lastTumbled = true;
      }
    }
  } else if (!organism.disableNurseryLocomotion &&
             chaosBernoulli(organism.tumbleRateFactor * kActuatorTumbleRate, rng)) {
    const float rightProb =
        0.5f + 0.5f * std::clamp(organism.tumbleChiralityBias, -kTumbleChiralityBiasMax,
                                 kTumbleChiralityBiasMax);
    const float sign = chaosBernoulli(rightProb, rng) ? 1.0f : -1.0f;
    organism.heading =
        normalizeAngle(organism.heading + sign * organism.tumbleTurnFactor * kActuatorTumbleTurn);
    organism.lastTumbled = true;
  }

  const std::uint32_t strokeRequest =
      organism.isCampNom() ? motorIntent.strokeBytes : kActuatorStrokeCostPerTick;
  if (!organism.disableNurseryLocomotion && organism.lastInWater &&
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
    if (organism.isCampNom()) {
      float thrustHeading = organism.heading;
      if (interoception.perceptorLocked && interoception.focusKind == PerceptFocusKind::Threat &&
          interoception.flee > interoception.approach) {
        thrustHeading = normalizeAngle(interoception.gazeHeading + interoception.focusBearing +
                                       3.14159265f);
      }
      queueCampStrokeImpulse(organism, mechanicalThrust, thrustHeading);
      organism.campActuatorProprio.pending = true;
      organism.campActuatorProprio.startX = startX;
      organism.campActuatorProprio.startZ = startZ;
      organism.campActuatorProprio.actuatorId = motorNode->id;
      organism.campActuatorProprio.interoception = interoception;
      organism.campActuatorProprio.motorIntent = motorIntent;
    } else {
      translateOrganismXZ(organism, thrustX, thrustZ);
    }
  }

  const AdvectionVelocity velocity =
      organism.disableTideAdvection
          ? AdvectionVelocity{}
          : shoreAdvection(world, root->worldX, root->worldZ, cellSize, halfExtent);
  if (organism.lastInWater && !organism.disableTideAdvection) {
    float tideTargetX = root->worldX;
    float tideTargetZ = root->worldZ;
    applyShoreAdvection(tideTargetX, tideTargetZ, velocity, halfExtent, cellSize * 0.25f);
    organism.lastTideVelX = tideTargetX - root->worldX;
    organism.lastTideVelZ = tideTargetZ - root->worldZ;
    if (!organism.isCampNom()) {
      root->worldX = tideTargetX;
      root->worldZ = tideTargetZ;
      for (SkeletonNode& node : organism.nodes) {
        if (&node == root) {
          continue;
        }
        node.worldX += organism.lastTideVelX;
        node.worldZ += organism.lastTideVelZ;
      }
    }
  } else {
    organism.lastTideVelX = 0.0f;
    organism.lastTideVelZ = 0.0f;
  }
  clampWorldPosition(root->worldX, root->worldZ, halfExtent, cellSize * 0.25f);

  evolab::emitCampActuatorSignals(organism, simTick);

  if (organism.isCampNom()) {
    commitActuatorMouthInboundPrior(organism, interoception, simTick);
  }

  if (!organism.isCampNom()) {
    const float dx = root->worldX - startX;
    const float dz = root->worldZ - startZ;
    organism.lastDisplacement = std::sqrt(dx * dx + dz * dz);
  }
}

void finalizeCampActuatorProprioception(Organism& organism, std::uint64_t simTick) {
  if (!organism.isCampNom() || !organism.alive) {
    return;
  }
  SkeletonNode* root = organism.findNode(organism.rootNodeId);
  if (root == nullptr) {
    return;
  }

  const float dx = root->worldX - organism.campAdvectStartX;
  const float dz = root->worldZ - organism.campAdvectStartZ;
  organism.lastDisplacement = std::sqrt(dx * dx + dz * dz);

  if (!organism.campActuatorProprio.pending) {
    return;
  }

  applyCampActuatorTrustLearning(organism, organism.campActuatorProprio.actuatorId,
                                 organism.campActuatorProprio.interoception,
                                 organism.campActuatorProprio.motorIntent,
                                 organism.lastDisplacement, simTick);
  organism.campActuatorProprio.pending = false;
}

void runMouthSignalPhase(Organism& organism, EnergonField& field, std::uint64_t simTick) {
  (void)field;
  (void)simTick;
  // CAMP mouths emit during pre-advect (before A reads M→A satiation).
}

}  // namespace evolab::organism_detail
