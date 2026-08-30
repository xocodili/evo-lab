#include "sim/OrganismMouth.hpp"



#include "engine/kinematics/Math.hpp"

#include "sim/CampNeuronGating.hpp"

#include "sim/CellConstants.hpp"

#include "sim/Energon.hpp"

#include "sim/EnergonString.hpp"

#include "sim/EnergonTasteSensory.hpp"

#include "sim/NeuralAxon.hpp"

#include "sim/NeuronSignal.hpp"

#include "sim/OrganismNeuron.hpp"

#include "sim/Organism.hpp"



#include <algorithm>

#include <cmath>



namespace evolab {



namespace {



using evolab::engine::kinematics::normalizeAngle;



float tasteSalienceFromDistance(float dist, float tasteRadius) {

  if (tasteRadius <= 1.0e-6f) {

    return 0.0f;

  }

  const float range01 = clamp01(dist / tasteRadius);

  return std::max(kMouthTasteSalienceFloor, 1.0f - range01 * 0.75f);

}



void sampleMouthTaste(SkeletonNode& mouth, const EnergonField& energon, float mouthX, float mouthZ,

                      float navX, float navZ, float heading, float tasteRadius) {

  const EnergonTasteSensoryPeak peak =

      energon.queryTasteSensoryPeak(mouthX, mouthZ, tasteRadius);



  if (!peak.valid || peak.cellBytes <= 1.0e-4f) {

    mouth.mouthTasteSymmetricAmbiguity = false;

    if (mouth.mouthTastePriorValid) {

      mouth.mouthTasteGradient = 0.0f - mouth.mouthTastePriorSalience;

    } else {

      mouth.mouthTasteGradient = 0.0f;

    }

    mouth.mouthTasteSalience = 0.0f;

    mouth.mouthTasteBearing = 0.0f;

    mouth.mouthTasteSampleValid = false;

    mouth.mouthTastePriorSalience = 0.0f;

    mouth.mouthTastePriorValid = false;

    return;

  }



  const float dx = peak.worldX - mouthX;

  const float dz = peak.worldZ - mouthZ;

  const float dist = std::sqrt(dx * dx + dz * dz);

  const float salience = tasteSalienceFromDistance(dist, tasteRadius);

  const float bearing = std::atan2(peak.worldX - navX, peak.worldZ - navZ);

  mouth.mouthTasteSymmetricAmbiguity = false;

  if (mouth.mouthTastePriorValid) {

    mouth.mouthTasteGradient = salience - mouth.mouthTastePriorSalience;

  } else {

    mouth.mouthTasteGradient = 0.0f;

  }

  mouth.mouthTasteSalience = clamp01(salience);

  mouth.mouthTasteBearing = normalizeAngle(bearing - heading);

  mouth.mouthTasteSampleValid = true;

  mouth.mouthTastePriorSalience = salience;

  mouth.mouthTastePriorValid = true;

}



}  // namespace



MouthInteroception gatherMouthInteroception(const Organism& organism, std::uint32_t mouthId,

                                            const SkeletonNode& mouth, std::uint64_t simTick) {

  MouthInteroception prior;

  prior.localSatiation = confidenceToUnit(mouthFuelConfidence(mouth));

  prior.tasteSalience = mouth.mouthTasteSalience;

  prior.tasteGradient = mouth.mouthTasteGradient;



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

  } else if (interoception.tasteSalience > kOrganismCampReflexMinValence ||

             interoception.tasteGradient > 0.0f) {

    const float tasteDrive =

        clamp01(interoception.tasteSalience * 0.65f +

                std::max(0.0f, interoception.tasteGradient) * kMouthTasteTemporalGain * 0.35f);

    go = hunger * std::max(kMouthBaselineFeedDrive, tasteDrive);

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

  const bool tasteGuided =

      !foodGuided && (interoception.tasteSalience > kOrganismCampReflexMinValence ||

                      interoception.tasteGradient > 0.0f);

  const float foodApproach = foodGuided ? interoception.approach : 0.0f;

  if (campMouthChewRefuseActive(mouthChewPaused, foodGuided || tasteGuided,

                                foodGuided ? foodApproach : interoception.tasteSalience)) {

    intent.allowFoodBite = false;

    intent.feedSuppressed = true;

  } else {

    intent.allowFoodBite = true;

    intent.feedSuppressed = intent.biteDrive < kMouthFeedIntentMinBite;

  }

  return intent;

}



void runMouthTastePhase(Organism& organism, const EnergonField& energon, float cellSize,

                        std::uint64_t simTick) {

  (void)simTick;

  if (!organism.alive || !organism.isCampNom()) {

    return;

  }



  const float tasteRadius = cellSize * kMouthTasteRadiusFactor;

  organism.lastMouthTasteSalience = 0.0f;

  organism.lastMouthTasteGradient = 0.0f;

  organism.lastMouthTasteBearing = 0.0f;

  organism.lastMouthTasteSymmetricAmbiguity = false;



  float navX = organism.rootWorldX();

  float navZ = organism.rootWorldZ();

  if (const SkeletonNode* perceptor = findFirstNeuronNode(organism, NeuronType::Perceptor, true)) {

    navX = perceptor->worldX;

    navZ = perceptor->worldZ;

  } else if (const SkeletonNode* actuator = findFirstNeuronNode(organism, NeuronType::Actuator, true)) {

    navX = actuator->worldX;

    navZ = actuator->worldZ;

  }



  for (SkeletonNode& node : organism.nodes) {

    if (!node.alive || node.neuron != NeuronType::Mouth) {

      continue;

    }

    sampleMouthTaste(node, energon, node.worldX, node.worldZ, navX, navZ, organism.heading,

                     tasteRadius);

    organism.lastMouthTasteSalience = std::max(organism.lastMouthTasteSalience, node.mouthTasteSalience);

    organism.lastMouthTasteGradient = std::max(organism.lastMouthTasteGradient, node.mouthTasteGradient);

    if (node.mouthTasteSampleValid &&

        node.mouthTasteSalience >= organism.lastMouthTasteSalience - 1.0e-4f) {

      organism.lastMouthTasteBearing = node.mouthTasteBearing;

      organism.lastMouthTasteSymmetricAmbiguity = node.mouthTasteSymmetricAmbiguity;

    }

  }

}



}  // namespace evolab

