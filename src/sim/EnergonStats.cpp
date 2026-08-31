#include "sim/EnergonStats.hpp"

namespace evolab {

EnergonStats computeEnergonStats(const EnergonField& field) {
  EnergonStats stats;
  stats.blobCount = field.activeCount();

  float wetTtlSum = 0.0f;
  float dryTtlSum = 0.0f;
  int wetTtlCount = 0;
  int dryTtlCount = 0;

  for (const EnergonBlob& blob : field.blobs()) {
    stats.totalBytes += blob.remaining;
    if (blob.grounded && blob.onWet && blob.remaining > 0) {
      if (blob.origin == EnergonOrigin::Sunfall || blob.origin == EnergonOrigin::Fragment) {
        stats.wetEdibleBytes += blob.remaining;
      }
    }
    if (!blob.grounded) {
      ++stats.falling;
    } else if (blob.onWet) {
      ++stats.groundedWet;
      wetTtlSum += blob.ttl;
      ++wetTtlCount;
    } else {
      ++stats.groundedDry;
      dryTtlSum += blob.ttl;
      ++dryTtlCount;
    }
  }

  if (wetTtlCount > 0) {
    stats.avgTtlWet = wetTtlSum / static_cast<float>(wetTtlCount);
  }
  if (dryTtlCount > 0) {
    stats.avgTtlDry = dryTtlSum / static_cast<float>(dryTtlCount);
  }

  return stats;
}

}  // namespace evolab
