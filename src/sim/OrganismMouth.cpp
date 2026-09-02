#include "sim/OrganismMouth.hpp"



#include "engine/kinematics/Math.hpp"

#include "sim/CampNeuronGating.hpp"

#include "sim/CellConstants.hpp"

#include "sim/Energon.hpp"

#include "sim/EnergonString.hpp"

#include "sim/EnergonTasteSensory.hpp"

#include "sim/NeuralAxon.hpp"

#include "sim/NeuronFuel.hpp"
#include "sim/NeuronSignal.hpp"
#include "sim/OrganismNeuron.hpp"
#include "sim/Organism.hpp"
#include "sim/PerceptorFocus.hpp"



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

bool organismPerceptorFoodLocked(const Organism& organism) {
  for (const SkeletonNode& node : organism.nodes) {
    if (node.alive && node.neuron == NeuronType::Perceptor && node.focusLocked &&
        node.focusKind == PerceptFocusKind::Food) {
      return true;
    }
  }
  return false;
}

void clearMouthTasteLatch(SkeletonNode& mouth) {
  mouth.mouthTasteLatchValid = false;
  mouth.mouthTasteLatchWorldX = 0.0f;
  mouth.mouthTasteLatchWorldZ = 0.0f;
  mouth.mouthTasteLatchPeakBytes = 0.0f;
  mouth.mouthTasteLatchTick = 0;
}

bool tryPayLatchSwitchCost(SkeletonNode& mouth) {
  if (mouth.store.size() < kMouthTasteLatchSwitchCostBytes) {
    return false;
  }
  neuronConsumeBack(mouth, kMouthTasteLatchSwitchCostBytes);
  return true;
}

void setMouthTasteLatch(SkeletonNode& mouth, const EnergonTasteSensoryPeak& peak,
                        std::uint64_t simTick) {
  mouth.mouthTasteLatchValid = true;
  mouth.mouthTasteLatchWorldX = peak.worldX;
  mouth.mouthTasteLatchWorldZ = peak.worldZ;
  mouth.mouthTasteLatchPeakBytes = peak.cellBytes;
  mouth.mouthTasteLatchTick = simTick;
}

EnergonTasteSensoryPeak peakFromLatch(const EnergonField& energon, const SkeletonNode& mouth) {
  EnergonTasteSensoryPeak latched;
  if (!mouth.mouthTasteLatchValid) {
    return latched;
  }
  latched.worldX = mouth.mouthTasteLatchWorldX;
  latched.worldZ = mouth.mouthTasteLatchWorldZ;
  latched.cellBytes = energon.queryTasteCellBytes(latched.worldX, latched.worldZ);
  latched.valid = latched.cellBytes > 1.0e-4f;
  return latched;
}

