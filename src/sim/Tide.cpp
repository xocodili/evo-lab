#include "sim/Tide.hpp"

#include <cmath>

namespace evolab {

Tide::Tide(TideConfig config) : config_(config) {}

void Tide::setConfig(TideConfig config) { config_ = config; }

float Tide::waterLevel(std::uint64_t tick) const {
  if (config_.periodTicks <= 0.0f) {
    return config_.meanLevel;
  }
  const float phase =
      static_cast<float>(tick) * (2.0f * 3.1415926535f / config_.periodTicks);
  return config_.meanLevel + config_.amplitude * std::sin(phase);
}

float Tide::waterLevelDelta(std::uint64_t tick) const {
  if (config_.periodTicks <= 0.0f) {
    return 0.0f;
  }
  const float phase =
      static_cast<float>(tick) * (2.0f * 3.1415926535f / config_.periodTicks);
  return config_.amplitude * std::cos(phase) * (2.0f * 3.1415926535f / config_.periodTicks);
}

float Tide::maxAbsDelta() const {
  if (config_.periodTicks <= 0.0f) {
    return 0.0f;
  }
  return config_.amplitude * (2.0f * 3.1415926535f / config_.periodTicks);
}

float Tide::minLevel() const { return config_.meanLevel - config_.amplitude; }

float Tide::maxLevel() const { return config_.meanLevel + config_.amplitude; }

}  // namespace evolab
