#include "sim/EnergonSpatialIndex.hpp"

#include "sim/EnergonString.hpp"

#include <algorithm>
#include <cmath>

namespace evolab {

void EnergonSpatialIndex::rebuild(const std::vector<EnergonBlob>& blobs, float gridCellSize,
                                  float worldHalfExtent) {
  cells_.clear();
  if (gridCellSize <= 0.0f) {
    return;
  }

  cellSize_ = gridCellSize;
  originX_ = -worldHalfExtent;
  originZ_ = -worldHalfExtent;

  for (std::size_t i = 0; i < blobs.size(); ++i) {
    const EnergonBlob& blob = blobs[i];
    if (!blob.grounded || !blob.onWet) {
      continue;
    }

    const float spanX = std::abs(blob.headX - blob.tailX);
    const float spanZ = std::abs(blob.headZ - blob.tailZ);
    const float span = std::max(spanX, spanZ);
    const float minX = std::min({blob.x, blob.tailX, blob.headX}) - span;
    const float maxX = std::max({blob.x, blob.tailX, blob.headX}) + span;
    const float minZ = std::min({blob.z, blob.tailZ, blob.headZ}) - span;
    const float maxZ = std::max({blob.z, blob.tailZ, blob.headZ}) + span;

    const int minIx = cellIndex(minX);
    const int maxIx = cellIndex(maxX);
    const int minIz = cellIndex(minZ);
    const int maxIz = cellIndex(maxZ);
    for (int iz = minIz; iz <= maxIz; ++iz) {
      for (int ix = minIx; ix <= maxIx; ++ix) {
        cells_[pack(ix, iz)].push_back(i);
      }
    }
  }
}

}  // namespace evolab
