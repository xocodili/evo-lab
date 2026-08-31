#pragma once

#include "sim/Energon.hpp"

namespace evolab {

struct EnergonStats {
  int blobCount = 0;
  int falling = 0;
  int groundedWet = 0;
  int groundedDry = 0;
  std::uint64_t totalBytes = 0;
  int wetEdibleBytes = 0;
  float avgTtlWet = 0.0f;
  float avgTtlDry = 0.0f;
};

EnergonStats computeEnergonStats(const EnergonField& field);

}  // namespace evolab
