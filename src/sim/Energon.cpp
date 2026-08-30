#include "sim/Energon.hpp"
#include "sim/EnergonTasteSensory.hpp"
#include "sim/CellConstants.hpp"

#include "sim/BarrenWorld.hpp"
#include "sim/CellConstants.hpp"
#include "sim/Chaos.hpp"
#include "sim/EnergonInformation.hpp"
#include "sim/EnergonRain.hpp"
#include "sim/EnergonSpatialIndex.hpp"
#include "sim/EnergonString.hpp"
#include "sim/Organism.hpp"
#include "sim/TideAdvection.hpp"
#include "sim/WaterColumn.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <random>

namespace evolab {

namespace {

std::uint64_t mixSeed(std::uint64_t seed, std::uint64_t salt) {
  seed ^= salt + 0x9e3779b97f4a7c15ULL;
  seed = (seed ^ (seed >> 30)) * 0xbf58476d1ce4e5b9ULL;
  seed = (seed ^ (seed >> 27)) * 0x94d049bb133111ebULL;
  return seed ^ (seed >> 31);
}

std::uint8_t randomByteCount(std::mt19937_64& rng) {
  std::uniform_int_distribution<int> dist(1, 8);
  const int n = dist(rng);
  const int bias = dist(rng);
  std::uint8_t bytes = static_cast<std::uint8_t>(n);
  if (bias <= 5) {
    bytes = static_cast<std::uint8_t>(std::min(n, 3));
  }
  const float jittered = chaosJitterFloat(static_cast<float>(bytes), rng);
  return static_cast<std::uint8_t>(std::clamp(static_cast<int>(jittered + 0.5f), 1, 8));
}

std::uint64_t randomData(std::mt19937_64& rng, std::uint8_t byteCount) {
  std::uint8_t bytes[kEnergonMaxBytesPerBlob]{};
  for (int i = 0; i < byteCount; ++i) {
    bytes[i] = energonRandomSunfallByte(rng);
  }
  return energonPackRawBytes(bytes, byteCount);
}

bool isMouthStickyFoodBlob(const EnergonBlob& blob) {
  if (!blob.grounded || !blob.onWet || blob.remaining == 0) {
    return false;
  }
  switch (blob.origin) {
    case EnergonOrigin::Waste:
    case EnergonOrigin::Cloaca:
    case EnergonOrigin::Signal:
      return false;
    case EnergonOrigin::Sunfall:
    case EnergonOrigin::Fragment:
      return true;
  }
  return false;
}

float mouthStickyRadiusForNode(float cellSize, const SkeletonNode& mouth) {
  const float base = cellSize * kMouthStickyRadiusFactor;
  if (mouth.mouthTasteSampleValid &&
      mouth.mouthTasteSalience >= kMouthTasteSalienceFloor) {
    return std::max(base, cellSize * kMouthTasteChemotaxisStickyRadiusFactor);
  }
  return base;
}

float mouthStickyPruneRadius(float cellSize) {
  return cellSize * std::max(kMouthStickyRadiusFactor, kMouthTasteChemotaxisStickyRadiusFactor);
}

float mouthCoAdvectRadiusFactor(const Organism& organism) {
  if (organism.lastMouthHadFoodContact) {
    return kMouthChewCoAdvectRadiusFactor;
  }
  if (organism.lastMouthTasteSalience >= kMouthTasteSalienceFloor) {
    return kMouthTasteChemotaxisStickyRadiusFactor;
  }
  return kMouthContactRadiusFactor;
}

}  // namespace

int energonEvictionScore(const EnergonBlob& blob) {
  // Lower score evicts first: shed cloaca/waste before sunfall food.
  int originRank = 0;
  switch (blob.origin) {
    case EnergonOrigin::Cloaca:
      originRank = 0;
      break;
    case EnergonOrigin::Waste:
      originRank = 1;
      break;
    case EnergonOrigin::Fragment:
      originRank = 2;
      break;
    case EnergonOrigin::Signal:
      originRank = 3;
      break;
    case EnergonOrigin::Sunfall:
      originRank = 4;
      break;
  }
  return originRank * 1'000'000 + static_cast<int>(blob.ttl * 1'000.0f);
}

bool EnergonField::evictOneBlob() {
  if (blobs_.empty()) {
    return false;
  }

  std::size_t evictIndex = 0;
  int bestScore = energonEvictionScore(blobs_.front());
  if (blobs_.front().cornucopia) {
    bestScore = std::numeric_limits<int>::max();
  }
  for (std::size_t i = 1; i < blobs_.size(); ++i) {
    if (blobs_[i].cornucopia) {
      continue;
    }
    const int score = energonEvictionScore(blobs_[i]);
    if (score < bestScore) {
      bestScore = score;
      evictIndex = i;
    }
  }

  if (blobs_[evictIndex].cornucopia) {
    return false;
  }

  releaseAnchorsForBlob(blobs_[evictIndex].id);
  blobs_.erase(blobs_.begin() + static_cast<std::ptrdiff_t>(evictIndex));
  return true;
}

void EnergonField::trimToCap() {
  while (static_cast<int>(blobs_.size()) > config_.maxBlobs) {
    if (!evictOneBlob()) {
      break;
    }
  }
}

EnergonField::EnergonField(std::uint64_t seed, EnergonConfig config)
    : config_(config), seed_(seed), spatialIndex_(std::make_unique<EnergonSpatialIndex>()) {}

EnergonField::~EnergonField() = default;

void EnergonField::setSeed(std::uint64_t seed) {
  seed_ = seed;
  clear();
}

void EnergonField::clear() {
  blobs_.clear();
  mouthAnchors_.clear();
  nextId_ = 1;
}

EnergonBlob* EnergonField::findBlob(std::uint32_t blobId) {
  for (EnergonBlob& blob : blobs_) {
    if (blob.id == blobId) {
      return &blob;
    }
  }
  return nullptr;
}

const EnergonBlob* EnergonField::findBlob(std::uint32_t blobId) const {
  for (const EnergonBlob& blob : blobs_) {
    if (blob.id == blobId) {
      return &blob;
    }
  }
  return nullptr;
}

void EnergonField::releaseAnchorsForBlob(std::uint32_t blobId) {
  mouthAnchors_.erase(std::remove_if(mouthAnchors_.begin(), mouthAnchors_.end(),
                                     [blobId](const EnergonMouthAnchor& anchor) {
                                       return anchor.blobId == blobId;
                                     }),
                      mouthAnchors_.end());
}

void EnergonField::releaseMouthAnchorsExcept(std::uint32_t organismId, std::uint32_t mouthNodeId,
                                             std::uint32_t blobId) {
  mouthAnchors_.erase(
      std::remove_if(mouthAnchors_.begin(), mouthAnchors_.end(),
                     [&](const EnergonMouthAnchor& anchor) {
                       return anchor.organismId == organismId && anchor.mouthNodeId == mouthNodeId &&
                              anchor.blobId != blobId;
                     }),
      mouthAnchors_.end());
}

void EnergonField::remapAnchorsOnSnip(std::uint32_t tailBlobId, std::uint32_t headBlobId,
                                      float splitT) {
  const float clampedSplit = std::clamp(splitT, 1.0e-4f, 1.0f - 1.0e-4f);
  for (EnergonMouthAnchor& anchor : mouthAnchors_) {
    if (anchor.blobId != tailBlobId) {
      continue;
    }
    if (anchor.anchorT <= clampedSplit) {
      anchor.anchorT = anchor.anchorT / clampedSplit;
    } else {
      anchor.blobId = headBlobId;
      anchor.anchorT = (anchor.anchorT - clampedSplit) / (1.0f - clampedSplit);
    }
  }
}

bool EnergonField::blobHasMouthAnchor(std::uint32_t blobId) const {
  for (const EnergonMouthAnchor& anchor : mouthAnchors_) {
    if (anchor.blobId == blobId) {
      return true;
    }
  }
  return false;
}

void EnergonField::setMouthAnchor(std::uint32_t blobId, std::uint32_t organismId,
                                  std::uint32_t mouthNodeId, float mouthX, float mouthZ) {
  const EnergonBlob* blob = findBlob(blobId);
  if (blob == nullptr || !blob->grounded || blob->remaining == 0) {
    return;
  }

  float anchorT = 0.0f;
  energonPointSegmentDistanceSq(mouthX, mouthZ, *blob, anchorT);

  releaseMouthAnchorsExcept(organismId, mouthNodeId, blobId);

  for (EnergonMouthAnchor& anchor : mouthAnchors_) {
    if (anchor.blobId == blobId && anchor.organismId == organismId &&
        anchor.mouthNodeId == mouthNodeId) {
      anchor.anchorT = anchorT;
      return;
    }
  }

  EnergonMouthAnchor anchor;
  anchor.blobId = blobId;
  anchor.organismId = organismId;
  anchor.mouthNodeId = mouthNodeId;
  anchor.anchorT = anchorT;
  mouthAnchors_.push_back(anchor);
}

void EnergonField::applyMouthStickiness(const std::vector<Organism>& organisms, float cellSize) {
  if (cellSize <= 0.0f) {
    return;
  }

  for (const Organism& organism : organisms) {
    if (!organism.alive) {
      continue;
    }

    for (const SkeletonNode& node : organism.nodes) {
      if (!node.alive || node.neuron != NeuronType::Mouth) {
        continue;
      }

      const float stickyRadius = mouthStickyRadiusForNode(cellSize, node);
      const float stickyRadiusSq = stickyRadius * stickyRadius;
      std::uint32_t closestBlobId = 0;
      float closestDistSq = stickyRadiusSq;

      forEachBlobNear(node.worldX, node.worldZ, stickyRadius,
                      [&](const EnergonBlob& blob) {
                        if (!isMouthStickyFoodBlob(blob)) {
                          return;
                        }

                        float t = 0.0f;
                        const float distSq =
                            energonPointSegmentDistanceSq(node.worldX, node.worldZ, blob, t);
                        if (distSq > closestDistSq) {
                          return;
                        }

                        closestDistSq = distSq;
                        closestBlobId = blob.id;
                      });

      if (closestBlobId != 0) {
        setMouthAnchor(closestBlobId, organism.id, node.id, node.worldX, node.worldZ);
      }
    }
  }
}

void EnergonField::syncMouthAttachments(const std::vector<Organism>& organisms,
                                        const BarrenWorld& world, float cellSize,
                                        float heightScale) {
  if (mouthAnchors_.empty()) {
    return;
  }

  for (EnergonBlob& blob : blobs_) {
    if (!blob.grounded || blob.remaining == 0) {
      continue;
    }

    float dxSum = 0.0f;
    float dzSum = 0.0f;
    int pinCount = 0;

    for (const EnergonMouthAnchor& anchor : mouthAnchors_) {
      if (anchor.blobId != blob.id) {
        continue;
      }

      const Organism* organism = nullptr;
      for (const Organism& candidate : organisms) {
        if (candidate.id == anchor.organismId && candidate.alive) {
          organism = &candidate;
          break;
        }
      }
      if (organism == nullptr) {
        continue;
      }

      const SkeletonNode* mouth = organism->findNode(anchor.mouthNodeId);
      if (mouth == nullptr || !mouth->alive || mouth->neuron != NeuronType::Mouth) {
        continue;
      }

      const float coAdvectRadius = cellSize * mouthCoAdvectRadiusFactor(*organism);
      const float coAdvectRadiusSq = coAdvectRadius * coAdvectRadius;
      float nearestT = 0.0f;
      const float distSq = energonPointSegmentDistanceSq(mouth->worldX, mouth->worldZ, blob,
                                                         nearestT);
      if (distSq > coAdvectRadiusSq) {
        continue;
      }

      const float anchorX = energonAnchorWorldX(blob, anchor.anchorT);
      const float anchorZ = energonAnchorWorldZ(blob, anchor.anchorT);
      dxSum += mouth->worldX - anchorX;
      dzSum += mouth->worldZ - anchorZ;
      ++pinCount;
    }

    if (pinCount <= 0) {
      continue;
    }

    energonTranslateBlob(blob, dxSum / static_cast<float>(pinCount),
                         dzSum / static_cast<float>(pinCount));

    const WaterColumn column = sampleWaterColumn(world, blob.x, blob.z, cellSize, heightScale);
    blob.onWet = column.wet;
    blob.y = column.wet ? column.surfaceY + kEnergonSurfaceClearance
                        : placementY(column, NomHabitat::Benthic);
  }
}

void EnergonField::pruneMouthAnchors(const std::vector<Organism>& organisms, float cellSize) {
  const float contactRadius = mouthStickyPruneRadius(cellSize);
  const float radiusSq = contactRadius * contactRadius;
  mouthAnchors_.erase(
      std::remove_if(mouthAnchors_.begin(), mouthAnchors_.end(),
                     [&](const EnergonMouthAnchor& anchor) {
                       const EnergonBlob* blob = findBlob(anchor.blobId);
                       if (blob == nullptr || blob->remaining == 0 || !blob->grounded) {
                         return true;
                       }

                       const Organism* organism = nullptr;
                       for (const Organism& candidate : organisms) {
                         if (candidate.id == anchor.organismId && candidate.alive) {
                           organism = &candidate;
                           break;
                         }
                       }
                       if (organism == nullptr) {
                         return true;
                       }

                       const SkeletonNode* mouth = organism->findNode(anchor.mouthNodeId);
                       if (mouth == nullptr || !mouth->alive || mouth->neuron != NeuronType::Mouth) {
                         return true;
                       }

                       float t = 0.0f;
                       const float distSq =
                           energonPointSegmentDistanceSq(mouth->worldX, mouth->worldZ, *blob, t);
                       return distSq > radiusSq;
                     }),
      mouthAnchors_.end());
}

void EnergonField::injectBlob(EnergonBlob blob) {
  (void)injectBlobReturnId(std::move(blob));
}

std::uint32_t EnergonField::injectBlobReturnId(EnergonBlob blob) {
  while (static_cast<int>(blobs_.size()) >= config_.maxBlobs) {
    if (!evictOneBlob()) {
      return 0;
    }
  }
  if (blob.id == 0) {
    blob.id = nextId_++;
  }
  if (blob.headX == 0.0f && blob.headZ == 0.0f && blob.tailX == 0.0f && blob.tailZ == 0.0f) {
    energonBlobInitPoint(blob);
  }
  const std::uint32_t id = blob.id;
  blobs_.push_back(blob);
  return id;
}

void EnergonField::splitSegmentAt(EnergonBlob& blob, int index, std::uint8_t eatenByte, float splitT) {
  (void)eatenByte;
  const int tailCount = index;
  const int headCount = static_cast<int>(blob.remaining) - index - 1;

  const float splitX = blob.tailX + (blob.headX - blob.tailX) * splitT;
  const float splitZ = blob.tailZ + (blob.headZ - blob.tailZ) * splitT;

  EnergonBlob headPart;
  if (headCount > 0) {
    headPart.data = energonPackBytes(blob, index + 1, headCount);
    headPart.remaining = static_cast<std::uint16_t>(headCount);
    headPart.initialBytes = static_cast<std::uint8_t>(headCount);
    headPart.origin = EnergonOrigin::Fragment;
    headPart.x = (splitX + blob.headX) * 0.5f;
    headPart.z = (splitZ + blob.headZ) * 0.5f;
    headPart.y = blob.y;
    headPart.grounded = blob.grounded;
    headPart.onWet = blob.onWet;
    headPart.ttl = blob.ttl;
    headPart.headX = blob.headX;
    headPart.headZ = blob.headZ;
    headPart.tailX = splitX;
    headPart.tailZ = splitZ;
  }

  const std::uint32_t tailBlobId = blob.id;
  if (tailCount > 0) {
    blob.data = energonPackBytes(blob, 0, tailCount);
    blob.remaining = static_cast<std::uint16_t>(tailCount);
    blob.headX = splitX;
    blob.headZ = splitZ;
    energonBlobSyncCenter(blob);
  } else {
    blob.remaining = 0;
  }

  if (headCount > 0) {
    const std::uint32_t headBlobId = injectBlobReturnId(headPart);
    remapAnchorsOnSnip(tailBlobId, headBlobId, splitT);
  }
}

EnergonBiteResult EnergonField::biteAt(std::uint32_t blobId, float mouthX, float mouthZ) {
  EnergonBiteResult result;
  for (EnergonBlob& blob : blobs_) {
    if (blob.id != blobId) {
      continue;
    }
    if (blob.remaining == 0 || !blob.onWet || !blob.grounded) {
      return result;
    }

    if (blob.cornucopia) {
      float t = 0.0f;
      energonPointSegmentDistanceSq(mouthX, mouthZ, blob, t);
      const int index = energonByteIndexAtProjection(blob, t);
      const int biteIndex =
          std::clamp(index, 0, static_cast<int>(blob.remaining) - 1);
      result.byte = energonByteAt(blob, biteIndex);
      result.tookByte = true;
      return result;
    }

    float t = 0.0f;
    energonPointSegmentDistanceSq(mouthX, mouthZ, blob, t);
    const int index = energonByteIndexAtProjection(blob, t);

    if (index <= 0) {
      result.byte = energonByteAt(blob, 0);
      result.tookByte = true;
      blob.data >>= 8;
      --blob.remaining;
      energonShrinkTailGeometry(blob);
      return result;
    }

    if (index >= static_cast<int>(blob.remaining) - 1) {
      const int last = static_cast<int>(blob.remaining) - 1;
      result.byte = energonByteAt(blob, last);
      result.tookByte = true;
      blob.data &= ~(0xFFULL << (8 * last));
      --blob.remaining;
      energonShrinkHeadGeometry(blob);
      return result;
    }

    result.byte = energonByteAt(blob, index);
    result.tookByte = true;
    result.snipped = true;
    const float last = static_cast<float>(std::max(1, static_cast<int>(blob.remaining) - 1));
    const float splitT = static_cast<float>(index) / last;
    splitSegmentAt(blob, index, result.byte, splitT);
    return result;
  }
  return result;
}

EnergonBiteResult EnergonField::biteOneByte(std::uint32_t blobId) {
  for (const EnergonBlob& blob : blobs_) {
    if (blob.id == blobId) {
      return biteAt(blobId, blob.tailX, blob.tailZ);
    }
  }
  return {};
}

void EnergonField::purgeDepletedBlobs() {
  for (auto it = blobs_.begin(); it != blobs_.end();) {
    if (it->remaining == 0) {
      releaseAnchorsForBlob(it->id);
      it = blobs_.erase(it);
    } else {
      ++it;
    }
  }
}

void EnergonField::prepareSpatialQueries(float gridCellSize, float worldHalfExtent,
                                         const BarrenWorld& world) {
  if (!spatialIndex_) {
    spatialIndex_ = std::make_unique<EnergonSpatialIndex>();
  }
  spatialIndex_->rebuild(blobs_, gridCellSize, worldHalfExtent);

  if (!tasteSensoryGrid_) {
    tasteSensoryGrid_ = std::make_unique<EnergonTasteSensoryGrid>();
  }
  tasteSensoryGrid_->rebuild(blobs_, worldHalfExtent, kMouthTasteSensoryGridResolution, &world,
                             gridCellSize);
}

EnergonTasteSensoryPeak EnergonField::queryTasteSensoryPeak(float x, float z, float radius) const {
  if (!tasteSensoryGrid_) {
    return {};
  }
  return tasteSensoryGrid_->peakInRadius(x, z, radius);
}

void EnergonField::forEachBlobNear(float x, float z, float radius,
                                   const std::function<void(const EnergonBlob&)>& fn) const {
  if (!spatialIndex_ || !fn) {
    return;
  }
  spatialIndex_->forEachNear(x, z, radius, blobs_, fn);
}

void EnergonField::spawnSunfall(const BarrenWorld& world, float sunIntensity,
                                 float cellSize, int liveOrganismCount) {
  if (sunIntensity <= 0.0f) {
    return;
  }

  const int res = world.heightmap().resolution;
  const float half = static_cast<float>(res - 1) * cellSize * 0.5f;
  if (half <= 0.0f) {
    return;
  }

  std::mt19937_64 rng(mixSeed(seed_, world.tickCount() * 1315423911ULL + blobs_.size() ^
                                         kChaosSaltEnergonSunfall));

  float expected = 0.0f;
  const float ambientFloor = config_.spawnRateMax * sunIntensity;
  if (config_.populationScaledRain && liveOrganismCount > 0) {
    expected = expectedSunfallBlobsPerTick(liveOrganismCount, sunIntensity);
    expected = std::max(expected, ambientFloor);
  } else {
    expected = ambientFloor;
  }

  int spawnCount = static_cast<int>(expected);
  std::bernoulli_distribution frac(expected - static_cast<float>(spawnCount));
  if (frac(rng)) {
    ++spawnCount;
  }

  while (spawnCount > 0 && static_cast<int>(blobs_.size()) + spawnCount > config_.maxBlobs) {
    if (!evictOneBlob()) {
      spawnCount = std::max(0, config_.maxBlobs - static_cast<int>(blobs_.size()));
      break;
    }
  }

  spawnCount = std::min(spawnCount, config_.maxBlobs - static_cast<int>(blobs_.size()));
  if (spawnCount <= 0) {
    return;
  }

  std::uniform_real_distribution<float> posDist(-half, half);

  for (int i = 0; i < spawnCount; ++i) {
    const std::uint8_t bytes = randomByteCount(rng);
    EnergonBlob blob;
    blob.id = nextId_++;
    blob.data = randomData(rng, bytes);
    blob.remaining = bytes;
    blob.initialBytes = bytes;
    blob.origin = EnergonOrigin::Sunfall;
    blob.x = posDist(rng);
    blob.z = posDist(rng);
    std::uniform_real_distribution<float> skySpread(0.0f, 12.0f);
    blob.y = chaosJitterFloat(config_.skyY + skySpread(rng), rng);
    blob.vy = -chaosJitterFloat(config_.fallSpeed, rng);
    blob.ttl = kEnergonAirborneTtlSeconds;
    blob.grounded = false;
    blob.onWet = false;
    blobs_.push_back(blob);
  }
}

void EnergonField::updateBlob(EnergonBlob& blob, const BarrenWorld& world, float cellSize,
                               float heightScale) {
  const int res = world.heightmap().resolution;
  const float halfExtent =
      res > 1 ? static_cast<float>(res - 1) * cellSize * 0.5f : 0.0f;

  const WaterColumn column =
      sampleWaterColumn(world, blob.x, blob.z, cellSize, heightScale);
  const float landingY = column.wet ? column.surfaceY + kEnergonSurfaceClearance
                                    : placementY(column, NomHabitat::Benthic);

  if (!blob.grounded) {
    blob.y += blob.vy * (1.0f / 60.0f);
    if (blob.y <= landingY) {
      blob.y = landingY;
      blob.vy = 0.0f;
      blob.grounded = true;
      blob.onWet = column.wet;
      energonAssignGroundedTtl(blob, config_, column.wet);
      std::mt19937_64 rng(mixSeed(seed_, blob.id * 2654435761ULL));
      std::uniform_real_distribution<float> headingDist(0.0f, kTwoPi);
      energonBlobLayoutSegment(blob, cellSize, chaosJitterHeading(headingDist(rng), rng));
    }
  } else {
    blob.onWet = column.wet;
    blob.y = landingY;
    if (!blobHasMouthAnchor(blob.id)) {
      const float oldX = blob.x;
      const float oldZ = blob.z;
      const AdvectionVelocity velocity =
          shoreAdvection(world, blob.x, blob.z, cellSize, halfExtent);
      applyShoreAdvection(blob.x, blob.z, velocity, halfExtent, cellSize * 0.25f);
      const float dx = blob.x - oldX;
      const float dz = blob.z - oldZ;
      blob.headX += dx;
      blob.headZ += dz;
      blob.tailX += dx;
      blob.tailZ += dz;
    }

    const float decayPerTick =
        (column.wet ? config_.ttlWetSeconds : config_.ttlDrySeconds) * 60.0f;
    if (blob.cornucopia) {
      blob.ttl = energonWetTtlSeconds(blob, config_);
    } else {
      const float originScale = energonWetTtlScaleForBlob(blob);
      int entropySteps = std::max(1, static_cast<int>(std::lround(1.0f / originScale)));
      if (!column.wet) {
        entropySteps = std::max(1, entropySteps / 2);
      }
      if (blob.origin == EnergonOrigin::Sunfall &&
          (world.tickCount() + blob.id) % static_cast<std::uint32_t>(kSunfallEntropyPeriodTicks) !=
              0) {
        entropySteps = 0;
      }
      if (entropySteps > 0) {
        energonEntropyDecayBlob(blob, entropySteps);
      }
      if (decayPerTick > 0.0f) {
        blob.ttl -= 1.0f / decayPerTick;
      }
      if (blob.remaining > 0) {
        const float mass = energonBlobInformationMass(blob);
        blob.ttl = std::min(blob.ttl, std::max(1.0f, mass * config_.ttlWetSeconds));
      }
    }
  }
}

void EnergonField::anchorCornucopiaBlob(float x, float z, float y) {
  for (EnergonBlob& blob : blobs_) {
    if (!blob.cornucopia || blob.remaining == 0) {
      continue;
    }
    blob.x = x;
    blob.z = z;
    blob.y = y;
    blob.headX = x;
    blob.headZ = z;
    blob.tailX = x;
    blob.tailZ = z;
    return;
  }
}

void EnergonField::tick(const BarrenWorld& world, float sunIntensity, float cellSize,
                         float heightScale, int liveOrganismCount) {
  blobs_.erase(
      std::remove_if(blobs_.begin(), blobs_.end(),
                     [&](EnergonBlob& blob) {
                       updateBlob(blob, world, cellSize, heightScale);
                       if (blob.cornucopia) {
                         return blob.remaining == 0;
                       }
                       return blob.ttl <= 0.0f || blob.remaining == 0;
                     }),
      blobs_.end());
  trimToCap();

  spawnSunfall(world, sunIntensity, cellSize, liveOrganismCount);
  trimToCap();
}

}  // namespace evolab
