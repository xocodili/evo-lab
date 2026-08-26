#include "sim/OrganismMouth.hpp"

#include "sim/CellConstants.hpp"
#include "sim/NeuralAxon.hpp"
#include "sim/NeuronSignal.hpp"
#include "sim/OrganismNeuron.hpp"
#include "sim/Organism.hpp"

#include <algorithm>

namespace evolab {

MouthInteroception gatherMouthInteroception(const Organism& organism, std::uint32_t mouthId,
                                            const SkeletonNode& mouth, std::uint64_t simTick) {
  MouthInteroception prior;
  prior.localSatiation = confidenceToUnit(mouthFuelConfidence(mouth));

  const PerceptorMirror perceptor = readPerceptorMirror(organism);
  prior.perceptorLocked = perceptor.locked;
  prior.perceptorSalience = perceptor.salience;
  prior.focusKind = perceptor.focusKind;

  const float gain = perceptorGain(perceptor.locked, perceptor.salience);

  forEachInboundAxon(organism, mouthId, simTick, true, [&](const InboundAxon& inbound) {
    if (!isNeuronConfidenceByte(inbound.axon.lastReceived.byte)) {
      return;
    }
    const float rawLevel = confidenceToUnit(inbound.axon.lastReceived.byte);
    if (inbound.src.neuron == NeuronType::Perceptor) {
      accumulateApproachFlee(prior.approach, prior.flee, inbound.axon.lastReceived.byte,
                             inbound.weight, gain);
    } else if (inbound.src.neuron == NeuronType::Actuator) {
      prior.actuatorActivity = std::max(prior.actuatorActivity, rawLevel * inbound.weight);
    }
  });

  prior.approach = clamp01(prior.approach);
  prior.flee = clamp01(prior.flee);
  prior.actuatorActivity = clamp01(prior.actuatorActivity);
  return prior;
}

FeedIntent computePmaFeedIntent(const MouthInteroception& interoception) {
  FeedIntent intent;
  const float storeBrake = confidenceToUnit(kMouthInhibitActuatorConfidence);
  const bool storeFull = interoception.localSatiation >= storeBrake;
  const bool threatFocus =
      interoception.perceptorLocked && interoception.focusKind == PerceptFocusKind::Threat;
  const bool fleeDominant =
      interoception.flee > kOrganismPmaReflexMinValence &&
      interoception.flee >= interoception.approach;

  if (threatFocus || fleeDominant) {
    intent.biteDrive = 0.0f;
    intent.allowFoodBite = false;
    intent.feedSuppressed = true;
    return intent;
  }

  float appetite = 0.0f;
  if (storeFull && interoception.approach <= kOrganismPmaReflexMinValence) {
    appetite = 0.0f;
  } else {
    const float hungerGap = 1.0f - interoception.localSatiation * 0.5f;
    appetite = std::max(kMouthBaselineFeedDrive, interoception.approach) * hungerGap;
    appetite *= 1.0f - interoception.flee * 0.9f;
    appetite *= 1.0f - interoception.actuatorActivity * 0.15f;
  }

  intent.biteDrive = clamp01(appetite);
  intent.allowFoodBite = intent.biteDrive >= kMouthFeedIntentMinBite;
  intent.feedSuppressed = !intent.allowFoodBite;
  return intent;
}

}  // namespace evolab
