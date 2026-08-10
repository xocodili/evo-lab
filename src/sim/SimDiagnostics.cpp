#include "sim/SimDiagnostics.hpp"

#include <cmath>

namespace evolab {

const char* tidePhaseLabel(float waterLevel, float minLevel, float maxLevel, std::uint64_t tick,
                           float tidePeriodTicks) {
  if (tidePeriodTicks <= 0.0f) {
    return "—";
  }

  const float phase = static_cast<float>(tick) * (2.0f * 3.1415926535f / tidePeriodTicks);
  const float derivative = -std::sin(phase);

  if (derivative > 0.15f) {
    return "Rising";
  }
  if (derivative < -0.15f) {
    return "Falling";
  }
  const float mid = (minLevel + maxLevel) * 0.5f;
  return waterLevel >= mid ? "High slack" : "Low slack";
}

}  // namespace evolab
