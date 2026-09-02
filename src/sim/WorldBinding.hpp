#pragma once

#include "sim/CellConstants.hpp"

#include <cstddef>
#include <cstdint>

namespace evolab {

// World physics for stem-tile assembly (Wang-tile analogue). Same for every organism.
// Genetics supplies locus order + bind records; the world supplies socket geometry.

inline constexpr std::size_t kWorldHubSocketCount = 3u;
inline constexpr float kWorldHubSocketSeparationRad = kCampNomArmSeparationRad;
inline constexpr int kStemBindCooperativeStrength = 2;
inline constexpr float kStemBindAssemblyEpsilonFactor = 0.01f;
inline constexpr std::uint8_t kStemGlueLabel = 1u;

inline constexpr float kStemBindRateDefault = kAxonDockRate;
inline constexpr std::uint32_t kStemBindCostBytesDefault = kHgtInsertionCostBytes;

float hubSocketAngleRad(float organismHeading, std::uint8_t slotIndex);

}  // namespace evolab
