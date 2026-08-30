#pragma once

#include "sim/Energon.hpp"

#include <cstdint>
#include <random>

namespace evolab {

// 256-slot palette: cool (low ordinal) → warm (high ordinal) = low → high information value.
inline constexpr int kEnergonEntropyDecayStep = 1;
// Max palette steps cooled per axon hop when eta → 0 (thermal dissipation along the wire).
inline constexpr int kEnergonHopCoolSpan = 32;

// Information-as-energy mass for one palette byte in [0, 1].
float energonInformationValue(std::uint8_t byte);
float energonBlobInformationMass(const EnergonBlob& blob);

// Cool→warm rainbow RGB in [0, 1] (violet/blue at 0, red/orange at 255).
void energonPaletteRgb(std::uint8_t byte, float& r, float& g, float& b);

// Entropy: step byte down the palette (toward cool / disorder).
std::uint8_t energonDecayByte(std::uint8_t byte, int steps = kEnergonEntropyDecayStep);

// Axon hop basal cost: transmitted bytes cool by (1 − η) of kEnergonHopCoolSpan.
std::uint8_t energonHopCoolByte(std::uint8_t byte, float eta);

// Cloaca vent tiers on the shared palette (single byte on the wire).
inline constexpr std::uint8_t kEnergonPaletteDistress = 32u;
inline constexpr std::uint8_t kEnergonPaletteBaseline = 128u;
inline constexpr std::uint8_t kEnergonPaletteMate = 240u;

std::uint8_t energonRandomSunfallByte(std::mt19937_64& rng);

void energonSetByteAt(EnergonBlob& blob, int index, std::uint8_t value);
void energonCompactZeroBytes(EnergonBlob& blob);
void energonEntropyDecayBlob(EnergonBlob& blob, int steps);

}  // namespace evolab
