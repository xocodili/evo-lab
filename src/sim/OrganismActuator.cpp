#include "sim/OrganismActuator.hpp"

#include "engine/kinematics/Math.hpp"

#include "sim/CampNeuronGating.hpp"

#include "sim/CellConstants.hpp"

#include "sim/NeuralAxon.hpp"

#include "sim/NeuronSignal.hpp"

#include "sim/OrganismNeuron.hpp"

#include "sim/Organism.hpp"

#include <algorithm>

#include <cmath>

namespace evolab {

namespace {

using evolab::engine::kinematics::normalizeAngle;

float campActuatorGo(const ActuatorInteroception& interoception, float hunger) {
  if (interoception.approach > kOrganismCampReflexMinValence) {
    return hunger * interoception.approach;
  }
  return hunger * kActuatorBaselineCrawlDrive;
}

float approachContribution(std::uint8_t confidenceByte, float weight, float gain) {
  const float valence = perceptorValenceFromConfidence(confidenceByte);
  if (valence <= 0.0f) {
    return 0.0f;
  }
  return valence * weight * gain;
}

float mouthSignalGradientDrive(float mouthSignalDelta) {
  // Rising satiation (positive Δ) trims crawl; falling satiation (negative Δ) restores it.
  return clamp01(-mouthSignalDelta * kActuatorMouthSignalGradientGain);
}

}  // namespace

ActuatorInteroception gatherActuatorInteroception(const Organism& organism,
                                                  std::uint32_t actuatorId,
                                                  std::uint64_t simTick) {
  ActuatorInteroception prior;

  const AggregatedPerceptSignals percept =
      aggregatePerceptorInboundSignals(organism, actuatorId, simTick, true);
  prior.approach = percept.approach;
  prior.flee = percept.flee;
  prior.perceptorLocked = percept.perceptorLocked;
  prior.perceptorSalience = percept.perceptorSalience;
  prior.focusKind = percept.focusKind;
  prior.focusBearing = percept.focusBearing;
  prior.gazeHeading = percept.gazeHeading;

  float bestMSteerScore = 0.0f;
  float mouthInboundUnit = 0.0f;
  bool haveMouthInbound = false;

  forEachInboundAxon(organism, actuatorId, simTick, true, [&](const InboundAxon& inbound) {
    if (!isNeuronConfidenceByte(inbound.axon.lastReceived.byte)) {
      return;
    }
    if (inbound.src.neuron == NeuronType::Mouth) {
      const float bite =
          approachContribution(inbound.axon.lastReceived.byte, inbound.weight, kMouthTasteSignalGain);
      accumulateApproachFlee(prior.approach, prior.flee, inbound.axon.lastReceived.byte,
                             inbound.weight, kMouthTasteSignalGain);
      if (bite > bestMSteerScore) {
        bestMSteerScore = bite;
        prior.mouthTasteApproach = bite;
        prior.mouthTasteBearing = inbound.src.mouthTasteBearing;
        prior.mouthTasteGradient = inbound.src.mouthTasteGradient;
        prior.mouthTasteSymmetricAmbiguity = inbound.src.mouthTasteSymmetricAmbiguity;
      }
      mouthInboundUnit = confidenceToUnit(inbound.axon.lastReceived.byte);
      haveMouthInbound = true;
    } else if (inbound.src.neuron == NeuronType::Computer) {
      const float rawLevel = confidenceToUnit(inbound.axon.lastReceived.byte);
      const float level = rawLevel * inbound.weight;
      prior.hubSatiation = std::max(prior.hubSatiation, rawLevel);
      prior.satiation = std::max(prior.satiation, level);
    }
  });

  if (haveMouthInbound) {
    if (organism.actuatorMouthInboundPriorValid) {
      prior.mouthSignalDelta = mouthInboundUnit - organism.actuatorMouthInboundPriorUnit;
    }
  }

  if (const SkeletonNode* mouth = findFirstNeuronNode(organism, NeuronType::Mouth, true)) {
    prior.mouthConfidence = confidenceToUnit(mouthFuelConfidence(*mouth));
  }

  prior.approach = clamp01(prior.approach);
  prior.flee = clamp01(prior.flee);
  prior.mouthConfidence = clamp01(prior.mouthConfidence);
  prior.hubSatiation = clamp01(prior.hubSatiation);
  prior.satiation = clamp01(prior.satiation);
  prior.mouthTasteApproach = clamp01(prior.mouthTasteApproach);
  return prior;
}

void commitActuatorMouthInboundPrior(Organism& organism, const ActuatorInteroception& interoception,
                                     std::uint64_t simTick) {
  const SkeletonNode* actuator = findFirstNeuronNode(organism, NeuronType::Actuator, true);
  if (actuator == nullptr) {
    return;
  }
  forEachInboundAxon(organism, actuator->id, simTick, true, [&](const InboundAxon& inbound) {
    if (inbound.src.neuron != NeuronType::Mouth ||
        !isNeuronConfidenceByte(inbound.axon.lastReceived.byte)) {
      return;
    }
    organism.actuatorMouthInboundPriorUnit =
        confidenceToUnit(inbound.axon.lastReceived.byte);
    organism.actuatorMouthInboundPriorValid = true;
  });
  (void)interoception;
}

MotorIntent computeCampMotorIntent(const ActuatorInteroception& interoception,
                                  std::uint32_t actuatorFuelBytes) {
  MotorIntent intent;

  const float hubBrake = confidenceToUnit(kComputerSatiationConfidence);
  const float hubNoGo = campHubRepleteNoGo(interoception.hubSatiation);
  const float fleeNoGo = campLocomotionFleeNoGo(interoception.flee);
  const float hunger = campHungerFromMouthUnit(interoception.mouthConfidence);
  const float go = campActuatorGo(interoception, hunger);
  const float gradientGo = mouthSignalGradientDrive(interoception.mouthSignalDelta);

  intent.netDrive = clamp01(go - hubNoGo - fleeNoGo + gradientGo * 0.35f);

  intent.turnRateScale =
      std::max(interoception.approach, interoception.flee) *
      perceptorGain(interoception.perceptorLocked, interoception.perceptorSalience);

  intent.tumbleRateScale = clamp01(1.0f - interoception.approach * 0.65f + interoception.flee * 0.45f +
                                   interoception.hubSatiation * 0.25f +
                                   std::max(0.0f, interoception.mouthSignalDelta) * 0.2f);
  if (interoception.mouthTasteSymmetricAmbiguity) {
    intent.tumbleRateScale =
        clamp01(intent.tumbleRateScale * kMouthTasteSymmetryTumbleBoost);
  }

  const bool hubBrakeActive = interoception.hubSatiation >= hubBrake;

  const float maxBytes = static_cast<float>(kActuatorStrokeCostPerTick);
  const float energyFactor =
      clamp01(static_cast<float>(actuatorFuelBytes) / std::max(maxBytes, 1.0f));
  float crawlDrive = clamp01(intent.netDrive + gradientGo * hunger);
  if (interoception.mouthTasteApproach > kOrganismCampReflexMinValence) {
    crawlDrive = std::max(crawlDrive, hunger * std::max(interoception.mouthTasteApproach,
                                                        kActuatorBaselineCrawlDrive));
  }
  const float strokeFloat = crawlDrive * maxBytes * energyFactor;

  if (strokeFloat >= kActuatorMotorIntentMinStroke * maxBytes && actuatorFuelBytes > 0) {
    intent.strokeBytes =
        static_cast<std::uint32_t>(std::lround(std::min(strokeFloat, maxBytes)));
    intent.strokeBytes = std::clamp(intent.strokeBytes, 1u, kActuatorStrokeCostPerTick);
    if (intent.strokeBytes > actuatorFuelBytes) {
      intent.strokeBytes = actuatorFuelBytes;
    }
  }

  intent.motorSuppressed = intent.strokeBytes == 0 && actuatorFuelBytes > 0 && hubBrakeActive;

  return intent;
}

namespace {

void turnTowardTarget(Organism& organism, float targetHeading, float turnRate) {
  const float bearingError = std::abs(normalizeAngle(targetHeading - organism.heading));
  const float adaptScale = clamp01(bearingError / kOrganismCampChemotaxisAdaptRad);
  organism.heading = turnToward(organism.heading, targetHeading,
                                kOrganismMaxTurnPerTick * turnRate * adaptScale);
}

}  // namespace

void applyCampChemotaxisHeading(Organism& organism, const ActuatorInteroception& interoception,
                               const MotorIntent& intent) {
  const bool flee = interoception.flee > interoception.approach;
  const float drive = flee ? interoception.flee : interoception.approach;
  const float fleeOffset = flee ? 3.14159265f : 0.0f;

  if (interoception.perceptorLocked && intent.turnRateScale >= kOrganismCampReflexMinValence &&
      drive >= kOrganismCampReflexMinValence) {
    const float targetHeading =
        normalizeAngle(interoception.gazeHeading + interoception.focusBearing + fleeOffset);
    turnTowardTarget(organism, targetHeading, intent.turnRateScale);
  }

  if (interoception.mouthTasteApproach > kOrganismCampReflexMinValence) {
    const float blendDenom =
        std::max(interoception.approach + interoception.mouthTasteApproach, 1.0e-4f);
    const float mouthTurnWeight = interoception.mouthTasteApproach / blendDenom;
    const float tasteTurn =
        clamp01((interoception.mouthTasteApproach * kMouthTasteTurnGain +
                 std::max(0.0f, interoception.mouthTasteGradient) * 0.15f) *
                mouthTurnWeight);
    const float targetHeading =
        normalizeAngle(organism.heading + interoception.mouthTasteBearing + fleeOffset);
    turnTowardTarget(organism, targetHeading, tasteTurn);
  }
}

}  // namespace evolab
