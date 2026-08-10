#include "sim/Energon.hpp"

#include "sim/BarrenWorld.hpp"
#include "sim/Chaos.hpp"
#include "sim/EnergonString.hpp"
#include "sim/TideAdvection.hpp"

#include <algorithm>
#include <cmath>
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
  std::uint64_t data = 0;
  for (int i = 0; i < byteCount; ++i) {
    std::uniform_int_distribution<int> dist(0, 255);
    data |= static_cast<std::uint64_t>(dist(rng)) << (8 * i);
  }
  return data;
}

}  // namespace

EnergonField::EnergonField(std::uint64_t seed, EnergonConfig config)
    : config_(config), seed_(seed) {}

void EnergonField::setSeed(std::uint64_t seed) {
  seed_ = seed;
  clear();
}

void EnergonField::clear() {
  blobs_.clear();
  nextId_ = 1;
}

void EnergonField::injectBlob(EnergonBlob blob) {
  if (blob.id == 0) {
    blob.id = nextId_++;
  }
  if (blob.headX == 0.0f && blob.headZ == 0.0f && blob.tailX == 0.0f && blob.tailZ == 0.0f) {
    energonBlobInitPoint(blob);
  }
  blobs_.push_back(blob);
}

namespace {

void splitSegmentAt(EnergonBlob& blob, int index, EnergonField& field, std::uint8_t eatenByte) {
  (void)eatenByte;
  const int tailCount = index;
  const int headCount = static_cast<int>(blob.remaining) - index - 1;

  const float last = static_cast<float>(std::max(1, static_cast<int>(blob.remaining) - 1));
  const float splitT = static_cast<float>(index) / last;
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
    field.injectBlob(headPart);
  }
}

}  // namespace

EnergonBiteResult EnergonField::biteAt(std::uint32_t blobId, float mouthX, float mouthZ) {
  EnergonBiteResult result;
  for (EnergonBlob& blob : blobs_) {
    if (blob.id != blobId) {
      continue;
    }
    if (blob.remaining == 0 || !blob.onWet || !blob.grounded) {
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
    splitSegmentAt(blob, index, *this, result.byte);
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
  blobs_.erase(std::remove_if(blobs_.begin(), blobs_.end(),
                              [](const EnergonBlob& blob) { return blob.remaining == 0; }),
                blobs_.end());
}

float EnergonField::surfaceY(const BarrenWorld& world, float wx, float wz, float cellSize,
                              float heightScale, bool& wet) const {
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

void EnergonField::spawnSunfall(const BarrenWorld& world, float sunIntensity,
                                 float cellSize) {
  if (sunIntensity <= 0.0f || static_cast<int>(blobs_.size()) >= config_.maxBlobs) {
    return;
  }

  const int res = world.heightmap().resolution;
  const float half = static_cast<float>(res - 1) * cellSize * 0.5f;
  if (half <= 0.0f) {
    return;
  }

  std::mt19937_64 rng(mixSeed(seed_, world.tickCount() * 1315423911ULL + blobs_.size() ^
                                         kChaosSaltEnergonSunfall));

  const float expected = config_.spawnRateMax * sunIntensity;
  int spawnCount = static_cast<int>(expected);
  std::bernoulli_distribution frac(expected - static_cast<float>(spawnCount));
  if (frac(rng)) {
    ++spawnCount;
  }

  spawnCount = std::min(spawnCount, config_.maxBlobs - static_cast<int>(blobs_.size()));
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
    blob.ttl = config_.ttlWetSeconds;
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

  bool wet = false;
  const float surface = surfaceY(world, blob.x, blob.z, cellSize, heightScale, wet);

  if (!blob.grounded) {
    blob.y += blob.vy * (1.0f / 60.0f);
    if (blob.y <= surface) {
      blob.y = surface + 0.05f;
      blob.vy = 0.0f;
      blob.grounded = true;
      blob.onWet = wet;
      blob.ttl = wet ? config_.ttlWetSeconds : config_.ttlDrySeconds;
      std::mt19937_64 rng(mixSeed(seed_, blob.id * 2654435761ULL));
      std::uniform_real_distribution<float> headingDist(0.0f, kTwoPi);
      energonBlobLayoutSegment(blob, cellSize, chaosJitterHeading(headingDist(rng), rng));
    }
  } else {
    blob.onWet = wet;
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

    const float decayPerTick =
        (wet ? config_.ttlWetSeconds : config_.ttlDrySeconds) * 60.0f;
    if (decayPerTick > 0.0f) {
      blob.ttl -= 1.0f / decayPerTick;
    }
  }
}

void EnergonField::tick(const BarrenWorld& world, float sunIntensity, float cellSize,
                         float heightScale) {
  spawnSunfall(world, sunIntensity, cellSize);

  blobs_.erase(
      std::remove_if(blobs_.begin(), blobs_.end(),
                     [&](EnergonBlob& blob) {
                       updateBlob(blob, world, cellSize, heightScale);
                       return blob.ttl <= 0.0f || blob.remaining == 0;
                     }),
      blobs_.end());
}

}  // namespace evolab
