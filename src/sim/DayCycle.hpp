#pragma once

#include <cstdint>

namespace evolab {

class DayCycle {
public:
  explicit DayCycle(float periodTicks = 2400.0f);

  float periodTicks() const { return periodTicks_; }
  float phase01(std::uint64_t tick) const;
  float sunIntensity(std::uint64_t tick) const;
  void skyColor(std::uint64_t tick, float& r, float& g, float& b) const;
  void clockTime(std::uint64_t tick, int& hours, int& minutes) const;
  const char* dayNightLabel(std::uint64_t tick) const;

private:
  float periodTicks_;
};

}  // namespace evolab
