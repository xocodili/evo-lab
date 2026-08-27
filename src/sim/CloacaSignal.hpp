#pragma once

#include "sim/Energon.hpp"

#include <cstdint>

namespace evolab {

class Organism;
struct SkeletonNode;
class EnergonField;

enum class CloacaBand : std::uint8_t { None = 0, Distress = 1, Baseline = 2, Mate = 3 };

inline constexpr std::uint8_t kCloacaTagDistress = 0xB1u;
inline constexpr std::uint8_t kCloacaTagBaseline = 0xE2u;
inline constexpr std::uint8_t kCloacaTagMate = 0xF3u;

inline constexpr std::uint32_t kCloacaVentCostDistress = 1u;
inline constexpr std::uint32_t kCloacaVentCostBaseline = 1u;
inline constexpr std::uint32_t kCloacaVentCostMate = 3u;
// Minimum survival age before red mate solicitation (≈⅓ visual day).
inline constexpr std::uint32_t kMateMinAgeTicks = 600u;

CloacaBand cloacaBandFromTag(std::uint8_t tag);
std::uint8_t cloacaBandTag(CloacaBand band);
std::uint32_t cloacaVentByteCost(CloacaBand band);
CloacaBand cloacaBandFromBlob(const EnergonBlob& blob);

bool campDistressPredicate(const Organism& organism);
bool campMateReadyPredicate(const Organism& organism, std::uint64_t simTick);
CloacaBand chooseCloacaBand(const Organism& organism, std::uint64_t simTick);

// Hub vent at the computer node; consumes hub bytes per band cost.
bool expelCloacaVent(Organism& organism, EnergonField& field, SkeletonNode& computer,
                     CloacaBand band);

}  // namespace evolab
