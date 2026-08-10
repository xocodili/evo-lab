#pragma once

#include "sim/EnergonStats.hpp"

#include <cstdint>
#include <string>

namespace evolab {

struct CellPopulationStats;

struct SimDiagnostics {
  float fps = 0.0f;
  float waterLevel = 0.0f;
  float tideMin = 0.0f;
  float tideMax = 0.0f;
  const char* tidePhase = "—";
  float sunIntensity = 0.0f;
  int clockHours = 0;
  int clockMinutes = 0;
  const char* dayNight = "Day";
  std::uint64_t simTick = 0;
  std::uint64_t seed = 0;
  bool paused = false;
  int energonCap = 0;
  EnergonStats energon{};
  int liveCells = 0;
  int organisms = 0;
  int stemCells = 0;
  int mouthOrganisms = 0;
  int mouthNeurons = 0;
  std::string hoveredCellSummary;
};

const char* tidePhaseLabel(float waterLevel, float minLevel, float maxLevel, std::uint64_t tick,
                           float tidePeriodTicks);

}  // namespace evolab
