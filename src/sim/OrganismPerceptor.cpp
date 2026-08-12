#include "sim/OrganismPerceptor.hpp"

#include "sim/BarrenWorld.hpp"
#include "sim/CellConstants.hpp"
#include "sim/Energon.hpp"
#include "sim/EnergonString.hpp"
#include "sim/NeuralAxon.hpp"
#include "sim/Organism.hpp"
#include "sim/OrganismInternal.hpp"
#include "sim/TideAdvection.hpp"

#include <algorithm>
#include <cmath>

namespace evolab {

namespace {

enum class PerceptKind : std::uint8_t { None = 0, Food, Organism, Block };

struct PerceptHit {
  PerceptKind kind = PerceptKind::None;
  float relBearing = 0.0f;
  float range01 = 1.0f;
  int priority = 0;
};

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

bool pointInFocusCone(float originX, float originZ, float heading, float halfAngle, float range,
                      float px, float pz, float& relBearing, float& range01) {
  const float dx = px - originX;
  const float dz = pz - originZ;
  const float distSq = dx * dx + dz * dz;
  if (distSq < 1.0e-8f) {
    return false;
  }
  const float maxRangeSq = range * range;
  if (distSq > maxRangeSq) {
    return false;
  }
  const float dist = std::sqrt(distSq);
  const float bearing = std::atan2(dx, dz);
  relBearing = normalizeAngle(bearing - heading);
  if (std::abs(relBearing) > halfAngle) {
    return false;
  }
  range01 = dist / range;
  return true;
}

std::uint8_t tagForKind(PerceptKind kind) {
  switch (kind) {
    case PerceptKind::Food:
      return kSignalTagISenseFood;
    case PerceptKind::Organism:
      return kSignalTagISenseOrganism;
    case PerceptKind::Block:
      return kSignalTagISenseBlock;
    default:
      return 0;
  }
}

int priorityForKind(PerceptKind kind) {
  switch (kind) {
    case PerceptKind::Block:
      return 3;
    case PerceptKind::Organism:
      return 2;
    case PerceptKind::Food:
      return 1;
    default:
      return 0;
  }
}

void considerHit(PerceptHit& best, PerceptKind kind, float relBearing, float range01) {
  const int priority = priorityForKind(kind);
  if (priority == 0) {
    return;
  }
  if (priority > best.priority || (priority == best.priority && range01 < best.range01)) {
    best.kind = kind;
    best.relBearing = relBearing;
    best.range01 = range01;
    best.priority = priority;
  }
}

PerceptHit scanFood(const SkeletonNode& perceptor, float heading, float senseRadius,
                    const EnergonField& energon) {
  PerceptHit best;
  energon.forEachBlobNear(
      perceptor.worldX, perceptor.worldZ, senseRadius, [&](const EnergonBlob& blob) {
        if (blob.remaining == 0) {
          return;
        }
        float t = 0.0f;
        const float distSq =
            energonPointSegmentDistanceSq(perceptor.worldX, perceptor.worldZ, blob, t);
        if (distSq > senseRadius * senseRadius) {
          return;
        }
        const float closestX = blob.tailX + t * (blob.headX - blob.tailX);
        const float closestZ = blob.tailZ + t * (blob.headZ - blob.tailZ);
        float relBearing = 0.0f;
        float range01 = 0.0f;
        if (!pointInFocusCone(perceptor.worldX, perceptor.worldZ, heading, kPerceptorFocusHalfAngle,
                              senseRadius, closestX, closestZ, relBearing, range01)) {
          return;
        }
        considerHit(best, PerceptKind::Food, relBearing, range01);
      });
  return best;
}

PerceptHit scanOrganisms(const Organism& self, const SkeletonNode& perceptor, float heading,
                         float senseRadius, const std::vector<Organism>& population) {
  PerceptHit best;
  const float broadRadius = senseRadius * 1.5f;
  const float broadRadiusSq = broadRadius * broadRadius;
  for (const Organism& other : population) {
    if (!other.alive || other.id == self.id) {
      continue;
    }
    const float dx = other.rootWorldX() - perceptor.worldX;
    const float dz = other.rootWorldZ() - perceptor.worldZ;
    if (dx * dx + dz * dz > broadRadiusSq) {
      continue;
    }
    for (const SkeletonNode& node : other.nodes) {
      if (!node.alive || node.neuron == NeuronType::None) {
        continue;
      }
      float relBearing = 0.0f;
      float range01 = 0.0f;
      if (!pointInFocusCone(perceptor.worldX, perceptor.worldZ, heading, kPerceptorFocusHalfAngle,
                            senseRadius, node.worldX, node.worldZ, relBearing, range01)) {
        continue;
      }
      considerHit(best, PerceptKind::Organism, relBearing, range01);
    }
  }
  return best;
}

PerceptHit scanBlocks(const BarrenWorld& world, const SkeletonNode& perceptor, float heading,
                      float senseRadius, float cellSize, float halfExtent) {
  PerceptHit best;
  const float fx = std::sin(heading);
  const float fz = std::cos(heading);
  const float samples[] = {0.35f, 0.6f, 0.85f, 1.0f};
  for (float fraction : samples) {
    const float probeX = perceptor.worldX + fx * senseRadius * fraction;
    const float probeZ = perceptor.worldZ + fz * senseRadius * fraction;
    float clampedX = probeX;
    float clampedZ = probeZ;
    clampWorldPosition(clampedX, clampedZ, halfExtent, cellSize * 0.25f);
    const bool atBoundary =
        std::abs(clampedX - probeX) > 1.0e-3f || std::abs(clampedZ - probeZ) > 1.0e-3f;
    const bool dry = !world.isWetWorld(clampedX, clampedZ, cellSize);
    if (!atBoundary && !dry) {
      continue;
    }
    float relBearing = 0.0f;
    float range01 = 0.0f;
    if (!pointInFocusCone(perceptor.worldX, perceptor.worldZ, heading, kPerceptorFocusHalfAngle,
                          senseRadius, clampedX, clampedZ, relBearing, range01)) {
      continue;
    }
    considerHit(best, PerceptKind::Block, relBearing, range01);
  }
  return best;
}

PerceptHit mergeHits(const PerceptHit& a, const PerceptHit& b) {
  if (a.priority >= b.priority) {
    return a;
  }
  return b;
}

void emitPerceptSignals(Organism& organism, std::uint32_t perceptorId, std::uint8_t tag,
                        std::uint64_t simTick) {
  if (tag == 0) {
    return;
  }
  for (NeuralAxon& axon : organism.neuralAxons) {
    if (axon.srcNodeId != perceptorId) {
      continue;
    }
    axon.lastSentByte = tag;
    axon.lastReceived.valid = true;
    axon.lastReceived.byte = tag;
    axon.lastReceived.tick = simTick;
  }
}

SkeletonNode* findLivePerceptor(Organism& organism) {
  for (SkeletonNode& node : organism.nodes) {
    if (node.alive && node.neuron == NeuronType::Perceptor) {
      return &node;
    }
  }
  return nullptr;
}

}  // namespace

void runPerceptorPhase(Organism& organism, const BarrenWorld& world, const EnergonField& energon,
                       float cellSize, float halfExtent, const std::vector<Organism>& population,
                       std::uint64_t simTick) {
  if (!organism.alive || !organism.isPmaNom()) {
    return;
  }

  SkeletonNode* perceptor = findLivePerceptor(organism);
  if (perceptor == nullptr) {
    return;
  }

  organism.lastPerceptTag = 0;
  organism.lastPerceptBearing = 0.0f;
  organism.lastPerceptRange = 0.0f;
  organism.lastPerceptScanPaid = false;
  organism.lastPerceptBytesPaid = 0;

  if (perceptor->store.size() < kPerceptorScanCostPerTick) {
    return;
  }

  const float senseRadius = cellSize * kPerceptorSenseRadiusFactor;
  const PerceptHit food = scanFood(*perceptor, organism.heading, senseRadius, energon);
  const PerceptHit organismHit =
      scanOrganisms(organism, *perceptor, organism.heading, senseRadius, population);
  const PerceptHit block =
      scanBlocks(world, *perceptor, organism.heading, senseRadius, cellSize, halfExtent);

  PerceptHit best = mergeHits(food, organismHit);
  best = mergeHits(best, block);

  std::uint32_t bytesDue = kPerceptorScanCostPerTick;
  if (best.kind != PerceptKind::None) {
    bytesDue += kPerceptorTransductionCostPerTick;
  }
  if (perceptor->store.size() < bytesDue) {
    return;
  }

  organism_detail::consumeBytes(perceptor->store, bytesDue);
  organism.lastPerceptScanPaid = true;
  organism.lastPerceptBytesPaid = bytesDue;

  if (best.kind == PerceptKind::None) {
    return;
  }

  organism.lastPerceptTag = tagForKind(best.kind);
  organism.lastPerceptBearing = best.relBearing;
  organism.lastPerceptRange = best.range01;
  perceptor->lastEmittedByte = organism.lastPerceptTag;
  emitPerceptSignals(organism, perceptor->id, organism.lastPerceptTag, simTick);
}

}  // namespace evolab
