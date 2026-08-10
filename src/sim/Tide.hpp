#pragma once

#include <cstdint>

namespace evolab {

struct TideConfig {
  float meanLevel = 0.0f;
  float amplitude = 8.0f;
  float periodTicks = 3600.0f;
};

class Tide {
public:
  Tide(TideConfig config = {});

  void setConfig(TideConfig config);
  const TideConfig& config() const { return config_; }

  float waterLevel(std::uint64_t tick) const;
  float waterLevelDelta(std::uint64_t tick) const;
  float maxAbsDelta() const;
  float minLevel() const;
  float maxLevel() const;

private:
  TideConfig config_;
};

}  // namespace evolab
