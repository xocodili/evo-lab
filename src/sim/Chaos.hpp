#pragma once

#include <cstdint>

#include <random>

namespace evolab {

// ε-chaos rates (DESIGN-NOTES §4.4). Jitter is ±kChaosJitterRate multiplicative.
inline constexpr float kChaosJitterRate = 0.03f;
inline constexpr float kEpsilonRandomChild = 0.03f;
inline constexpr float kEpsilonRandomMate = 0.05f;
inline constexpr float kMigrationRate = 0.01f;
inline constexpr float kMisalignmentRate = 0.03f;
inline constexpr float kMacromutationRate = 0.001f;

inline constexpr float kTwoPi = 6.2831853f;
inline constexpr float kSpawnSurfaceYOffset = 0.12f;
inline constexpr float kNominalBoneLengthFactor = 1.05f;
inline constexpr float kDefaultNeuralAxonEta = 0.88f;

inline constexpr std::uint64_t kChaosSaltStemCell = 0xC011C011ULL;
inline constexpr std::uint64_t kChaosSaltStarMouth = 0x0B17E011ULL;
inline constexpr std::uint64_t kChaosSaltTwoMouth = 0x07210475ULL;
inline constexpr std::uint64_t kChaosSaltEnergonSunfall = 0xE16E6050111ULL;

std::mt19937 chaosSpawnRng(std::uint64_t seed, std::uint64_t salt);

float chaosJitterMultiplier(std::mt19937& rng);
float chaosJitterMultiplier(std::mt19937_64& rng);

float chaosJitterFloat(float baseline, std::mt19937& rng);
float chaosJitterFloat(float baseline, std::mt19937_64& rng);

std::uint16_t chaosJitterTrust(std::uint16_t baseline, std::mt19937& rng);

bool chaosBernoulli(float rate, std::mt19937& rng);
bool chaosBernoulli(float rate, std::mt19937_64& rng);

// Macro-random heading in [0, 2π).
float chaosSpawnHeading(std::mt19937& rng);

// Micro jitter on an existing heading (±3% of one revolution).
float chaosJitterHeading(float headingRadians, std::mt19937& rng);
float chaosJitterHeading(float headingRadians, std::mt19937_64& rng);

// 1–3 fuel-days at baseline, then ±3% jitter (clamped to valid storage band).
std::uint32_t chaosInitialStorage(std::mt19937& rng);

float nominalBoneLength(float cellSize);

}  // namespace evolab
