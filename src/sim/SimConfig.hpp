#pragma once

#include "sim/Tide.hpp"

#include <cstdint>
#include <string>

namespace evolab {

enum class SeedArchetype : std::uint8_t {
  StemCell,
  TwinMouth,
  Actuator,
  MouthActuator,
  PerceptorMouthActuator,
};

struct SimConfig {
  std::uint64_t seed = 42;
  int resolution = 128;
  SeedArchetype archetype = SeedArchetype::PerceptorMouthActuator;
  int nomCount = 60;
  float tidePeriodTicks = 0.0f;
  int designWidth = 1280;
  int designHeight = 720;
  float fixedSimHz = 60.0f;
  int maxSimStepsPerFrame = 5;
};

SeedArchetype parseSeedArchetype(const char* text);
const char* seedArchetypeLabel(SeedArchetype archetype);
std::string windowTitleForConfig(const SimConfig& config);
Tide makeTideFromConfig(const SimConfig& config);

}  // namespace evolab
