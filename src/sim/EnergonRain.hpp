#pragma once

#include "sim/CellConstants.hpp"
#include "sim/WorldConstants.hpp"

#include <algorithm>
#include <cmath>

namespace evolab {

// Mean sin(day phase) over one visual rain cycle — sunfall is weighted by sunIntensity.
inline constexpr float kSunDiurnalMeanIntensity = 0.636619772f;

// CAMP Nom idle regulated duty (basal×4 + P scan + transduction); see DESIGN-NOTES §2.11.
inline constexpr std::uint32_t kCampNomRainCycleBurnPerTick =
    kStemCellBasalCostPerTick * 4u + kPerceptorScanCostPerTick + kPerceptorTransductionCostPerTick;

// Expected wet sunfall blob size (fixed chomp quantum; see Energon.cpp sunfallBlobByteCount).
inline constexpr float kSunfallMeanBytesPerBlob = static_cast<float>(kChompFieldBytes);

// Field→organism harvest / wet landing / routing inefficiency multiplier on the rain budget.
inline constexpr float kEnergonRainEntropy = 2.0f;

// Field-capacity spawn gate: fuller field → lower sunfall probability (see energonSunfallSpawnProbability).
// 100%→1%; +1% spawn per 1% fullness drop down to 50%; +2% per 1% drop from 50%→20%; ≤20% → 100%.
inline constexpr float kEnergonSpawnProbAtFull = 0.01f;
inline constexpr float kEnergonSpawnProbStepAboveHalf = 0.01f;
inline constexpr float kEnergonSpawnProbStepBelowHalf = 0.02f;
inline constexpr float kEnergonSpawnProbHalfFullness = 0.50f;
inline constexpr float kEnergonSpawnProbLowFullness = 0.20f;

// When the field is food-starved at night (sun=0), keep a trickle of rain so dawn is not a cliff.
inline constexpr float kEnergonNightFamineRainSun = 0.18f;

// Rain gate weights: wet edible bytes only — blob shells must not throttle rain.
inline constexpr float kEnergonRainFoodFullnessWeight = 1.0f;
inline constexpr float kEnergonRainBlobFullnessWeight = 0.0f;

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

// Rain scales up with live population but never below seed baseline (survivors keep spawn-day food).
inline int effectiveRainPopulation(int liveOrganisms, int baseline) {
  if (baseline > 0) {
    return std::max(liveOrganisms, baseline);
  }
  return std::max(0, liveOrganisms);
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

struct EnergonSunfallTickStats {
  int spawnedBlobs = 0;
  float fieldFullness = 0.0f;
  float spawnProbability = 0.0f;
  float nominalExpected = 0.0f;
  float adjustedExpected = 0.0f;
  float effectiveSunIntensity = 0.0f;
  bool nightFamineRain = false;
};

// Daylight sun drives nominal rain; at night, starving fields keep a reduced trickle.
inline float effectiveRainSunIntensity(float sunIntensity, int wetEdibleBytes, int rainPopulation) {
  if (sunIntensity > 0.0f) {
    return sunIntensity;
  }
  if (rainPopulation <= 0) {
    return 0.0f;
  }
  const float quota = rainCycleFieldBytesForPopulation(rainPopulation);
  if (quota <= 0.0f) {
    return 0.0f;
  }
  const float foodFullness =
      std::clamp(static_cast<float>(wetEdibleBytes) / quota, 0.0f, 1.0f);
  if (foodFullness <= kEnergonSpawnProbLowFullness) {
    return kEnergonNightFamineRainSun;
  }
  return 0.0f;
}

// Piecewise spawn gate from field fullness in [0, 1] (blobCount / maxBlobs).
inline float energonSunfallSpawnProbability(float fieldFullness) {
  const float fullness = std::clamp(fieldFullness, 0.0f, 1.0f);
  if (fullness <= kEnergonSpawnProbLowFullness) {
    return 1.0f;
  }
  if (fullness >= kEnergonSpawnProbHalfFullness) {
    return kEnergonSpawnProbAtFull +
           (1.0f - fullness) * 100.0f * kEnergonSpawnProbStepAboveHalf;
  }
  const float atHalf = kEnergonSpawnProbAtFull +
                       (1.0f - kEnergonSpawnProbHalfFullness) * 100.0f *
                           kEnergonSpawnProbStepAboveHalf;
  return std::min(1.0f, atHalf + (kEnergonSpawnProbHalfFullness - fullness) * 100.0f *
                                    kEnergonSpawnProbStepBelowHalf);
}

// Rain spawn gate fullness: wet edible bytes vs seed-baseline mouth quota (blob shells ignored).
inline float energonRainGateFullness(int wetEdibleBytes, int blobCount, int maxBlobs,
                                     int liveOrganisms) {
  (void)blobCount;
  (void)maxBlobs;
  if (liveOrganisms <= 0) {
    return 0.0f;
  }
  const float quota = rainCycleFieldBytesForPopulation(liveOrganisms);
  const float foodFullness =
      quota > 0.0f
          ? std::clamp(static_cast<float>(wetEdibleBytes) / quota, 0.0f, 1.0f)
          : 0.0f;
  return std::clamp(foodFullness * kEnergonRainFoodFullnessWeight +
                        0.0f * kEnergonRainBlobFullnessWeight,
                    0.0f, 1.0f);
}

}  // namespace evolab
