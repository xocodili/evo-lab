#include "sim/DayCycle.hpp"

#include <algorithm>
#include <cmath>

namespace evolab {

DayCycle::DayCycle(float periodTicks) : periodTicks_(std::max(1.0f, periodTicks)) {}

float DayCycle::phase01(std::uint64_t tick) const {
  const float phase = static_cast<float>(tick) * (2.0f * 3.1415926535f / periodTicks_);
  return 0.5f + 0.5f * std::sin(phase - 3.1415926535f * 0.5f);
}

float DayCycle::sunIntensity(std::uint64_t tick) const {
  const float phase = static_cast<float>(tick) * (2.0f * 3.1415926535f / periodTicks_);
  return std::max(0.0f, std::sin(phase));
}

void DayCycle::skyColor(std::uint64_t tick, float& r, float& g, float& b) const {
  const float sun = sunIntensity(tick);
  const float twilight = phase01(tick);

  const float dayR = 0.53f;
  const float dayG = 0.75f;
  const float dayB = 0.92f;
  const float nightR = 0.04f;
  const float nightG = 0.05f;
  const float nightB = 0.12f;

  const float t = std::clamp(twilight, 0.0f, 1.0f);
  r = nightR + (dayR - nightR) * t;
  g = nightG + (dayG - nightG) * t;
  b = nightB + (dayB - nightB) * t;

  r += sun * 0.08f;
  g += sun * 0.06f;
}

void DayCycle::clockTime(std::uint64_t tick, int& hours, int& minutes) const {
  // Linear 0→1 over the visual day; phase01 is a twilight sine and runs backward at night.
  const float dayT =
      std::fmod(static_cast<float>(tick), periodTicks_) / periodTicks_;
  const int totalMinutes = static_cast<int>(dayT * 24.0f * 60.0f) % (24 * 60);
  hours = totalMinutes / 60;
  minutes = totalMinutes % 60;
}

const char* DayCycle::dayNightLabel(std::uint64_t tick) const {
  return sunIntensity(tick) > 0.05f ? "Day" : "Night";
}

}  // namespace evolab
