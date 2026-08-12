#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <vector>

namespace evolab {

class BarrenWorld;
class EnergonSpatialIndex;

enum class EnergonOrigin : std::uint8_t { Sunfall = 0, Signal = 1, Fragment = 2 };

struct EnergonBlob {
  std::uint32_t id = 0;
  std::uint64_t data = 0;
  std::uint16_t remaining = 0;
  std::uint8_t initialBytes = 0;
  EnergonOrigin origin = EnergonOrigin::Sunfall;
  float x = 0.0f;
  float y = 0.0f;
  float z = 0.0f;
  float headX = 0.0f;
  float headZ = 0.0f;
  float tailX = 0.0f;
  float tailZ = 0.0f;
  float vy = 0.0f;
  float ttl = 0.0f;
  bool grounded = false;
  bool onWet = false;
};

struct EnergonConfig {
  float skyY = 58.0f;
  float fallSpeed = 42.0f;
  float spawnRateMax = 6.0f;
  float ttlWetSeconds = 50.0f;
  float ttlDrySeconds = 6.0f;
  int maxBlobs = 1800;
  float worldHalfExtent = 0.0f;
};

struct EnergonBiteResult {
  bool tookByte = false;
  bool snipped = false;
  std::uint8_t byte = 0;
};

class EnergonField {
public:
  explicit EnergonField(std::uint64_t seed, EnergonConfig config = {});
  ~EnergonField();

  EnergonField(const EnergonField&) = delete;
  EnergonField& operator=(const EnergonField&) = delete;

  void setSeed(std::uint64_t seed);
  void clear();
  void tick(const BarrenWorld& world, float sunIntensity, float cellSize, float heightScale);

  const std::vector<EnergonBlob>& blobs() const { return blobs_; }
  const EnergonConfig& config() const { return config_; }
  int activeCount() const { return static_cast<int>(blobs_.size()); }

  void injectBlob(EnergonBlob blob);

  // Bite one byte at the mouth position along the string (may snip into two blobs).
  EnergonBiteResult biteAt(std::uint32_t blobId, float mouthX, float mouthZ);

  // Legacy helper: bites from the tail contact point.
  EnergonBiteResult biteOneByte(std::uint32_t blobId);

  void purgeDepletedBlobs();

  // Rebuild the spatial index before feed/perceive queries for the current tick.
  void prepareSpatialQueries(float gridCellSize, float worldHalfExtent);

  void forEachBlobNear(float x, float z, float radius,
                       const std::function<void(const EnergonBlob&)>& fn) const;

private:
  void spawnSunfall(const BarrenWorld& world, float sunIntensity, float cellSize);
  void updateBlob(EnergonBlob& blob, const BarrenWorld& world, float cellSize, float heightScale);

  EnergonConfig config_;
  std::uint64_t seed_ = 0;
  std::uint32_t nextId_ = 1;
  std::vector<EnergonBlob> blobs_;
  std::unique_ptr<EnergonSpatialIndex> spatialIndex_;
};

}  // namespace evolab
