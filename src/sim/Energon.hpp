#pragma once



#include "sim/EnergonRain.hpp"

#include <cstdint>

#include <functional>

#include <memory>

#include <vector>



namespace evolab {



class BarrenWorld;
class EnergonSpatialIndex;
class EnergonTasteSensoryGrid;
class Organism;

struct EnergonTasteSensoryPeak;



enum class EnergonOrigin : std::uint8_t {
  Sunfall = 0,
  Signal = 1,
  Fragment = 2,
  Waste = 3,
  Cloaca = 4
};



struct EnergonBlob {

  std::uint32_t id = 0;

  std::uint8_t bytes[kEnergonMaxBytesPerBlob]{};

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

  // Benchmark / oracle food: infinite bytes, no TTL decay (see test_cornucopia_chemotaxis).
  bool cornucopia = false;

};



struct EnergonConfig {

  float skyY = 58.0f;

  float fallSpeed = 42.0f;

  float spawnRateMax = 10.0f;
  // When true, sunfall scales as f(liveOrganisms) × entropy each rain cycle.
  // spawnRateMax is also a per-tick floor at full sun (ambient rain minimum).
  bool populationScaledRain = true;
  // Rain budget and spawn-gate quota never drop below this population (seed count).
  // Live count above baseline scales rain up; attrition does not shrink the field.
  int rainPopulationBaseline = 0;

  float ttlWetSeconds = 50.0f;

  float ttlDrySeconds = 6.0f;

  int maxBlobs = 4000;

  float worldHalfExtent = 0.0f;

};



struct EnergonBiteResult {

  bool tookByte = false;

  bool snipped = false;

  std::uint8_t byteCount = 0;

  std::uint8_t bytes[kEnergonMaxBytesPerBlob]{};

  // First field byte of the chomp (legacy single-byte callers).
  std::uint8_t byte = 0;

};



struct EnergonMouthAnchor {

  std::uint32_t blobId = 0;

  std::uint32_t organismId = 0;

  std::uint32_t mouthNodeId = 0;

  // Parametric position tail→head where the string is pinned to the mouth.

  float anchorT = 0.0f;

};



class EnergonField {

public:

  explicit EnergonField(std::uint64_t seed, EnergonConfig config = {});

  ~EnergonField();



  EnergonField(const EnergonField&) = delete;

  EnergonField& operator=(const EnergonField&) = delete;



  void setSeed(std::uint64_t seed);

  void clear();

  void tick(const BarrenWorld& world, float sunIntensity, float cellSize, float heightScale,
            int liveOrganismCount = -1);



  const std::vector<EnergonBlob>& blobs() const { return blobs_; }

  const EnergonConfig& config() const { return config_; }

  int activeCount() const { return static_cast<int>(blobs_.size()); }



  void injectBlob(EnergonBlob blob);

  std::uint32_t injectBlobReturnId(EnergonBlob blob);



  // Bite up to kChompFieldBytes at the mouth projection along the string (may snip into two blobs).

  EnergonBiteResult biteAt(std::uint32_t blobId, float mouthX, float mouthZ);



  // Pin a grounded string to a mouth at the nearest point (consumption/contact).

  void setMouthAnchor(std::uint32_t blobId, std::uint32_t organismId, std::uint32_t mouthNodeId,

                      float mouthX, float mouthZ);



  // Move mouth-anchored strings after organism advection; skips tide drift while anchored.

  void syncMouthAttachments(const std::vector<Organism>& organisms, const BarrenWorld& world,

                            float cellSize, float heightScale);



  // Anchor grounded wet food strings; reach scales with M taste homing (cellSize).

  void applyMouthStickiness(const std::vector<Organism>& organisms, float cellSize);



  void pruneMouthAnchors(const std::vector<Organism>& organisms, float cellSize);



  bool blobHasMouthAnchor(std::uint32_t blobId) const;



  const std::vector<EnergonMouthAnchor>& mouthAnchors() const { return mouthAnchors_; }



  // Legacy helper: bites from the tail contact point.

  EnergonBiteResult biteOneByte(std::uint32_t blobId);



  void purgeDepletedBlobs();



  // Rebuild the spatial index before feed/perceive queries for the current tick.

  void prepareSpatialQueries(float gridCellSize, float worldHalfExtent,
                             const BarrenWorld& world);



  void forEachBlobNear(float x, float z, float radius,

                       const std::function<void(const EnergonBlob&)>& fn) const;

  // Coarse zoomed-out taste map rebuilt with prepareSpatialQueries; mouth steers to peak cell.
  EnergonTasteSensoryPeak queryTasteSensoryPeak(float x, float z, float radius) const;
  float queryTasteResultantMagSq(float x, float z, float radius) const;
  float queryTasteCellBytes(float worldX, float worldZ) const;

  // Nursery / oracle: keep the eternal cornucopia blob glued to a world anchor (e.g. mouth).
  void anchorCornucopiaBlob(float x, float z, float y);

  const EnergonSunfallTickStats& lastSunfallTickStats() const { return lastSunfallStats_; }
  std::uint64_t cumulativeSunfallSpawns() const { return cumulativeSunfallSpawns_; }
  void resetSunfallTelemetry();



private:

  void spawnSunfall(const BarrenWorld& world, float sunIntensity, float cellSize,
                    int liveOrganismCount);

  void updateBlob(EnergonBlob& blob, const BarrenWorld& world, float cellSize, float heightScale);

  bool evictOneBlob();

  void trimToCap();



  EnergonConfig config_;

  std::uint64_t seed_ = 0;

  std::uint32_t nextId_ = 1;

  std::vector<EnergonBlob> blobs_;

  std::vector<EnergonMouthAnchor> mouthAnchors_;

  std::unique_ptr<EnergonSpatialIndex> spatialIndex_;
  std::unique_ptr<EnergonTasteSensoryGrid> tasteSensoryGrid_;

  EnergonSunfallTickStats lastSunfallStats_{};
  std::uint64_t cumulativeSunfallSpawns_ = 0;



  void releaseAnchorsForBlob(std::uint32_t blobId);

  void releaseMouthAnchorsExcept(std::uint32_t organismId, std::uint32_t mouthNodeId,
                                 std::uint32_t blobId);

  void remapAnchorsOnSnip(std::uint32_t tailBlobId, std::uint32_t headBlobId, float splitT);

  void splitSegmentAt(EnergonBlob& blob, int index, std::uint8_t eatenByte, float splitT);
  void splitChompRange(EnergonBlob& blob, int startIndex, int chompCount, float splitTailT,
                       float splitHeadT);

  EnergonBlob* findBlob(std::uint32_t blobId);

  const EnergonBlob* findBlob(std::uint32_t blobId) const;

};



}  // namespace evolab


