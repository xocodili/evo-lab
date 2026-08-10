#pragma once

#include <cstdint>

namespace evolab {

// Basal metabolism burns 1 storage byte per sim tick. One fuel-day = 60*60*24 bytes.
inline constexpr std::uint32_t kTicksPerStemCellDay = 60u * 60u * 24u;
inline constexpr std::uint32_t kStemCellStorageMaxBytes = kTicksPerStemCellDay * 3u;
inline constexpr std::uint32_t kStemCellBasalCostPerTick = 1;

// Mouth: each byte in a wet energon string yields this many energon units before bite tax.
inline constexpr std::uint32_t kEnergonUnitsPerByte = 2u;
// Mastication tax — paid from the bitten byte when food is present.
inline constexpr std::uint32_t kBiteCost = 1u;
// Empty-string contact: same tax, paid from organism body storage.
inline constexpr std::uint32_t kMouthLocalStoreMaxBytes = 32u;
inline constexpr std::uint32_t kMouthStoreSoftPressureBytes = 8u;
inline constexpr std::uint32_t kMouthSignalHeartbeatTicks = 30u;
inline constexpr std::uint8_t kMouthSignalTagShipping = 0xF0u;
inline constexpr std::uint32_t kAxonChannelCapacity = 64u;
inline constexpr float kNeuralAxonMinGateScale = 0.05f;
// XZ overlap radius as a fraction of world cell size.
inline constexpr float kMouthContactRadiusFactor = 0.65f;

// Skeleton yaw: max turn per tick (radians) toward flow / nearby food.
inline constexpr float kOrganismMaxTurnPerTick = 0.22f;
// Faster slewing when food is within mouth contact range.
inline constexpr float kOrganismFoodSnapTurnMultiplier = 3.0f;
// Sense wet food within this many cell sizes of root / mouth anchors.
inline constexpr float kOrganismFoodSenseRadiusFactor = 3.5f;
// Blend toward food bearing when sensed but not yet in bite range.
inline constexpr float kOrganismFoodHeadingWeight = 0.7f;

// Neural axon trust fixed-point: 256 = 100% developmental baseline; 33%–166% modifiable range.
inline constexpr std::uint16_t kTrustBaseline = 256;
inline constexpr std::uint16_t kTrustMin = 85;
inline constexpr std::uint16_t kTrustMax = 426;
inline constexpr std::uint16_t kNeuralAxonDefaultTrust = kTrustBaseline;
inline constexpr std::uint16_t kNeuralAxonMinTrustFeed = kTrustMin;
inline constexpr std::uint32_t kNeuralAxonShareMinStoreBytes = 6u;

}  // namespace evolab
