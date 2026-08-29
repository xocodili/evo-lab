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

  if (interoception.perceptorLocked &&

      interoception.approach > kOrganismCampReflexMinValence) {

    return hunger * interoception.approach;

  }

  if (interoception.perceptorLocked) {

    return 0.0f;

  }

  return hunger * kActuatorBaselineCrawlDrive;

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

  forEachInboundAxon(organism, actuatorId, simTick, true, [&](const InboundAxon& inbound) {
    if (!isNeuronConfidenceByte(inbound.axon.lastReceived.byte)) {
      return;
    }
    const float rawLevel = confidenceToUnit(inbound.axon.lastReceived.byte);
    const float level = rawLevel * inbound.weight;
    if (inbound.src.neuron == NeuronType::Mouth) {
      prior.mouthConfidence = std::max(prior.mouthConfidence, rawLevel);
      prior.satiation = std::max(prior.satiation, level);
    } else if (inbound.src.neuron == NeuronType::Computer) {
      prior.hubSatiation = std::max(prior.hubSatiation, rawLevel);
      prior.satiation = std::max(prior.satiation, level);
    }
  });

  prior.mouthConfidence = clamp01(prior.mouthConfidence);
  prior.hubSatiation = clamp01(prior.hubSatiation);
  prior.satiation = clamp01(prior.satiation);
  return prior;
}



MotorIntent computeCampMotorIntent(const ActuatorInteroception& interoception,

                                  std::uint32_t actuatorFuelBytes) {

  MotorIntent intent;



  const float satiationBrake = confidenceToUnit(kMouthInhibitActuatorConfidence);

  const float hubBrake = confidenceToUnit(kComputerSatiationConfidence);

  const float mouthNoGo = campMouthChewNoGo(interoception.mouthConfidence);

  const float hubNoGo = campHubRepleteNoGo(interoception.hubSatiation);

  const float fleeNoGo = campLocomotionFleeNoGo(interoception.flee);

  const float hunger = campHungerFromMouthUnit(interoception.mouthConfidence);

  const float go = campActuatorGo(interoception, hunger);

  const float brakeLevel = std::max(interoception.mouthConfidence, interoception.hubSatiation);



  intent.netDrive = clamp01(go - mouthNoGo - hubNoGo - fleeNoGo);



  intent.turnRateScale =

      std::max(interoception.approach, interoception.flee) *

      perceptorGain(interoception.perceptorLocked, interoception.perceptorSalience);

  intent.tumbleRateScale = clamp01(1.0f - interoception.approach * 0.65f + interoception.flee * 0.45f +

                                   brakeLevel * 0.25f);



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



  const bool mouthBrakeActive = interoception.mouthConfidence >= satiationBrake;

  const bool hubBrakeActive = interoception.hubSatiation >= hubBrake;

  intent.motorSuppressed = intent.strokeBytes == 0 && actuatorFuelBytes > 0 &&

                             (mouthBrakeActive || hubBrakeActive ||

                              (interoception.approach > 0.15f && brakeLevel > 0.45f));



  return intent;

}



void applyCampChemotaxisHeading(Organism& organism, const ActuatorInteroception& interoception,

                               const MotorIntent& intent) {

  if (!interoception.perceptorLocked) {

    return;

  }

  if (intent.turnRateScale < kOrganismCampReflexMinValence) {

    return;

  }



  const bool flee = interoception.flee > interoception.approach;

  const float drive = flee ? interoception.flee : interoception.approach;

  if (drive < kOrganismCampReflexMinValence) {

    return;

  }



  const float fleeOffset = flee ? 3.14159265f : 0.0f;

  const float targetHeading =

      normalizeAngle(interoception.gazeHeading + interoception.focusBearing + fleeOffset);

  const float bearingError = std::abs(normalizeAngle(targetHeading - organism.heading));

  const float adaptScale = clamp01(bearingError / kOrganismCampChemotaxisAdaptRad);

  const float turnRate = kOrganismMaxTurnPerTick * intent.turnRateScale * adaptScale;

  organism.heading = turnToward(organism.heading, targetHeading, turnRate);

}



}  // namespace evolab

