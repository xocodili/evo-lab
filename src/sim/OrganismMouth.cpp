#include "sim/OrganismMouth.hpp"

#include "sim/CampNeuronGating.hpp"
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

  const AggregatedPerceptSignals percept =
      aggregatePerceptorInboundSignals(organism, mouthId, simTick, true);
  prior.approach = percept.approach;
  prior.flee = percept.flee;
  prior.perceptorLocked = percept.perceptorLocked;
  prior.perceptorSalience = percept.perceptorSalience;
  prior.focusKind = percept.focusKind;

  forEachInboundAxon(organism, mouthId, simTick, true, [&](const InboundAxon& inbound) {
    if (!isNeuronConfidenceByte(inbound.axon.lastReceived.byte)) {
      return;
    }
    const float rawLevel = confidenceToUnit(inbound.axon.lastReceived.byte);
    if (inbound.src.neuron == NeuronType::Actuator) {
      prior.actuatorActivity = std::max(prior.actuatorActivity, rawLevel * inbound.weight);
    }
  });

  prior.actuatorActivity = clamp01(prior.actuatorActivity);
  return prior;
}

FeedIntent computeCampFeedIntent(const MouthInteroception& interoception, bool& mouthChewPaused) {
  FeedIntent intent;

  const bool threatFocus =
      interoception.perceptorLocked && interoception.focusKind == PerceptFocusKind::Threat;
  const bool fleeDominant =
      interoception.flee > kOrganismCampReflexMinValence &&
      interoception.flee >= interoception.approach;

  if (threatFocus || fleeDominant) {
    intent.biteDrive = 0.0f;
    intent.allowFoodBite = false;
    intent.feedSuppressed = true;
    return intent;
  }

  updateMouthChewPause(mouthChewPaused, interoception.localSatiation);

  const float hunger = campHungerFromMouthUnit(interoception.localSatiation);
  float go = 0.0f;
  if (interoception.perceptorLocked && interoception.focusKind == PerceptFocusKind::Food &&
      interoception.approach > kOrganismCampReflexMinValence) {
    go = hunger * interoception.approach;
  } else if (!interoception.perceptorLocked ||
             interoception.focusKind != PerceptFocusKind::Food) {
    go = hunger * kMouthBaselineFeedDrive;
  }

  const float chewNoGo =
      mouthChewPaused ? interoception.localSatiation : campMouthChewNoGo(interoception.localSatiation);
  const float fleeNoGo = campFeedFleeNoGo(interoception.flee);
  const float activityNoGo = campActuatorActivityNoGo(interoception.actuatorActivity);

  intent.biteDrive = clamp01(go - chewNoGo - fleeNoGo - activityNoGo);

  const bool foodGuided =
      interoception.perceptorLocked && interoception.focusKind == PerceptFocusKind::Food;
  const float foodApproach = foodGuided ? interoception.approach : 0.0f;
  if (campMouthChewRefuseActive(mouthChewPaused, foodGuided, foodApproach)) {
    intent.allowFoodBite = false;
    intent.feedSuppressed = true;
  } else {
    intent.allowFoodBite = true;
    intent.feedSuppressed = intent.biteDrive < kMouthFeedIntentMinBite;
  }
  return intent;
}

}  // namespace evolab
