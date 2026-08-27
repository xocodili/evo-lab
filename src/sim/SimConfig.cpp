#include "sim/SimConfig.hpp"

#include "sim/Tide.hpp"

#include <algorithm>
#include <cctype>
#include <string>

namespace evolab {

SeedArchetype parseSeedArchetype(const char* text) {
  if (text == nullptr) {
    return SeedArchetype::Nom;
  }
  std::string value(text);
  for (char& ch : value) {
    ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
  }
  if (value == "nom" || value == "camp") {
    return SeedArchetype::Nom;
  }
  if (value == "stem" || value == "stemcell") {
    return SeedArchetype::StemCell;
  }
  if (value == "actuator" || value == "a") {
    return SeedArchetype::Actuator;
  }
  return SeedArchetype::Nom;
}

const char* seedArchetypeLabel(SeedArchetype archetype) {
  switch (archetype) {
    case SeedArchetype::Nom:
      return "CAMP Nom";
    case SeedArchetype::StemCell:
      return "StemCell";
    case SeedArchetype::Actuator:
      return "Actuator";
  }
  return "Unknown";
}

std::string windowTitleForConfig(const SimConfig& config) {
  return std::string("evo-lab — Phase 2.x ") + seedArchetypeLabel(config.archetype);
}

Tide makeTideFromConfig(const SimConfig& config) {
  TideConfig tideConfig;
  if (config.tidePeriodTicks > 0.0f) {
    tideConfig.periodTicks = config.tidePeriodTicks;
  }
  return Tide(tideConfig);
}

}  // namespace evolab
