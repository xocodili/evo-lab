#pragma once

#include "sim/CellConstants.hpp"
#include "sim/WorldConstants.hpp"

#include <cmath>

namespace evolab {

// Mean sin(day phase) over one visual rain cycle — sunfall is weighted by sunIntensity.
inline constexpr float kSunDiurnalMeanIntensity = 0.636619772f;

// CAMP Nom idle regulated duty (basal×4 + P scan + transduction); see DESIGN-NOTES §2.11.
inline constexpr std::uint32_t kCampNomRainCycleBurnPerTick =
    kStemCellBasalCostPerTick * 4u + kPerceptorScanCostPerTick + kPerceptorTransductionCostPerTick;

// Expected wet sunfall blob size after spawn jitter (matches Energon.cpp randomByteCount).
inline constexpr float kSunfallMeanBytesPerBlob = 3.33f;

// Field→organism harvest / wet landing / routing inefficiency multiplier on the rain budget.
inline constexpr float kEnergonRainEntropy = 2.0f;

// f(n): field bytes required this rain cycle to keep n live Noms at idle duty (before entropy).
inline constexpr float rainCycleFieldBytesPerNom() {
  return static_cast<float>(static_cast<std::uint32_t>(kVisualDayCyclePeriodTicks)) *
         static_cast<float>(kCampNomRainCycleBurnPerTick) /
         static_cast<float>(kBiteNetYieldBytes);
}

inline float rainCycleFieldBytesForPopulation(int liveOrganisms) {
  if (liveOrganisms <= 0) {
    return 0.0f;
  }
  return static_cast<float>(liveOrganisms) * rainCycleFieldBytesPerNom() * kEnergonRainEntropy;
}

// Expected sunfall blob spawns this tick: rain = f(n) × entropy, distributed by diurnal sun.
inline float expectedSunfallBlobsPerTick(int liveOrganisms, float sunIntensity) {
  if (liveOrganisms <= 0 || sunIntensity <= 0.0f) {
    return 0.0f;
  }
  const float cycleFieldBytes = rainCycleFieldBytesForPopulation(liveOrganisms);
  const float meanBytesPerTick =
      cycleFieldBytes /
      (static_cast<float>(kVisualDayCyclePeriodTicks) * kSunDiurnalMeanIntensity);
  const float meanBlobsPerTick = meanBytesPerTick / kSunfallMeanBytesPerBlob;
  return meanBlobsPerTick * sunIntensity;
}

}  // namespace evolab
