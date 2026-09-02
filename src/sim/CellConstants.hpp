#pragma once

#include "sim/Chaos.hpp"

#include <cstdint>

namespace evolab {

// Basal metabolism burns 1 storage byte per sim tick. One fuel-day = 60*60*24 bytes.
inline constexpr std::uint32_t kTicksPerStemCellDay = 60u * 60u * 24u;
// Four fuel-days: hub must hold baseline parthenogenesis debit + parent reserve (see PARTHENOGENESIS.md).
inline constexpr std::uint32_t kStemCellStorageMaxBytes = kTicksPerStemCellDay * 4u;
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
// Chew-buffer drain per tick when not biting (satiation broadcast tracks live mouth state).
inline constexpr std::uint32_t kMouthChewDecayPerTick = 2u;
// Mouth wallet kept local for basal + chew; remainder conveyed downstream each tick.
inline constexpr std::uint32_t kMouthConveyReserveBytes = 8u;
inline constexpr std::uint32_t kMouthConveyanceMaxPerTick = 12u;
// C hub surplus dispatch per tick — one bite-worth so grazing can refill the hub.
inline constexpr std::uint32_t kComputerHubDispatchMaxPerTick = kBiteNetYieldBytes;
// Universal axon analog byte (all neuron types): 0–7 on the believe channel.
// Semantics depend on source neuron — see NeuronSignal.hpp.
inline constexpr std::uint8_t kNeuronConfidenceMax = 7u;
inline constexpr std::uint8_t kNeuronConfidenceNeutral = 4u;
inline constexpr std::size_t kNeuronConfidenceBinCount =
    static_cast<std::size_t>(kNeuronConfidenceMax) + 1u;
inline constexpr std::uint32_t kNeuronConfidenceFullFuelBytes = kTicksPerStemCellDay;
// Mouth satiation at/above this level inhibits baseline crawl (M→A brake threshold).
inline constexpr std::uint8_t kMouthInhibitActuatorConfidence = 5u;
// Chew FSA resume band — mouth un-pauses only after satiation falls to/below this byte.
inline constexpr std::uint8_t kMouthChewResumeActuatorConfidence = 3u;
// When chew-paused, P food-approach above this unit overrides REFUSE (keep grazing).
inline constexpr float kMouthChewRefuseMaxApproach = 0.15f;
// CAMP horror-crawl baseline when hungry and mouth is not signaling satiation.
inline constexpr float kActuatorBaselineCrawlDrive = 0.35f;
// A modulates crawl from M satiation Δ (rising satiation trims crawl; falling restores it).
inline constexpr float kActuatorMouthSignalGradientGain = 0.85f;
inline constexpr float kMouthBaselineFeedDrive = 0.35f;
inline constexpr float kMouthFeedIntentMinBite = 0.08f;
// Mouth diet EMA: postingestive palatability / gag reflex (CTA analogue — see EVOLUTION.md §1.3).
inline constexpr float kMouthDietEmaAlpha = 0.18f;
inline constexpr float kMouthDietGagDistressThreshold = 0.55f;
inline constexpr float kMouthDietPalatableSunfallThreshold = 0.45f;
// Minimum integrated motor drive (× max stroke bytes) before paying fuel.
inline constexpr float kActuatorMotorIntentMinStroke = 0.08f;
// Perceptor world-focus outbound uses the same encoding (0=avoid … 7=approach).
inline constexpr std::uint8_t kPerceptorConfidenceMax = kNeuronConfidenceMax;
inline constexpr std::uint8_t kPerceptorConfidenceNeutral = kNeuronConfidenceNeutral;
// CAMP Nom skeleton: legacy Y-star hub arms (dual-computer tests); gen-0 uses torpedo chain.
inline constexpr float kCampNomArmSeparationRad = 2.094395102f;
inline constexpr float kCampTorpedoForwardSegmentOffset = 0.0f;
inline constexpr float kCampPerceptorBindAngle = 0.0f;
inline constexpr float kCampActuatorBindAngle = -kCampNomArmSeparationRad;
inline constexpr float kCampMouthBindAngle = kCampNomArmSeparationRad;
inline constexpr float kCampNomLinkJointAngle = 0.0f;
// Axon-bundle musculature: believe-traffic tension → local yaw delta on each bone.
inline constexpr float kAxonBundleMaxFlexRad = 0.38f;
inline constexpr float kAxonBundleFlexGain = 0.34f;
inline constexpr float kAxonBundleFlexStiffness = 0.82f;
// Bundle-coupled stroke: lateral arms trail; keel torque from asymmetry.
inline constexpr float kActuatorStrokeFlexGain = 3.2f;
inline constexpr float kAxonBundleTrailFlexGain = 1.1f;
inline constexpr float kAxonBundleKeelYawGain = 14.0f;

inline constexpr float kBodyLinearDrag = 0.12f;
inline constexpr float kBodyYawDamping = 0.15f;
inline constexpr float kBodyInvMass = 1.0f;
inline constexpr float kBodyInvInertia = 0.35f;
inline constexpr float kMusclePdDamping = 0.28f;
inline constexpr float kStrokeMuscleStiffnessBoost = 2.4f;
inline constexpr std::size_t kComputerRegisterBytes = 8u;
inline constexpr std::uint32_t kComputerHubStoreMaxBytes = kStemCellStorageMaxBytes;
inline constexpr std::uint32_t kComputerHubReserveBytes = kTicksPerStemCellDay / 4u;
inline constexpr std::uint8_t kComputerSatiationConfidence = 6u;
inline constexpr std::uint8_t kComputerSignalExpulsionByte = 1u;
inline constexpr float kComputerMinDispatchGain = 0.15f;
// Black Queen equilibrium (stem layer): stop export below reserve slack or while store drains.
inline constexpr std::uint32_t kComputerHubConservationSlackBytes = 3600u;
inline constexpr std::uint32_t kComputerHubConservationDrainToleranceBytes = 4u;
// Begin ramping export above this fill unit; full export at kComputerSatiationConfidence.
inline constexpr float kComputerHubConservationExportStartUnit = 0.55f;
// Shared stem defaults — every differentiated neuron (P/M/C/A) inherits ± jitter on exportStart.
inline constexpr float kStemEquilibriumExportStartUnit = kComputerHubConservationExportStartUnit;
inline constexpr std::uint32_t kStemEquilibriumDrainToleranceBytes =
    kComputerHubConservationDrainToleranceBytes;
inline constexpr float kStemEquilibriumExportStartMin = 0.40f;
inline constexpr float kStemEquilibriumExportStartMax = 0.70f;
// Minimum export while store is stable but below the heritable ramp knee (keeps P/M/A alive).
inline constexpr float kStemEquilibriumMinExportScale = kComputerMinDispatchGain;
// P vs M valence mismatch suppresses dispatch (CTA / postingestive RPE analogue).
inline constexpr float kComputerCtaDisagreementGain = 0.3f;
inline constexpr std::uint8_t kSignalTagReservedMin = 0xA0u;
// Perceptor focus cone (radians): total width ≈ 90°.
inline constexpr float kPerceptorFocusHalfAngle = 0.7853982f;
// Photoreceptor-inspired scan + transduction costs (bytes per tick, see DESIGN-NOTES).
inline constexpr std::uint32_t kPerceptorScanCostPerTick = 1u;
inline constexpr std::uint32_t kPerceptorTransductionCostPerTick = 1u;
// Chemotaxis horizon in multiples of cellSize (~nom body scale). Near-sighted P cone;
// M taste reaches ~2× P (medium range). Berg & Purcell run-and-tumble bias is body-length
// scaled, not map-spanning (Gardiner & Atema 2010; Hueter et al. 2004 JEB).
inline constexpr float kPerceptorSenseRadiusFactor = 2.0f;
inline constexpr float kPerceptorFocusLockThreshold = 0.05f;
inline constexpr float kPerceptorFocusReleaseThreshold = 0.02f;
// Berg-style run bias: outbound confidence nudged by Δ food Go/NoGo score vs prior paid scan.
inline constexpr float kPerceptTemporalGradientGain = 4.0f;
inline constexpr float kOrganismCampChemotaxisAdaptRad = 0.35f;
inline constexpr float kOrganismCampFoodTumbleBearingRad = 0.25f;
inline constexpr float kPerceptorRangeSalienceFloor = 0.25f;
// Diurnal transduction: night shrinks effective radius and inflates perceptual noise.
inline constexpr float kPerceptDiurnalRadiusFloor = 0.35f;
inline constexpr float kPerceptNoiseBearingRad = 0.05f;
inline constexpr float kPerceptNightChaosGain = 2.5f;
inline constexpr float kPerceptFalseNegativeNightRate = 0.12f;
// Ambient photoreception mapped to believe bytes 0=night, 7=noon (P→M/C world-clock channel).
inline constexpr float kPerceptDiurnalNeutralSun = 0.05f;
// Deep famine: skip expensive P field scans (prior-tick famineUnit); diurnal signal still free.
inline constexpr float kCoordinatorDeepTorporFamineThreshold = 0.72f;
inline constexpr float kTorporScanSkipMaxProbability = 0.85f;
// Max basal skip at famineUnit=1 (was 0.45 — too shallow for quiescence-scale savings).
inline constexpr float kCoordinatorFamineBasalSkipGain = 0.92f;
inline constexpr float kOrganismCampReflexMinValence = 0.12f;
inline constexpr std::uint32_t kAxonChannelCapacity = 64u;
// R0 HGT: idle/dangling axon line maintenance debited from downstream dst (or live cap if dst dead).
inline constexpr std::uint32_t kAxonTransitBasalCostPerTick = 1u;
// Uncapped-end INSERTION: brush radius and dock entropy (see docs/HGT-INSERTION.md).
inline constexpr float kAxonDockRadiusFactor = 0.5f;
inline constexpr std::uint32_t kHgtInsertionCostBytes = 4u;

// R1 parthenogenesis — vertical reproduction (see docs/PARTHENOGENESIS.md).
inline constexpr std::uint32_t kParthenogenesisMinAgeTicks = 600u;
inline constexpr std::uint32_t kParthenogenesisInitCost = 864u;
inline constexpr std::uint32_t kParthenogenesisStepBasalCost = 8u;
inline constexpr std::uint32_t kParthenogenesisChildEndowmentBytes = kTicksPerStemCellDay * 2u;
// Retain hub reserve floor after birth — aligned with vent steady-state (~6/7 hub cap).
inline constexpr std::uint32_t kParthenogenesisParentReserveMin = 10750u;
inline constexpr std::uint32_t kParthenogenesisBaselineCampDebit = 259'200u;
inline constexpr float kParthenogenesisStructuralRate = kMisalignmentRate;
inline constexpr std::size_t kCampMorphogenesisMaxNeurons = 8u;
inline constexpr std::uint32_t kParthenogenesisDuplicationSurcharge = 2'160u;
inline constexpr std::uint32_t kParthenogenesisDeletionSurcharge = 432u;
inline constexpr std::uint32_t kParthenogenesisInsertionSurcharge = 4'320u;
inline constexpr float kParthenogenesisSpawnOffsetFactor = 0.8f;
inline constexpr std::uint32_t kParthenogenesisCelebrationTicks = 180u;
inline constexpr std::uint32_t kFeedbagOracleParthenogenesisMinAgeTicks = 120u;
inline constexpr std::uint32_t kParthenogenesisRefractoryTicks = 3600u;
inline constexpr float kNeuralAxonMinGateScale = 0.05f;
// XZ overlap radius as a fraction of world cell size (strict bite / chew contact).
inline constexpr float kMouthContactRadiusFactor = 0.65f;
// Short-range adhesion analogue (van der Waals, mucus, pili): discovery, prune, and chew
// co-advect all use this radius. Match neuron billboard diameter on screen (~8px); was 3.5×
// cellSize and hoovered patches. Slightly wider than bite contact so near-misses latch.
inline constexpr float kMouthStickyRadiusFactor = 0.75f;
// Extra-oral taste buds: omnidirectional chemo sampling at the mouth (barbel / olfaction
// analogue). Medium reach — wider than P focus cone but not map-spanning.
inline constexpr float kMouthTasteRadiusFactor = kPerceptorSenseRadiusFactor * 2.0f;
inline constexpr float kMouthTasteSalienceFloor = 0.25f;
// Berg-style temporal ΔC bias on run/tumble when P has no lock.
inline constexpr float kMouthTasteTemporalGain = 3.0f;
inline constexpr float kMouthTasteTumbleSuppressGain = 0.55f;
inline constexpr float kMouthTasteApproachGain = 0.45f;
inline constexpr float kMouthTasteTurnGain = 0.35f;
// Vestigial M→A believe gain — evolved weaker than P; dominance emerges via trust × valence.
inline constexpr float kMouthTasteSignalGain = 0.32f;
// Pineal-analogue vestigial gain when P is locked (lateral eyes dominate).
inline constexpr float kMouthTasteVestigialGainWhenPerceptorLocked = 0.18f;
inline constexpr float kMouthTasteVestigialTurnScale = 0.22f;
// When omnidirectional taste vectors cancel (symmetric food ring), resultant bearing is ambiguous.
inline constexpr float kMouthTasteSymmetryVectorEpsilonSq = 0.04f;
inline constexpr float kMouthTasteSymmetryTumbleBoost = 1.75f;
// Taste attention latch: hold one coarse-grid peak until eaten, depleted, P handover, or timeout.
inline constexpr std::uint32_t kMouthTasteLatchSwitchCostBytes = 1u;
inline constexpr float kMouthTastePeakHysteresisFraction = 0.15f;
inline constexpr float kMouthTasteLatchQuitMassFraction = 0.12f;
inline constexpr std::uint32_t kMouthTasteLatchMaxTicks = 1200u;
// Coarse taste grid byte weights (sunfall ≫ distress blue — cannibal cue when nothing better).
inline constexpr float kMouthTasteSunfallGridWeight = 1.0f;
inline constexpr float kMouthTasteFragmentGridWeight = 0.85f;
inline constexpr float kMouthTasteDistressCloacaGridWeight = 0.12f;
inline constexpr float kMouthTasteBaselineCloacaGridWeight = 0.05f;
// Coarse chemo map for extra-oral taste: world-spanning byte-density field (clumping / mass flux).
inline constexpr int kMouthTasteSensoryGridResolution = 256;
inline constexpr int kSunfallEntropyPeriodTicks = 3;
// Dry-land energon clears faster than wet (TTL + byte entropy); un edible on dry anyway.
inline constexpr float kEnergonDryDecayMultiplier = 18.0f;

// Ordinal classification for jittered cloaca palette bytes on the field.
inline constexpr std::uint8_t kCloacaPaletteDistressCeiling = 96u;
inline constexpr std::uint8_t kCloacaPaletteMateFloor = 192u;
// Minimum separation between heritable band tiers after jitter.
inline constexpr std::uint8_t kCloacaPaletteMinTierGap = 24u;

// Stem-cell Hz coordinator (precursor C): per-node activity throttle from pattern match + delta.
inline constexpr std::size_t kCoordinatorRegisterBytes = 4u;
inline constexpr float kCoordinatorMinDutyScale = 0.08f;
inline constexpr float kCoordinatorMaxDutyScale = 1.0f;
inline constexpr float kCoordinatorBaselineDutyScale = 0.55f;
inline constexpr float kCoordinatorExcitationGain = 0.45f;
inline constexpr float kCoordinatorDeltaGain = 0.25f;
inline constexpr float kCoordinatorSatiationBrake = 0.35f;
// Famine/torpor: low hub + quiet axons + empty field → lower duty (replaces starvationBoost).
inline constexpr float kCoordinatorFamineTorpor = 0.45f;
inline constexpr float kCoordinatorFamineHubWeight = 0.35f;
inline constexpr float kCoordinatorFamineQuietWeight = 0.30f;
inline constexpr float kCoordinatorFamineFieldWeight = 0.35f;
inline constexpr float kCoordinatorFeastHubUnit = 0.55f;
inline constexpr float kCoordinatorFeastFieldFood = 0.25f;
inline constexpr float kCoordinatorFamineFieldSuppress = 0.20f;
inline constexpr float kCoordinatorFamineBasalSkipThreshold = 0.45f;  // famineUnit gate for basal skip
inline constexpr float kCoordinatorMinDutyPerceptor = 0.18f;
inline constexpr float kCoordinatorMinDutyMouth = 0.22f;
inline constexpr float kCoordinatorMinDutyActuator = 0.08f;
inline constexpr float kCoordinatorMinDutyComputer = 0.12f;

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
inline constexpr float kDefaultTumbleRateFactor = 1.0f;
inline constexpr float kDefaultTumbleTurnFactor = 1.0f;
inline constexpr float kDefaultTumbleChiralityBias = 0.0f;
inline constexpr float kTumbleChiralityBiasMax = 0.85f;
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