void applyMouthTasteOutputs(SkeletonNode& mouth, const EnergonField& energon, float mouthX,
                            float mouthZ, float heading, float tasteRadius,
                            const EnergonTasteSensoryPeak& activePeak) {
  if (!activePeak.valid || activePeak.cellBytes <= 1.0e-4f) {
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

  const float dx = activePeak.worldX - mouthX;
  const float dz = activePeak.worldZ - mouthZ;
  const float dist = std::sqrt(dx * dx + dz * dz);
  const float salience = tasteSalienceFromDistance(dist, tasteRadius);
  const float bearing = std::atan2(activePeak.worldX - mouthX, activePeak.worldZ - mouthZ);

  if (mouth.mouthTastePriorValid) {
    mouth.mouthTasteGradient = salience - mouth.mouthTastePriorSalience;
  } else {
    mouth.mouthTasteGradient = 0.0f;
  }

  const float avgVectorMagSq =
      energon.queryTasteResultantMagSq(mouthX, mouthZ, tasteRadius);
  mouth.mouthTasteSymmetricAmbiguity =
      salience >= kMouthTasteSalienceFloor &&
      avgVectorMagSq <= kMouthTasteSymmetryVectorEpsilonSq &&
      std::abs(mouth.mouthTasteGradient) <= kOrganismCampReflexMinValence;

  mouth.mouthTasteSalience = clamp01(salience);
  mouth.mouthTasteBearing = normalizeAngle(bearing - heading);
  mouth.mouthTasteSampleValid = true;
  mouth.mouthTastePriorSalience = salience;
  mouth.mouthTastePriorValid = true;
}

void sampleMouthTaste(SkeletonNode& mouth, const Organism& organism, const EnergonField& energon,
                      float mouthX, float mouthZ, float heading, float tasteRadius,
                      std::uint64_t simTick) {
  const EnergonTasteSensoryPeak challenger =
      energon.queryTasteSensoryPeak(mouthX, mouthZ, tasteRadius);

  if (mouth.mouthTasteLatchValid) {
    const float latchedMass =
        energon.queryTasteCellBytes(mouth.mouthTasteLatchWorldX, mouth.mouthTasteLatchWorldZ);
    const float quitMass =
        mouth.mouthTasteLatchPeakBytes * kMouthTasteLatchQuitMassFraction;
    const bool massGone = latchedMass <= quitMass;
    const bool timedOut =
        simTick > mouth.mouthTasteLatchTick &&
        simTick - mouth.mouthTasteLatchTick > kMouthTasteLatchMaxTicks;
    if (massGone || timedOut || organismPerceptorFoodLocked(organism) || mouth.ateThisTick) {
      clearMouthTasteLatch(mouth);
    }
  }

  EnergonTasteSensoryPeak activePeak = challenger;

  if (mouth.mouthTasteLatchValid) {
    EnergonTasteSensoryPeak latched = peakFromLatch(energon, mouth);
    if (!latched.valid) {
      clearMouthTasteLatch(mouth);
    } else {
      activePeak = latched;
      if (challenger.valid &&
          challenger.cellBytes >
              latched.cellBytes * (1.0f + kMouthTastePeakHysteresisFraction)) {
        if (tryPayLatchSwitchCost(mouth)) {
          setMouthTasteLatch(mouth, challenger, simTick);
          activePeak = challenger;
        }
      }
    }
  } else if (challenger.valid && challenger.cellBytes > 1.0e-4f) {
    const float dx = challenger.worldX - mouthX;
    const float dz = challenger.worldZ - mouthZ;
    const float dist = std::sqrt(dx * dx + dz * dz);
    const float acquireSalience = tasteSalienceFromDistance(dist, tasteRadius);
    const float avgVectorMagSq =
        energon.queryTasteResultantMagSq(mouthX, mouthZ, tasteRadius);
    const bool symmetric =
        acquireSalience >= kMouthTasteSalienceFloor &&
        avgVectorMagSq <= kMouthTasteSymmetryVectorEpsilonSq;
    if (!symmetric && acquireSalience >= kOrganismCampReflexMinValence) {
      setMouthTasteLatch(mouth, challenger, simTick);
      activePeak = challenger;
    }
  }

  applyMouthTasteOutputs(mouth, energon, mouthX, mouthZ, heading, tasteRadius, activePeak);

  if (mouth.mouthTasteSymmetricAmbiguity && mouth.mouthTasteLatchValid) {
    clearMouthTasteLatch(mouth);
  }
}



}  // namespace



MouthInteroception gatherMouthInteroception(const Organism& organism, std::uint32_t mouthId,

                                            const SkeletonNode& mouth, std::uint64_t simTick) {

  MouthInteroception prior;

  prior.localSatiation = confidenceToUnit(mouthFuelConfidence(mouth));

  prior.tasteSalience = mouth.mouthTasteSalience;

  prior.tasteGradient = mouth.mouthTasteGradient;

  prior.mouthChewPaused = mouth.mouthChewPaused;



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



FeedIntent computeCampFeedIntent(const MouthInteroception& interoception) {

  FeedIntent intent;

  bool mouthChewPaused = interoception.mouthChewPaused;



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

  intent.mouthChewPaused = mouthChewPaused;

  return intent;

}



void runMouthTastePhase(Organism& organism, const EnergonField& energon, float cellSize,
                        std::uint64_t simTick) {
  if (!organism.alive || !organism.isCampNom()) {
    return;
  }

  const float tasteRadius = cellSize * kMouthTasteRadiusFactor;
  organism.lastMouthTasteSalience = 0.0f;
  organism.lastMouthTasteGradient = 0.0f;
  organism.lastMouthTasteBearing = 0.0f;
  organism.lastMouthTasteSymmetricAmbiguity = false;

  for (SkeletonNode& node : organism.nodes) {

    if (!node.alive || node.neuron != NeuronType::Mouth) {

      continue;

    }

    sampleMouthTaste(node, organism, energon, node.worldX, node.worldZ, organism.heading,
                     tasteRadius, simTick);

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

