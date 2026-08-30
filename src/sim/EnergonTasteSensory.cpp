#include "sim/EnergonTasteSensory.hpp"

#include "sim/BarrenWorld.hpp"
#include "sim/CellConstants.hpp"
#include "sim/CloacaSignal.hpp"
#include "sim/EnergonInformation.hpp"
#include "sim/EnergonString.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace evolab {

bool energonBlobEligibleForMouthTaste(const EnergonBlob& blob) {
  if (blob.remaining == 0 || !blob.grounded || !blob.onWet) {
    return false;
  }
  if (blob.cornucopia) {
    return true;
  }
  switch (blob.origin) {
    case EnergonOrigin::Waste:
    case EnergonOrigin::Signal:
      return false;
    case EnergonOrigin::Sunfall:
    case EnergonOrigin::Fragment:
      return true;
    case EnergonOrigin::Cloaca: {
      switch (cloacaBandFromBlob(blob)) {
        case CloacaBand::Distress:
        case CloacaBand::Baseline:
          return true;
        default:
          return false;
      }
    }
  }
  return false;
}

float mouthTasteGridByteWeight(const EnergonBlob& blob) {
  return energonBlobEligibleForMouthTaste(blob) ? 1.0f : 0.0f;
}

void EnergonTasteSensoryGrid::clear() {
  density_.assign(static_cast<std::size_t>(resolution_ * resolution_), 0.0f);
}

void EnergonTasteSensoryGrid::rebuild(const std::vector<EnergonBlob>& blobs,
                                      float worldHalfExtent, int resolution,
                                      const BarrenWorld* world, float cellSize) {
  resolution_ = std::max(1, resolution);
  world_ = world;
  terrainCellSize_ = cellSize;
  if (worldHalfExtent <= 0.0f) {
    density_.clear();
    return;
  }

  originX_ = -worldHalfExtent;
  originZ_ = -worldHalfExtent;
  const float span = worldHalfExtent * 2.0f;
  cellSize_ = span / static_cast<float>(resolution_);
  clear();

  for (const EnergonBlob& blob : blobs) {
    if (!energonBlobEligibleForMouthTaste(blob)) {
      continue;
    }

    const float dx = blob.headX - blob.tailX;
    const float dz = blob.headZ - blob.tailZ;
    const int lastIndex = std::max(0, static_cast<int>(blob.remaining) - 1);

    for (int byteIndex = 0; byteIndex < blob.remaining; ++byteIndex) {
      const float information = energonInformationValue(energonByteAt(blob, byteIndex));
      if (information <= 0.0f) {
        continue;
      }
      const float t = lastIndex > 0 ? static_cast<float>(byteIndex) / static_cast<float>(lastIndex)
                                    : 0.5f;
      depositBytes(blob.tailX + dx * t, blob.tailZ + dz * t, information);
    }
  }
}

bool EnergonTasteSensoryGrid::sampleReachable(float worldX, float worldZ) const {
  if (world_ == nullptr || terrainCellSize_ <= 0.0f) {
    return true;
  }
  return world_->isWetWorld(worldX, worldZ, terrainCellSize_);
}

int EnergonTasteSensoryGrid::cellIndexX(float worldX) const {
  return static_cast<int>(std::floor((worldX - originX_) / cellSize_));
}

int EnergonTasteSensoryGrid::cellIndexZ(float worldZ) const {
  return static_cast<int>(std::floor((worldZ - originZ_) / cellSize_));
}

float EnergonTasteSensoryGrid::cellCenterX(int ix) const {
  return originX_ + (static_cast<float>(ix) + 0.5f) * cellSize_;
}

float EnergonTasteSensoryGrid::cellCenterZ(int iz) const {
  return originZ_ + (static_cast<float>(iz) + 0.5f) * cellSize_;
}

void EnergonTasteSensoryGrid::depositBytes(float worldX, float worldZ, float bytes) {
  if (bytes <= 0.0f || density_.empty() || !sampleReachable(worldX, worldZ)) {
    return;
  }
  const int ix = cellIndexX(worldX);
  const int iz = cellIndexZ(worldZ);
  if (ix < 0 || iz < 0 || ix >= resolution_ || iz >= resolution_) {
    return;
  }
  density_[static_cast<std::size_t>(iz * resolution_ + ix)] += bytes;
}

float EnergonTasteSensoryGrid::cellBytesAt(int ix, int iz) const {
  if (ix < 0 || iz < 0 || ix >= resolution_ || iz >= resolution_) {
    return 0.0f;
  }
  return density_[static_cast<std::size_t>(iz * resolution_ + ix)];
}

EnergonTasteSensoryPeak EnergonTasteSensoryGrid::peakInRadius(float queryX, float queryZ,
                                                              float radius) const {
  EnergonTasteSensoryPeak peak;
  if (density_.empty() || radius <= 0.0f) {
    return peak;
  }

  const float radiusSq = radius * radius;
  const int minIx = std::max(0, cellIndexX(queryX - radius));
  const int maxIx = std::min(resolution_ - 1, cellIndexX(queryX + radius));
  const int minIz = std::max(0, cellIndexZ(queryZ - radius));
  const int maxIz = std::min(resolution_ - 1, cellIndexZ(queryZ + radius));

  float bestBytes = 0.0f;
  float bestDistSq = std::numeric_limits<float>::max();
  int bestIx = -1;
  int bestIz = -1;

  for (int iz = minIz; iz <= maxIz; ++iz) {
    for (int ix = minIx; ix <= maxIx; ++ix) {
      const float bytes = cellBytesAt(ix, iz);
      if (bytes <= 0.0f) {
        continue;
      }
      const float cx = cellCenterX(ix);
      const float cz = cellCenterZ(iz);
      if (!sampleReachable(cx, cz)) {
        continue;
      }
      const float dx = cx - queryX;
      const float dz = cz - queryZ;
      const float distSq = dx * dx + dz * dz;
      if (distSq > radiusSq) {
        continue;
      }
      if (bytes > bestBytes + 1.0e-4f ||
          (std::abs(bytes - bestBytes) <= 1.0e-4f &&
           (distSq < bestDistSq - 1.0e-4f ||
            (std::abs(distSq - bestDistSq) <= 1.0e-4f &&
             (bestIx < 0 || ix < bestIx || (ix == bestIx && iz < bestIz)))))) {
        bestBytes = bytes;
        bestDistSq = distSq;
        bestIx = ix;
        bestIz = iz;
      }
    }
  }

  if (bestIx < 0) {
    return peak;
  }

  peak.valid = true;
  peak.worldX = cellCenterX(bestIx);
  peak.worldZ = cellCenterZ(bestIz);
  peak.cellBytes = bestBytes;
  return peak;
}

}  // namespace evolab
