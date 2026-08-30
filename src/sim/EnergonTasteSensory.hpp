#pragma once

#include "sim/Energon.hpp"

#include <cstdint>
#include <vector>

namespace evolab {

class BarrenWorld;

struct EnergonTasteSensoryPeak {
  bool valid = false;
  float worldX = 0.0f;
  float worldZ = 0.0f;
  float cellBytes = 0.0f;
};

bool energonBlobEligibleForMouthTaste(const EnergonBlob& blob);

class EnergonTasteSensoryGrid {
public:
  void rebuild(const std::vector<EnergonBlob>& blobs, float worldHalfExtent, int resolution,
               const BarrenWorld* world, float cellSize);

  EnergonTasteSensoryPeak peakInRadius(float queryX, float queryZ, float radius) const;

private:
  void clear();
  void depositBytes(float worldX, float worldZ, float bytes);
  bool sampleReachable(float worldX, float worldZ) const;
  int cellIndexX(float worldX) const;
  int cellIndexZ(float worldZ) const;
  float cellCenterX(int ix) const;
  float cellCenterZ(int iz) const;
  float cellBytesAt(int ix, int iz) const;

  int resolution_ = 0;
  float originX_ = 0.0f;
  float originZ_ = 0.0f;
  float cellSize_ = 0.0f;
  const BarrenWorld* world_ = nullptr;
  float terrainCellSize_ = 0.0f;
  std::vector<float> density_;
};

}  // namespace evolab
