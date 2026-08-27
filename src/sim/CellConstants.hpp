#pragma once

#include <cstdint>

namespace evolab {

// Basal metabolism burns 1 storage byte per sim tick. One fuel-day = 60*60*24 bytes.
inline constexpr std::uint32_t kTicksPerStemCellDay = 60u * 60u * 24u;
inline constexpr std::uint32_t kStemCellStorageMaxBytes = kTicksPerStemCellDay * 3u;
inline constexpr std::uint32_t kStemCellBasalCostPerTick = 1;
// Ticks a neuron may run basal-arrears before death (conveyance/refill happens same frame after viability).
inline constexpr std::uint32_t kNeuronBasalGraceTicks = 8u;

// Mouth: each byte in a wet energon string yields this many energon units before bite tax.
// Feedbag equilibrium: 9 gross − 1 mastication tax = 8 net B/chew (matches ~8 B/tick crawl ceiling).
inline constexpr std::uint32_t kEnergonUnitsPerByte = 9u;
// Mastication tax — paid from the bitten byte when food is present.
inline constexpr std::uint32_t kBiteCost = 1u;
inline constexpr std::uint32_t kBiteNetYieldBytes = kEnergonUnitsPerByte - kBiteCost;
// Empty-string contact: same tax, paid from organism body storage.
inline constexpr std::uint32_t kMouthLocalStoreMaxBytes = 32u;
inline constexpr std::uint32_t kNeuronStoreMaxBytes = kMouthLocalStoreMaxBytes;
// Universal axon analog byte (all neuron types): 0–7 on the believe channel.
// Semantics depend on source neuron — see NeuronSignal.hpp.
inline constexpr std::uint8_t kNeuronConfidenceMax = 7u;
inline constexpr std::uint8_t kNeuronConfidenceNeutral = 4u;
inline constexpr std::size_t kNeuronConfidenceBinCount =
    static_cast<std::size_t>(kNeuronConfidenceMax) + 1u;
inline constexpr std::uint32_t kNeuronConfidenceFullFuelBytes = kTicksPerStemCellDay;
// Mouth satiation at/above this level inhibits baseline crawl (M→A brake threshold).
inline constexpr std::uint8_t kMouthInhibitActuatorConfidence = 5u;
// CAMP horror-crawl baseline when hungry and mouth is not signaling satiation.
inline constexpr float kActuatorBaselineCrawlDrive = 0.35f;
inline constexpr float kMouthBaselineFeedDrive = 0.35f;
inline constexpr float kMouthFeedIntentMinBite = 0.08f;
// Minimum integrated motor drive (× max stroke bytes) before paying fuel.
inline constexpr float kActuatorMotorIntentMinStroke = 0.08f;
// Perceptor world-focus outbound uses the same encoding (0=avoid … 7=approach).
inline constexpr std::uint8_t kPerceptorConfidenceMax = kNeuronConfidenceMax;
inline constexpr std::uint8_t kPerceptorConfidenceNeutral = kNeuronConfidenceNeutral;
// CAMP Nom skeleton: Y-star from computer hub — P forward, A/M on ±120° arms (flux cap).
inline constexpr float kCampNomArmSeparationRad = 2.094395102f;
inline constexpr float kCampPerceptorBindAngle = 0.0f;
inline constexpr float kCampActuatorBindAngle = -kCampNomArmSeparationRad;
inline constexpr float kCampMouthBindAngle = kCampNomArmSeparationRad;
inline constexpr float kCampNomLinkJointAngle = 0.0f;
// Axon-bundle musculature: believe-traffic tension → local yaw delta on each bone.
inline constexpr float kAxonBundleMaxFlexRad = 0.38f;
inline constexpr float kAxonBundleFlexGain = 0.34f;
inline constexpr float kAxonBundleFlexStiffness = 0.82f;
inline constexpr std::size_t kComputerRegisterBytes = 8u;
inline constexpr std::uint32_t kComputerHubStoreMaxBytes = kStemCellStorageMaxBytes;
inline constexpr std::uint32_t kComputerHubReserveBytes = kTicksPerStemCellDay / 4u;
inline constexpr std::uint8_t kComputerSatiationConfidence = 6u;
inline constexpr std::uint8_t kComputerSignalExpulsionByte = 1u;
inline constexpr float kComputerMinDispatchGain = 0.15f;
inline constexpr std::uint8_t kSignalTagReservedMin = 0xA0u;
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
inline constexpr float kOrganismCampReflexMinValence = 0.15f;
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
// Stroke batch (2 B) is the locomotion slice; gross chew (9 B) is the feedbag slice — net 8 ≈ crawl duty.
// Only kActuatorTranslationEta becomes directed motion; the rest is translation entropy (viscous/thermal loss).
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
// Three-factor believe-trust nudge per qualifying outcome (fixed-point step).
inline constexpr std::uint16_t kTrustLearnStep = 6u;

}  // namespace evolab
