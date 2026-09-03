#include "sim/OrganismActuator.hpp"

#include "engine/kinematics/Math.hpp"

#include "sim/CampLocomotionBody.hpp"
#include "sim/CampNeuronGating.hpp"

#include "sim/CellConstants.hpp"

#include "sim/NeuralAxon.hpp"

#include "sim/NeuronSignal.hpp"

#include "sim/OrganismNeuron.hpp"

#include "sim/Organism.hpp"

#include "sim/NeuronFuel.hpp"
#include "sim/NeuronStem.hpp"

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

void seedActuatorLocomotionFuelInteroception(ActuatorInteroception& interoception,
                                            std::uint32_t locomotionFuelBytes,
                                            std::uint32_t actuatorWalletBytes) {
  interoception.locomotionFuelBytes = locomotionFuelBytes;
  const float strokeCost = static_cast<float>(kActuatorStrokeCostPerTick);
  interoception.actuatorFuelUnit =
      clamp01(static_cast<float>(actuatorWalletBytes) / std::max(strokeCost, 1.0f));
  interoception.locomotionFuelUnit =
      clamp01(static_cast<float>(locomotionFuelBytes) / std::max(strokeCost, 1.0f));
}

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

  const float mouthSignalGain =
      (prior.perceptorLocked && prior.perceptorSalience >= kOrganismCampReflexMinValence)
          ? kMouthTasteSignalGain * kMouthTasteVestigialGainWhenPerceptorLocked
          : kMouthTasteSignalGain;

  float bestMSteerScore = 0.0f;
  float mouthInboundUnit = 0.0f;
  bool haveMouthInbound = false;

  forEachInboundAxon(organism, actuatorId, simTick, true, [&](const InboundAxon& inbound) {
    if (!isNeuronConfidenceByte(inbound.axon.lastReceived.byte)) {
      return;
    }
    if (inbound.src.neuron == NeuronType::Mouth) {
      const float bite =
          approachContribution(inbound.axon.lastReceived.byte, inbound.weight, mouthSignalGain);
      accumulateApproachFlee(prior.approach, prior.flee, inbound.axon.lastReceived.byte,
                             inbound.weight, mouthSignalGain);
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

  if (const SkeletonNode* actuator = organism.findNode(actuatorId)) {
    seedActuatorLocomotionFuelInteroception(prior, campLocomotionFuelBytes(organism),
                                            static_cast<std::uint32_t>(actuator->store.size()));
  }

  prior.approach = clamp01(prior.approach);
  prior.flee = clamp01(prior.flee);
  prior.mouthConfidence = clamp01(prior.mouthConfidence);
  prior.hubSatiation = clamp01(prior.hubSatiation);
  prior.satiation = clamp01(prior.satiation);
  prior.mouthTasteApproach = clamp01(prior.mouthTasteApproach);
  return prior;
}

bool campLocomotionAnchored(const ActuatorInteroception& interoception) {
  const float minValence = kOrganismCampReflexMinValence;
  if (interoception.perceptorLocked && interoception.focusKind == PerceptFocusKind::Food &&
      interoception.approach > minValence) {
    return true;
  }
  if (interoception.perceptorLocked && interoception.focusKind == PerceptFocusKind::Threat &&
      interoception.flee > minValence) {
    return true;
  }
  return false;
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
                                  float coordinatorDutyScale) {
  MotorIntent intent;

  const float duty = clamp01(coordinatorDutyScale);

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

  if (campLocomotionAnchored(interoception)) {
    intent.tumbleRateScale = 0.0f;
  } else {
    intent.tumbleRateScale =
        clamp01(1.0f - interoception.approach * 0.65f + interoception.flee * 0.45f +
                std::max(0.0f, interoception.mouthSignalDelta) * 0.2f);
    intent.tumbleRateScale = clamp01(intent.tumbleRateScale * duty);
    if (interoception.mouthTasteSymmetricAmbiguity) {
      intent.tumbleRateScale =
          clamp01(intent.tumbleRateScale * kMouthTasteSymmetryTumbleBoost);
    }
  }

  const bool hubBrakeActive = interoception.hubSatiation >= hubBrake;

  const float maxBytes = static_cast<float>(kActuatorStrokeCostPerTick);
  const float energyFactor = clamp01(interoception.locomotionFuelUnit);
  float crawlDrive = clamp01((intent.netDrive + gradientGo * hunger) * duty);
  if (interoception.mouthTasteApproach > kOrganismCampReflexMinValence) {
    crawlDrive = std::max(crawlDrive, hunger * std::max(interoception.mouthTasteApproach,
                                                        kActuatorBaselineCrawlDrive));
  }
  const float strokeFloat = crawlDrive * maxBytes * energyFactor;

  if (strokeFloat >= kActuatorMotorIntentMinStroke * maxBytes &&
      interoception.locomotionFuelBytes > 0) {
    intent.strokeBytes =
        static_cast<std::uint32_t>(std::lround(std::min(strokeFloat, maxBytes)));
    intent.strokeBytes = std::clamp(intent.strokeBytes, 1u, kActuatorStrokeCostPerTick);
    if (intent.strokeBytes > interoception.locomotionFuelBytes) {
      intent.strokeBytes = interoception.locomotionFuelBytes;
    }
  }

  intent.motorSuppressed =
      intent.strokeBytes == 0 && interoception.locomotionFuelBytes > 0 && hubBrakeActive;

  return intent;
}

namespace {

void steerBodyTowardTarget(Organism& organism, float targetHeading, float turnRate) {
  applyCampBodyYawSteering(organism, targetHeading, turnRate);
}

}  // namespace

void applyCampChemotaxisSteering(Organism& organism, const ActuatorInteroception& interoception,
                                 const MotorIntent& intent) {
  const bool flee = interoception.flee > interoception.approach;
  const float drive = flee ? interoception.flee : interoception.approach;
  const float fleeOffset = flee ? 3.14159265f : 0.0f;

  if (interoception.perceptorLocked && intent.turnRateScale >= kOrganismCampReflexMinValence &&
      drive >= kOrganismCampReflexMinValence) {
    const float targetHeading =
        normalizeAngle(interoception.gazeHeading + interoception.focusBearing + fleeOffset);
    steerBodyTowardTarget(organism, targetHeading, intent.turnRateScale);
  }

  if (interoception.mouthTasteApproach > kOrganismCampReflexMinValence) {
    const bool perceptorAnchored =
        interoception.perceptorLocked &&
        (interoception.approach > kOrganismCampReflexMinValence ||
         interoception.flee > kOrganismCampReflexMinValence);
    const float mouthTurnScale =
        perceptorAnchored ? kMouthTasteVestigialTurnScale : 1.0f;
    const float blendDenom =
        std::max(interoception.approach + interoception.mouthTasteApproach, 1.0e-4f);
    const float mouthTurnWeight = interoception.mouthTasteApproach / blendDenom;
    const float tasteTurn =
        clamp01((interoception.mouthTasteApproach * kMouthTasteTurnGain +
                 std::max(0.0f, interoception.mouthTasteGradient) * 0.15f) *
                mouthTurnWeight * mouthTurnScale);
    const float targetHeading = normalizeAngle(campLocomotionBodyYaw(organism) +
                                               interoception.mouthTasteBearing + fleeOffset);
    steerBodyTowardTarget(organism, targetHeading, tasteTurn);
  }
}

}  // namespace evolab
