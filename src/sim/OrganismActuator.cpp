#include "sim/OrganismActuator.hpp"

#include "engine/kinematics/Math.hpp"
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

}  // namespace

ActuatorInteroception gatherActuatorInteroception(const Organism& organism,
                                                  std::uint32_t actuatorId,
                                                  std::uint64_t simTick) {
  ActuatorInteroception prior;

  const PerceptorMirror perceptor = readPerceptorMirror(organism);
  prior.perceptorLocked = perceptor.locked;
  prior.perceptorSalience = perceptor.salience;
  prior.focusBearing = perceptor.focusBearing;
  prior.gazeHeading = perceptor.gazeHeading;

  const float gain = perceptorGain(perceptor.locked, perceptor.salience);

  forEachInboundAxon(organism, actuatorId, simTick, true, [&](const InboundAxon& inbound) {
    if (!isNeuronConfidenceByte(inbound.axon.lastReceived.byte)) {
      return;
    }
    const float rawLevel = confidenceToUnit(inbound.axon.lastReceived.byte);
    const float level = rawLevel * inbound.weight;
    if (inbound.src.neuron == NeuronType::Mouth) {
      prior.mouthConfidence = std::max(prior.mouthConfidence, rawLevel);
      prior.satiation = std::max(prior.satiation, level);
    } else if (inbound.src.neuron == NeuronType::Perceptor) {
      accumulateApproachFlee(prior.approach, prior.flee, inbound.axon.lastReceived.byte,
                             inbound.weight, gain);
    }
  });

  prior.mouthConfidence = clamp01(prior.mouthConfidence);
  prior.approach = clamp01(prior.approach);
  prior.flee = clamp01(prior.flee);
  prior.satiation = clamp01(prior.satiation);
  return prior;
}

MotorIntent computePmaMotorIntent(const ActuatorInteroception& interoception,
                                  std::uint32_t actuatorFuelBytes) {
  MotorIntent intent;
  const float satiationBrake = confidenceToUnit(kMouthInhibitActuatorConfidence);
  const bool mouthBrakeActive = interoception.mouthConfidence >= satiationBrake;

  float motivation = 0.0f;
  if (mouthBrakeActive && interoception.approach > kOrganismPmaReflexMinValence) {
    motivation = 0.0f;
  } else if (mouthBrakeActive) {
    motivation = interoception.approach * (1.0f - interoception.mouthConfidence);
  } else {
    const float hungerGap = 1.0f - interoception.satiation * 0.5f;
    motivation = std::max(kActuatorBaselineCrawlDrive, interoception.approach) * hungerGap;
  }
  motivation *= 1.0f - interoception.flee * 0.85f;

  intent.netDrive = clamp01(motivation);
  intent.turnRateScale =
      std::max(interoception.approach, interoception.flee) *
      perceptorGain(interoception.perceptorLocked, interoception.perceptorSalience);
  intent.tumbleRateScale = clamp01(1.0f - interoception.approach * 0.65f +
                                   interoception.flee * 0.45f +
                                   interoception.mouthConfidence * 0.25f);

  const float maxBytes = static_cast<float>(kActuatorStrokeCostPerTick);
  const float energyFactor =
      clamp01(static_cast<float>(actuatorFuelBytes) / std::max(maxBytes, 1.0f));
  const float strokeFloat = intent.netDrive * maxBytes * energyFactor;

  if (strokeFloat >= kActuatorMotorIntentMinStroke * maxBytes && actuatorFuelBytes > 0) {
    intent.strokeBytes =
        static_cast<std::uint32_t>(std::lround(std::min(strokeFloat, maxBytes)));
    intent.strokeBytes = std::clamp(intent.strokeBytes, 1u, kActuatorStrokeCostPerTick);
    if (intent.strokeBytes > actuatorFuelBytes) {
      intent.strokeBytes = actuatorFuelBytes;
    }
  }

  intent.motorSuppressed = intent.strokeBytes == 0 && actuatorFuelBytes > 0 &&
                           (mouthBrakeActive ||
                            (interoception.approach > 0.15f &&
                             interoception.mouthConfidence > 0.45f));

  return intent;
}

void applyPmaChemotaxisHeading(Organism& organism, const ActuatorInteroception& interoception,
                               const MotorIntent& intent) {
  if (!interoception.perceptorLocked) {
    return;
  }
  if (intent.turnRateScale < kOrganismPmaReflexMinValence) {
    return;
  }

  const bool flee = interoception.flee > interoception.approach;
  const float drive = flee ? interoception.flee : interoception.approach;
  if (drive < kOrganismPmaReflexMinValence) {
    return;
  }

  const float fleeOffset = flee ? 3.14159265f : 0.0f;
  const float targetHeading =
      normalizeAngle(interoception.gazeHeading + interoception.focusBearing + fleeOffset);
  const float turnRate = kOrganismMaxTurnPerTick * intent.turnRateScale;
  organism.heading = turnToward(organism.heading, targetHeading, turnRate);
}

}  // namespace evolab
