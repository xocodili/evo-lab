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
// Universal axon analog byte (all neuron types): 0–7 on the believe channel.
// Semantics depend on source neuron — see NeuronSignal.hpp.
inline constexpr std::uint8_t kNeuronConfidenceMax = 7u;
inline constexpr std::uint8_t kNeuronConfidenceNeutral = 4u;
inline constexpr std::uint32_t kNeuronConfidenceFullFuelBytes = kTicksPerStemCellDay;
// Mouth satiation at/above this level inhibits actuator stroke (M→A brake).
inline constexpr std::uint8_t kMouthInhibitActuatorConfidence = 5u;
// Perceptor world-focus outbound uses the same encoding (0=avoid … 7=approach).
inline constexpr std::uint8_t kPerceptorConfidenceMax = kNeuronConfidenceMax;
inline constexpr std::uint8_t kPerceptorConfidenceNeutral = kNeuronConfidenceNeutral;
// P-M-A Nom skeleton: equilateral triangle (A forward, M at -60°, closing M-A bone).
inline constexpr float kPmaNomActuatorJointAngle = 0.0f;
inline constexpr float kPmaNomMouthJointAngle = -1.0471976f;  // -60° from heading forward
inline constexpr std::uint8_t kSignalTagIAte = 0xA1u;
inline constexpr std::uint8_t kSignalTagIHunger = 0xA2u;
inline constexpr std::uint8_t kSignalTagIActuate = 0xA3u;
// Legacy sense tags (superseded by confidence bytes on P outbound axons).
inline constexpr std::uint8_t kSignalTagISenseFood = 0xB1u;
inline constexpr std::uint8_t kSignalTagISenseOrganism = 0xB2u;
inline constexpr std::uint8_t kSignalTagISenseBlock = 0xB3u;
// Perceptor focus cone (radians): total width ≈ 90°.
inline constexpr float kPerceptorFocusHalfAngle = 0.7853982f;
// Photoreceptor-inspired scan + transduction costs (bytes per tick, see DESIGN-NOTES).
inline constexpr std::uint32_t kPerceptorScanCostPerTick = 1u;
inline constexpr std::uint32_t kPerceptorTransductionCostPerTick = 1u;
inline constexpr float kPerceptorSenseRadiusFactor = 3.5f;
// Diurnal transduction: night shrinks effective radius and inflates perceptual noise.
inline constexpr float kPerceptDiurnalRadiusFloor = 0.35f;
inline constexpr float kPerceptNoiseBearingRad = 0.05f;
inline constexpr float kPerceptNightChaosGain = 2.5f;
inline constexpr float kPerceptFalseNegativeNightRate = 0.12f;
inline constexpr float kOrganismPmaReflexMinValence = 0.15f;
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

// Actuator (flagellar) stroke — active IMF work on top of basal (1 B/tick).
// One stroke batch costs kActuatorStrokeCostPerTick body bytes, aligned with
// kEnergonUnitsPerByte gross yield from food (2 B before mastication tax).
// Only kActuatorTranslationEta becomes directed motion; the rest is translation
// entropy (viscous/thermal loss in low-Re flow).
inline constexpr std::uint32_t kActuatorStrokeCostPerTick = 2u;
inline constexpr float kActuatorThrustPerStrokeByte = 0.055f;
inline constexpr float kActuatorTranslationEta = 0.12f;
inline constexpr float kActuatorTumbleRate = 0.07f;
inline constexpr float kActuatorTumbleTurn = 0.85f;
// Horror crawl: basal + one stroke when fuel allows (stroke gate is >= stroke cost only).
inline constexpr std::uint32_t kActuatorCrawlCostPerTick =
    kStemCellBasalCostPerTick + kActuatorStrokeCostPerTick;

// Neural axon trust fixed-point: 256 = 100% developmental baseline; 33%–166% modifiable range.
inline constexpr std::uint16_t kTrustBaseline = 256;
inline constexpr std::uint16_t kTrustMin = 85;
inline constexpr std::uint16_t kTrustMax = 426;
inline constexpr std::uint16_t kNeuralAxonDefaultTrust = kTrustBaseline;
inline constexpr std::uint16_t kNeuralAxonMinTrustFeed = kTrustMin;
inline constexpr std::uint32_t kNeuralAxonShareMinStoreBytes = 6u;

}  // namespace evolab
