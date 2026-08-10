#include "sim/Chaos.hpp"

#include "sim/CellConstants.hpp"

#include <algorithm>
#include <random>

namespace evolab {

namespace {

template <typename Rng>
float jitterMultiplier(Rng& rng) {
  std::uniform_real_distribution<float> dist(1.0f - kChaosJitterRate, 1.0f + kChaosJitterRate);
  return dist(rng);
}

template <typename Rng>
bool bernoulli(Rng& rng, float rate) {
  std::uniform_real_distribution<float> dist(0.0f, 1.0f);
  return dist(rng) < rate;
}

}  // namespace

std::mt19937 chaosSpawnRng(std::uint64_t seed, std::uint64_t salt) {
  return std::mt19937(static_cast<std::uint32_t>(seed ^ salt));
}

float chaosJitterMultiplier(std::mt19937& rng) {
  return jitterMultiplier(rng);
}

float chaosJitterMultiplier(std::mt19937_64& rng) {
  return jitterMultiplier(rng);
}

float chaosJitterFloat(float baseline, std::mt19937& rng) {
  return baseline * jitterMultiplier(rng);
}

float chaosJitterFloat(float baseline, std::mt19937_64& rng) {
  return baseline * jitterMultiplier(rng);
}

std::uint16_t chaosJitterTrust(std::uint16_t baseline, std::mt19937& rng) {
  const float scaled = static_cast<float>(baseline) * jitterMultiplier(rng);
  return static_cast<std::uint16_t>(
      std::clamp(scaled, static_cast<float>(kTrustMin), static_cast<float>(kTrustMax)));
}

bool chaosBernoulli(float rate, std::mt19937& rng) {
  return bernoulli(rng, rate);
}

bool chaosBernoulli(float rate, std::mt19937_64& rng) {
  return bernoulli(rng, rate);
}

float chaosSpawnHeading(std::mt19937& rng) {
  std::uniform_real_distribution<float> dist(0.0f, kTwoPi);
  return dist(rng);
}

float chaosJitterHeading(float headingRadians, std::mt19937& rng) {
  std::uniform_real_distribution<float> dist(-kChaosJitterRate * kTwoPi,
                                             kChaosJitterRate * kTwoPi);
  return headingRadians + dist(rng);
}

float chaosJitterHeading(float headingRadians, std::mt19937_64& rng) {
  std::uniform_real_distribution<float> dist(-kChaosJitterRate * kTwoPi,
                                             kChaosJitterRate * kTwoPi);
  return headingRadians + dist(rng);
}

std::uint32_t chaosInitialStorage(std::mt19937& rng) {
  std::uniform_int_distribution<std::uint32_t> daysDist(1, 3);
  const std::uint32_t days = daysDist(rng);
  const float bytes = chaosJitterFloat(static_cast<float>(days * kTicksPerStemCellDay), rng);
  const auto jittered = static_cast<std::uint32_t>(bytes + 0.5f);
  return std::clamp(jittered, kTicksPerStemCellDay, kStemCellStorageMaxBytes);
}

float nominalBoneLength(float cellSize) {
  return cellSize * kNominalBoneLengthFactor;
}

}  // namespace evolab
