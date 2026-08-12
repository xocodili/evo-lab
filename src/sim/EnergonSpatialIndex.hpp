#pragma once

#include "sim/Energon.hpp"
#include "sim/EnergonString.hpp"

#include <cmath>
#include <cstdint>
#include <unordered_map>
#include <vector>

namespace evolab {

// Uniform grid for broad-phase energon blob queries (rebuilt each sim tick).
class EnergonSpatialIndex {
public:
  void rebuild(const std::vector<EnergonBlob>& blobs, float gridCellSize, float worldHalfExtent);

  template <typename Fn>
  void forEachNear(float x, float z, float radius, const std::vector<EnergonBlob>& blobs,
                   Fn&& fn) const {
    if (radius <= 0.0f) {
      return;
    }

    const float radiusSq = radius * radius;
    if (cells_.empty()) {
      for (const EnergonBlob& blob : blobs) {
        if (!blob.grounded || !blob.onWet) {
          continue;
        }
        const float dx = blob.x - x;
        const float dz = blob.z - z;
        const float spanX = std::abs(blob.headX - blob.tailX);
        const float spanZ = std::abs(blob.headZ - blob.tailZ);
        const float span = std::max(spanX, spanZ);
        const float rejectRadius = radius + span;
        if (dx * dx + dz * dz > rejectRadius * rejectRadius) {
          continue;
        }
        float t = 0.0f;
        if (energonPointSegmentDistanceSq(x, z, blob, t) > radiusSq) {
          continue;
        }
        fn(blob);
      }
      return;
    }

    const int minIx = cellIndex(x - radius);
    const int maxIx = cellIndex(x + radius);
    const int minIz = cellIndex(z - radius);
    const int maxIz = cellIndex(z + radius);

    if (visitStamp_.size() < blobs.size()) {
      visitStamp_.resize(blobs.size(), 0);
    }
    ++visitGeneration_;

    for (int iz = minIz; iz <= maxIz; ++iz) {
      for (int ix = minIx; ix <= maxIx; ++ix) {
        const auto it = cells_.find(pack(ix, iz));
        if (it == cells_.end()) {
          continue;
        }
        for (const std::size_t idx : it->second) {
          if (visitStamp_[idx] == visitGeneration_) {
            continue;
          }
          visitStamp_[idx] = visitGeneration_;

          const EnergonBlob& blob = blobs[idx];
          const float dx = blob.x - x;
          const float dz = blob.z - z;
          const float spanX = std::abs(blob.headX - blob.tailX);
          const float spanZ = std::abs(blob.headZ - blob.tailZ);
          const float span = std::max(spanX, spanZ);
          const float rejectRadius = radius + span;
          if (dx * dx + dz * dz > rejectRadius * rejectRadius) {
            continue;
          }

          float t = 0.0f;
          if (energonPointSegmentDistanceSq(x, z, blob, t) > radiusSq) {
            continue;
          }
          fn(blob);
        }
      }
    }
  }

private:
  int cellIndex(float coord) const {
    return static_cast<int>(std::floor((coord - originX_) / cellSize_));
  }

  static std::int64_t pack(int ix, int iz) {
    return (static_cast<std::int64_t>(ix) << 32) |
           static_cast<std::uint32_t>(iz);
  }

  float originX_ = 0.0f;
  float originZ_ = 0.0f;
  float cellSize_ = 1.2f;
  std::unordered_map<std::int64_t, std::vector<std::size_t>> cells_;
  mutable std::vector<std::uint32_t> visitStamp_;
  mutable std::uint32_t visitGeneration_ = 0;
};

}  // namespace evolab
