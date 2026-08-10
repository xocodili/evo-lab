#include "sim/Organism.hpp"

#include "sim/BarrenWorld.hpp"
#include "sim/CellConstants.hpp"
#include "sim/Chaos.hpp"
#include "sim/Energon.hpp"
#include "sim/EnergonString.hpp"
#include "sim/TideAdvection.hpp"
#include "sim/WorldConstants.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>

namespace evolab {

namespace {

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

void creditNodeStore(Organism& organism, SkeletonNode& node, EnergonField& field,
                     std::uint32_t units) {
  for (std::uint32_t i = 0; i < units; ++i) {
    if (node.store.size() < kMouthLocalStoreMaxBytes) {
      node.store.push_back(1);
      continue;
    }

    EnergonBlob fragment;
    fragment.data = 1;
    fragment.remaining = 1;
    fragment.initialBytes = 1;
    fragment.origin = EnergonOrigin::Fragment;
    fragment.x = node.worldX;
    fragment.z = node.worldZ + kWorldCellSize * kMouthContactRadiusFactor * 0.35f;
    fragment.y = node.worldY;
    fragment.grounded = true;
    fragment.onWet = true;
    fragment.ttl = field.config().ttlWetSeconds;
    field.injectBlob(fragment);
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

  for (const EnergonBlob& blob : field.blobs()) {
    if (!blobInRange(blob, wx, wz, radius)) {
      continue;
    }
    const float dx = blob.x - wx;
    const float dz = blob.z - wz;
    const float distSq = dx * dx + dz * dz;
    if (distSq <= bestDistSq) {
      bestDistSq = distSq;
      best.kind = MouthContactKind::Food;
      best.blobId = blob.id;
    }
  }

  if (best.kind == MouthContactKind::Food) {
    return best;
  }

  for (const EnergonBlob& blob : field.blobs()) {
    if (!emptyBlobInRange(blob, wx, wz, radius)) {
      continue;
    }
    const float dx = blob.x - wx;
    const float dz = blob.z - wz;
    const float distSq = dx * dx + dz * dz;
    if (distSq <= bestDistSq) {
      bestDistSq = distSq;
      best.kind = MouthContactKind::EmptyString;
      best.blobId = blob.id;
    }
  }

  return best;
}

void tickMouthNode(Organism& organism, SkeletonNode& node, EnergonField& field, float radius) {
  if (!organism.alive || node.neuron != NeuronType::Mouth) {
    return;
  }

  const MouthContact contact = findMouthContact(field, node.worldX, node.worldZ, radius);
  if (contact.kind == MouthContactKind::None) {
    return;
  }

  if (contact.kind == MouthContactKind::EmptyString) {
    consumeBytes(organism.bodyStorage, kBiteCost);
    if (organism.bodyStorage.empty()) {
      organism.alive = false;
    }
    return;
  }

  const auto bite = field.biteAt(contact.blobId, node.worldX, node.worldZ);
  if (!bite.tookByte) {
    consumeBytes(organism.bodyStorage, kBiteCost);
    if (organism.bodyStorage.empty()) {
      organism.alive = false;
    }
    return;
  }

  const std::uint32_t gross = kEnergonUnitsPerByte;
  const std::uint32_t net = gross > kBiteCost ? gross - kBiteCost : 0u;
  creditNodeStore(organism, node, field, net);
  node.ateThisTick = true;
  node.lastEmittedByte = bite.byte;
}

float surfaceY(const BarrenWorld& world, float wx, float wz, float cellSize, float heightScale,
               bool& wet) {
  const float terrainHeight = world.heightAtWorld(wx, wz, cellSize);
  const float terrainY = terrainHeight * heightScale;
  const float localWaterLevel = world.effectiveWaterLevelAt(wx, wz, cellSize);
  const float waterY = localWaterLevel * heightScale;
  wet = terrainHeight < localWaterLevel;
  if (wet) {
    return std::max(terrainY, waterY);
  }
  return terrainY;
}

void consumeBasalCost(Organism& organism) {
  std::uint32_t cost = kStemCellBasalCostPerTick;
  for (SkeletonNode& node : organism.nodes) {
    while (cost > 0 && !node.store.empty()) {
      node.store.pop_back();
      --cost;
    }
  }
  if (cost > 0) {
    consumeBytes(organism.bodyStorage, cost);
  }
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

  for (const EnergonBlob& blob : field.blobs()) {
    if (!blob.grounded || !blob.onWet || blob.remaining == 0) {
      continue;
    }

    const float spanX = std::abs(blob.headX - blob.tailX);
    const float spanZ = std::abs(blob.headZ - blob.tailZ);
    const float span = std::max(spanX, spanZ);
    const float dx = blob.x - wx;
    const float dz = blob.z - wz;
    const float rejectRadius = maxRadius + span;
    if (dx * dx + dz * dz > rejectRadius * rejectRadius) {
      continue;
    }

    float t = 0.0f;
    const float distSq = energonPointSegmentDistanceSq(wx, wz, blob, t);
    if (distSq >= bestDistSq) {
      continue;
    }

    bestDistSq = distSq;
    const float closestX = blob.tailX + t * (blob.headX - blob.tailX);
    const float closestZ = blob.tailZ + t * (blob.headZ - blob.tailZ);
    cue.found = true;
    cue.heading = std::atan2(closestX - wx, closestZ - wz);
    cue.proximity = 1.0f - std::sqrt(distSq) / maxRadius;
  }

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
    for (const EnergonBlob& blob : field.blobs()) {
      if (!blob.grounded || !blob.onWet || blob.remaining == 0) {
        continue;
      }
      float t = 0.0f;
      const float distSq = energonPointSegmentDistanceSq(probe.x, probe.z, blob, t);
      if (distSq >= bestDistSq) {
        continue;
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
    }
  }

  if (!best.found) {
    return best;
  }

  if (bestDistSq < minHeadingDistSq) {
    // Sitting on food — use string axis so we still slew along the strand.
    for (const EnergonBlob& blob : field.blobs()) {
      if (!blob.grounded || !blob.onWet || blob.remaining == 0) {
        continue;
      }
      float t = 0.0f;
      if (energonPointSegmentDistanceSq(rootX, rootZ, blob, t) > biteRadiusSq) {
        continue;
      }
      const float sx = blob.headX - blob.tailX;
      const float sz = blob.headZ - blob.tailZ;
      if (sx * sx + sz * sz > 1.0e-4f) {
        best.heading = std::atan2(sx, sz);
        return best;
      }
    }
    best.found = false;
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

void transferNeuralEnergy(Organism& organism) {
  for (NeuralAxon& axon : organism.neuralAxons) {
    if (axon.trustFeed < kNeuralAxonMinTrustFeed) {
      continue;
    }
    SkeletonNode* src = organism.findNode(axon.srcNodeId);
    SkeletonNode* dst = organism.findNode(axon.dstNodeId);
    if (src == nullptr || dst == nullptr) {
      continue;
    }
    if (src->store.size() < kNeuralAxonShareMinStoreBytes) {
      continue;
    }
    if (dst->store.size() >= kMouthLocalStoreMaxBytes) {
      continue;
    }

    const float feedScale = axonTrustScale(axon.trustFeed) * axon.etaEnergy;
    if (feedScale < 0.05f) {
      continue;
    }

    dst->store.push_back(src->store.back());
    src->store.pop_back();
  }
}

}  // namespace

SkeletonNode* Organism::findNode(std::uint32_t nodeId) {
  for (SkeletonNode& node : nodes) {
    if (node.id == nodeId) {
      return &node;
    }
  }
  return nullptr;
}

const SkeletonNode* Organism::findNode(std::uint32_t nodeId) const {
  for (const SkeletonNode& node : nodes) {
    if (node.id == nodeId) {
      return &node;
    }
  }
  return nullptr;
}

NeuralAxon* Organism::findNeuralAxon(std::uint32_t srcNodeId, std::uint32_t dstNodeId) {
  for (NeuralAxon& axon : neuralAxons) {
    if (axon.srcNodeId == srcNodeId && axon.dstNodeId == dstNodeId) {
      return &axon;
    }
  }
  return nullptr;
}

const NeuralAxon* Organism::findNeuralAxon(std::uint32_t srcNodeId,
                                             std::uint32_t dstNodeId) const {
  for (const NeuralAxon& axon : neuralAxons) {
    if (axon.srcNodeId == srcNodeId && axon.dstNodeId == dstNodeId) {
      return &axon;
    }
  }
  return nullptr;
}

float Organism::rootWorldX() const {
  if (const SkeletonNode* root = findNode(rootNodeId)) {
    return root->worldX;
  }
  return 0.0f;
}

float Organism::rootWorldY() const {
  if (const SkeletonNode* root = findNode(rootNodeId)) {
    return root->worldY;
  }
  return 0.0f;
}

float Organism::rootWorldZ() const {
  if (const SkeletonNode* root = findNode(rootNodeId)) {
    return root->worldZ;
  }
  return 0.0f;
}

void Organism::updateKinematics(const BarrenWorld& world, float cellSize, float heightScale) {
  SkeletonNode* root = findNode(rootNodeId);
  if (root == nullptr) {
    return;
  }

  bool wet = false;
  root->worldY = surfaceY(world, root->worldX, root->worldZ, cellSize, heightScale, wet) + 0.12f;

  std::vector<bool> placed(nodes.size(), false);
  auto nodeIndex = [this](std::uint32_t id) -> std::size_t {
    for (std::size_t i = 0; i < nodes.size(); ++i) {
      if (nodes[i].id == id) {
        return i;
      }
    }
    return nodes.size();
  };
  placed[nodeIndex(rootNodeId)] = true;

  bool progress = true;
  while (progress) {
    progress = false;
    for (const SkeletonLink& link : links) {
      const std::size_t parentIdx = nodeIndex(link.parentNodeId);
      const std::size_t childIdx = nodeIndex(link.childNodeId);
      if (parentIdx >= nodes.size() || childIdx >= nodes.size()) {
        continue;
      }
      if (!placed[parentIdx] || placed[childIdx]) {
        continue;
      }

      const SkeletonNode& parent = nodes[parentIdx];
      SkeletonNode& child = nodes[childIdx];
      const float angle = link.jointAngle + heading;
      child.worldX = parent.worldX + std::sin(angle) * link.restLength;
      child.worldZ = parent.worldZ + std::cos(angle) * link.restLength;
      child.worldY =
          surfaceY(world, child.worldX, child.worldZ, cellSize, heightScale, wet) + 0.12f;
      placed[childIdx] = true;
      progress = true;
    }
  }
}

void Organism::advectRoot(const BarrenWorld& world, const EnergonField& energon, float cellSize,
                          float heightScale, float halfExtent) {
  if (!alive) {
    return;
  }
  SkeletonNode* root = findNode(rootNodeId);
  if (root == nullptr) {
    return;
  }

  const AdvectionVelocity velocity =
      shoreAdvection(world, root->worldX, root->worldZ, cellSize, halfExtent);
  updateOrganismHeading(*this, velocity, energon, cellSize);
  applyShoreAdvection(root->worldX, root->worldZ, velocity, halfExtent, cellSize * 0.25f);
  updateKinematics(world, cellSize, heightScale);
  landAdjacent = organismLandAdjacent(world, root->worldX, root->worldZ, cellSize);
}

void Organism::metabolise(const BarrenWorld& world, float cellSize, float heightScale) {
  if (!alive) {
    return;
  }

  updateKinematics(world, cellSize, heightScale);

  if (bodyStorage.empty() && allLocalStoresEmpty()) {
    alive = false;
    return;
  }

  consumeBasalCost(*this);

  if (bodyStorage.empty() && allLocalStoresEmpty()) {
    alive = false;
  }
}

void Organism::feed(EnergonField& field, float cellSize) {
  if (!alive) {
    return;
  }

  for (SkeletonNode& node : nodes) {
    node.ateThisTick = false;
  }

  const float radius = cellSize * kMouthContactRadiusFactor;
  for (SkeletonNode& node : nodes) {
    if (node.neuron == NeuronType::Mouth) {
      tickMouthNode(*this, node, field, radius);
    }
  }
}

void Organism::transferEnergy(EnergonField& field, float cellSize) {
  (void)field;
  (void)cellSize;
  if (!alive) {
    return;
  }

  if (hasNeuralAxons()) {
    transferNeuralEnergy(*this);
    return;
  }

  SkeletonNode* root = findNode(rootNodeId);
  if (root == nullptr) {
    return;
  }

  for (const SkeletonLink& link : links) {
    if (link.energyEta <= 0.0f) {
      continue;
    }
    SkeletonNode* child = findNode(link.childNodeId);
    SkeletonNode* parent = findNode(link.parentNodeId);
    if (child == nullptr || parent == nullptr || child->store.empty()) {
      continue;
    }

    const std::size_t moveCount =
        std::max<std::size_t>(1, static_cast<std::size_t>(static_cast<float>(child->store.size()) *
                                                          link.energyEta));
    const std::size_t actualMove = std::min(moveCount, child->store.size());

    if (parent->id == rootNodeId) {
      for (std::size_t i = 0; i < actualMove; ++i) {
        if (bodyStorage.size() >= kStemCellStorageMaxBytes) {
          break;
        }
        bodyStorage.push_back(child->store.back());
        child->store.pop_back();
      }
    } else {
      for (std::size_t i = 0; i < actualMove; ++i) {
        if (parent->store.size() >= kMouthLocalStoreMaxBytes) {
          break;
        }
        parent->store.push_back(child->store.back());
        child->store.pop_back();
      }
    }
  }
}

void Organism::signal(std::uint64_t simTick) {
  if (!alive || neuralAxons.empty()) {
    return;
  }

  for (NeuralAxon& axon : neuralAxons) {
    axon.pendingSend.valid = false;
  }

  for (SkeletonNode& node : nodes) {
    if (node.neuron != NeuronType::Mouth) {
      continue;
    }

    std::uint8_t byte = 0;
    bool emit = false;
    if (node.ateThisTick) {
      byte = node.lastEmittedByte;
      emit = true;
    } else if (!node.store.empty()) {
      byte = node.store.back();
      emit = (simTick % 30 == 0);
    }
    if (!emit) {
      continue;
    }

    for (NeuralAxon& axon : neuralAxons) {
      if (axon.srcNodeId != node.id) {
        continue;
      }
      axon.pendingSend.byte = byte;
      axon.pendingSend.valid = true;
      axon.pendingSend.tick = simTick;
      axon.lastSentByte = byte;
      axon.lastReceived = axon.pendingSend;
    }
  }
}

void Organism::transferColony() {}

int Organism::mouthCount() const {
  int count = 0;
  for (const SkeletonNode& node : nodes) {
    if (node.neuron == NeuronType::Mouth) {
      ++count;
    }
  }
  return count;
}

bool Organism::hasMouthNeurons() const {
  return mouthCount() > 0;
}

bool Organism::hasNeuralAxons() const {
  return !neuralAxons.empty();
}

void Organism::finalizeSpawn(std::mt19937& rng) {
  for (NeuralAxon& axon : neuralAxons) {
    initializeDevelopmentalAxonTrust(axon, rng);
  }

  heading = chaosJitterHeading(heading, rng);

  for (SkeletonLink& link : links) {
    link.restLength = chaosJitterFloat(link.restLength, rng);
    link.jointAngle = chaosJitterFloat(link.jointAngle, rng);
    link.energyEta = chaosJitterFloat(link.energyEta, rng);
  }
}

void Organism::pruneNeuralAxons() {
  neuralAxons.erase(std::remove_if(neuralAxons.begin(), neuralAxons.end(), axonMarkedForPruning),
                    neuralAxons.end());
}

bool Organism::allLocalStoresEmpty() const {
  for (const SkeletonNode& node : nodes) {
    if (!node.store.empty()) {
      return false;
    }
  }
  return true;
}

Organism makeUndifferentiatedOrganism(std::uint32_t id, float wx, float wz, float wy,
                                      std::size_t storageBytes, std::uint64_t createdAtTick) {
  Organism organism;
  organism.id = id;
  organism.createdAtTick = createdAtTick;
  organism.rootNodeId = 1;
  organism.bodyStorage.resize(storageBytes);

  SkeletonNode root;
  root.id = 1;
  root.neuron = NeuronType::None;
  root.worldX = wx;
  root.worldZ = wz;
  root.worldY = wy;
  organism.nodes.push_back(root);
  return organism;
}

Organism makeStarMouthOrganism(std::uint32_t id, float wx, float wz, float wy,
                               std::size_t storageBytes, std::uint64_t createdAtTick,
                               int mouthCount, float boneLength) {
  Organism organism;
  organism.id = id;
  organism.createdAtTick = createdAtTick;
  organism.bodyStorage.resize(storageBytes);

  SkeletonNode root;
  root.id = 1;
  root.neuron = NeuronType::Computer;
  root.worldX = wx;
  root.worldZ = wz;
  root.worldY = wy;
  organism.rootNodeId = root.id;
  organism.nodes.push_back(root);

  const int spokes = std::max(1, mouthCount);
  for (int i = 0; i < spokes; ++i) {
    const float angle = static_cast<float>(i) * 6.2831853f / static_cast<float>(spokes);
    SkeletonNode mouth;
    mouth.id = static_cast<std::uint32_t>(2 + i);
    mouth.neuron = NeuronType::Mouth;

    SkeletonLink link;
    link.parentNodeId = root.id;
    link.childNodeId = mouth.id;
    link.restLength = boneLength;
    link.jointAngle = angle;
    link.energyEta = 0.88f;

    organism.nodes.push_back(mouth);
    organism.links.push_back(link);
  }

  return organism;
}

Organism makeTwoMouthOrganism(std::uint32_t id, float wx, float wz, float wy,
                              std::size_t storageBytes, std::uint64_t createdAtTick,
                              float boneLength, std::uint16_t trustFeedM1ToM2,
                              std::uint16_t trustFeedM2ToM1) {
  Organism organism;
  organism.id = id;
  organism.createdAtTick = createdAtTick;
  organism.bodyStorage.resize(storageBytes);
  organism.rootNodeId = 1;

  SkeletonNode mouthA;
  mouthA.id = 1;
  mouthA.neuron = NeuronType::Mouth;
  mouthA.worldX = wx;
  mouthA.worldZ = wz;
  mouthA.worldY = wy;

  SkeletonNode mouthB;
  mouthB.id = 2;
  mouthB.neuron = NeuronType::Mouth;
  mouthB.worldX = wx;
  mouthB.worldZ = wz + boneLength;
  mouthB.worldY = wy;

  organism.nodes.push_back(mouthA);
  organism.nodes.push_back(mouthB);

  SkeletonLink bone;
  bone.parentNodeId = 1;
  bone.childNodeId = 2;
  bone.restLength = boneLength;
  bone.jointAngle = 0.0f;
  bone.energyEta = 0.0f;
  organism.links.push_back(bone);

  NeuralAxon axonAtoB;
  axonAtoB.srcNodeId = 1;
  axonAtoB.dstNodeId = 2;
  axonAtoB.trustBelieve = kTrustBaseline;
  axonAtoB.trustFeed = trustFeedM1ToM2;

  NeuralAxon axonBtoA;
  axonBtoA.srcNodeId = 2;
  axonBtoA.dstNodeId = 1;
  axonBtoA.trustBelieve = kTrustBaseline;
  axonBtoA.trustFeed = trustFeedM2ToM1;

  organism.neuralAxons.push_back(axonAtoB);
  organism.neuralAxons.push_back(axonBtoA);

  return organism;
}

bool organismLandAdjacent(const BarrenWorld& world, float wx, float wz, float cellSize) {
  const float terrainHeight = world.heightAtWorld(wx, wz, cellSize);
  const float waterLevel = world.effectiveWaterLevelAt(wx, wz, cellSize);
  if (terrainHeight >= waterLevel) {
    return true;
  }
  const float eps = 0.05f;
  return terrainHeight >= waterLevel - eps;
}

namespace {

int trustDisplayPercent(std::uint16_t trust) {
  return static_cast<int>((static_cast<unsigned>(trust) * 100u + kTrustBaseline / 2u) /
                          kTrustBaseline);
}

}  // namespace

std::string Organism::architectureLabel() const {
  char buffer[640];
  const float daysRemaining =
      static_cast<float>(bodyStorage.size()) / static_cast<float>(kTicksPerStemCellDay);

  if (!hasMouthNeurons()) {
    std::snprintf(buffer, sizeof(buffer),
                  "StemCell #%u\n"
                  "Type: undifferentiated\n"
                  "Nodes: 1  Links: 0\n"
                  "Storage: %zu bytes (%.2f d)\n"
                  "Land-adjacent: %s\n"
                  "Created: tick %llu\n"
                  "Status: %s",
                  id, bodyStorage.size(), daysRemaining, landAdjacent ? "yes" : "no",
                  static_cast<unsigned long long>(createdAtTick), alive ? "alive" : "dead");
    return buffer;
  }

  std::size_t localBytes = 0;
  for (const SkeletonNode& node : nodes) {
    localBytes += node.store.size();
  }

  if (hasNeuralAxons() && mouthCount() == 2 && nodes.size() == 2) {
    const NeuralAxon* axon12 = findNeuralAxon(1, 2);
    const NeuralAxon* axon21 = findNeuralAxon(2, 1);
    const char* recv12 =
        axon12 != nullptr && axon12->lastReceived.valid
            ? "0x"
            : "—";
    char recv12Byte[8] = "—";
    if (axon12 != nullptr && axon12->lastReceived.valid) {
      std::snprintf(recv12Byte, sizeof(recv12Byte), "0x%02X", axon12->lastReceived.byte);
    }
    char recv21Byte[8] = "—";
    if (axon21 != nullptr && axon21->lastReceived.valid) {
      std::snprintf(recv21Byte, sizeof(recv21Byte), "0x%02X", axon21->lastReceived.byte);
    }
    (void)recv12;
    std::snprintf(
        buffer, sizeof(buffer),
        "Organism #%u\n"
        "Type: twin mouth (2 M, 2 axons)\n"
        "Nodes: 2  Bone: 1  Heading: %.0f deg\n"
        "Body: %zu bytes (%.2f d)  Node stores: %zu\n"
        "Axon M1→M2 feed:%d%% believe:%d%% last:0x%02X recv:%s\n"
        "Axon M2→M1 feed:%d%% believe:%d%% last:0x%02X recv:%s\n"
        "Land-adjacent: %s  tick %llu  %s",
        id, heading * 180.0f / 3.14159265f, bodyStorage.size(), daysRemaining, localBytes,
        axon12 != nullptr ? trustDisplayPercent(axon12->trustFeed) : 0,
        axon12 != nullptr ? trustDisplayPercent(axon12->trustBelieve) : 0,
        axon12 != nullptr ? axon12->lastSentByte : 0, recv12Byte,
        axon21 != nullptr ? trustDisplayPercent(axon21->trustFeed) : 0,
        axon21 != nullptr ? trustDisplayPercent(axon21->trustBelieve) : 0,
        axon21 != nullptr ? axon21->lastSentByte : 0, recv21Byte, landAdjacent ? "yes" : "no",
        static_cast<unsigned long long>(createdAtTick), alive ? "alive" : "dead");
    return buffer;
  }

  std::snprintf(buffer, sizeof(buffer),
                "Organism #%u\n"
                "Type: kinetic mouth (%d M)\n"
                "Nodes: %zu  Links: %zu  Heading: %.0f deg\n"
                "Body storage: %zu bytes (%.2f d)\n"
                "Node stores: %zu bytes\n"
                "Land-adjacent: %s\n"
                "Created: tick %llu\n"
                "Status: %s",
                id, mouthCount(), nodes.size(), links.size(),
                heading * 180.0f / 3.14159265f, bodyStorage.size(), daysRemaining,
                localBytes, landAdjacent ? "yes" : "no",
                static_cast<unsigned long long>(createdAtTick), alive ? "alive" : "dead");
  return buffer;
}

std::string Organism::hoverSummary() const {
  char buffer[160];
  if (!hasMouthNeurons()) {
    std::snprintf(buffer, sizeof(buffer), "Hover: StemCell #%u (undifferentiated)", id);
  } else if (hasNeuralAxons() && mouthCount() == 2) {
    std::snprintf(buffer, sizeof(buffer), "Hover: Organism #%u twin mouth (2 axons)", id);
  } else {
    std::snprintf(buffer, sizeof(buffer),
                  "Hover: Organism #%u kinetic mouth (%d M, %zu links)", id, mouthCount(),
                  links.size());
  }
  return buffer;
}

}  // namespace evolab
