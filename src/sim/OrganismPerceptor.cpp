#include "sim/CampTopology.hpp"
#include "sim/OrganismPerceptor.hpp"



#include "sim/BarrenWorld.hpp"

#include "sim/CloacaSignal.hpp"
#include "sim/CellConstants.hpp"

#include "sim/Chaos.hpp"

#include "sim/Energon.hpp"

#include "sim/EnergonString.hpp"

#include "sim/NeuralAxon.hpp"

#include "sim/Organism.hpp"

#include "sim/NeuronStem.hpp"

#include "sim/NeuronFuel.hpp"
#include "sim/OrganismNeuron.hpp"
#include "sim/NeuronTrust.hpp"
#include "sim/PerceptorFocus.hpp"
#include "sim/NeuronSignal.hpp"

#include "sim/TideAdvection.hpp"



#include <algorithm>

#include <cmath>

#include <random>

#include <vector>



namespace evolab {



const char* perceptFocusKindLabel(PerceptFocusKind kind) {

  switch (kind) {

    case PerceptFocusKind::Food:

      return "food";

    case PerceptFocusKind::Mate:

      return "mate";

    case PerceptFocusKind::Threat:

      return "threat";

    default:

      return "none";

  }

}



namespace {



struct PerceptCandidate {

  PerceptFocusKind kind = PerceptFocusKind::None;

  float relBearing = 0.0f;

  float range01 = 1.0f;

  float salience = 0.0f;

};



struct InteroceptionPrior {

  float hunger = 0.0f;

  float satiation = 0.0f;

  float movementSmear = 0.0f;

  // Proprioceptive body state (not inbound axon bytes).
  float bodyHunger = 0.0f;
  float perceptorFuelUnit = 0.0f;
  std::uint32_t perceptorFuelBytes = 0;
  std::uint32_t maxScanPaymentBytes = 0;
  bool canAffordMaxScanPayment = false;
  std::uint32_t scanPaymentBytes = 0;
  bool canAffordPayment = false;
  bool selfMateReady = false;

};

std::uint32_t perceptorScanPaymentBytes(const Organism& organism, const SkeletonNode& perceptor,
                                        bool transductionDue) {
  std::uint32_t bytesDue = kPerceptorScanCostPerTick;
  if (transductionDue) {
    bytesDue += kPerceptorTransductionCostPerTick;
  }
  if (organism.isCampNom() && perceptor.coordinatorDutyScale < kCoordinatorMaxDutyScale - 1.0e-4f) {
    bytesDue = std::max<std::uint32_t>(
        1u, static_cast<std::uint32_t>(std::lround(static_cast<float>(bytesDue) *
                                                     perceptor.coordinatorDutyScale)));
  }
  return bytesDue;
}

void seedPerceptorScanPaymentInteroception(InteroceptionPrior& prior, std::uint32_t bytesDue) {
  prior.scanPaymentBytes = bytesDue;
  prior.canAffordPayment = prior.perceptorFuelBytes >= bytesDue;
}



struct LocalFocus {

  PerceptFocusKind kind = PerceptFocusKind::None;

  float relBearing = 0.0f;

  float range01 = 1.0f;

  float salience = 0.0f;

  bool locked = false;

};



float normalizeAngle(float radians) {

  constexpr float kTwoPi = 6.2831853f;

  while (radians > 3.14159265f) {

    radians -= kTwoPi;

  }

  while (radians < -3.14159265f) {

    radians += kTwoPi;

  }

  return radians;

}



float clamp01(float value) {

  return std::clamp(value, 0.0f, 1.0f);

}



bool pointInFocusCone(float originX, float originZ, float heading, float halfAngle, float range,

                      float px, float pz, float& relBearing, float& range01) {

  const float dx = px - originX;

  const float dz = pz - originZ;

  const float distSq = dx * dx + dz * dz;

  if (distSq < 1.0e-8f) {

    return false;

  }

  const float maxRangeSq = range * range;

  if (distSq > maxRangeSq) {

    return false;

  }

  const float dist = std::sqrt(distSq);

  const float bearing = std::atan2(dx, dz);

  relBearing = normalizeAngle(bearing - heading);

  if (std::abs(relBearing) > halfAngle) {

    return false;

  }

  range01 = dist / range;

  return true;

}



float diurnalRadiusScale(float sunIntensity) {

  const float sun = clamp01(sunIntensity);

  return kPerceptDiurnalRadiusFloor + (1.0f - kPerceptDiurnalRadiusFloor) * sun;

}



float perceptNoiseSigma(float sunIntensity) {

  const float sun = clamp01(sunIntensity);

  return kPerceptNoiseBearingRad * (1.0f + kPerceptNightChaosGain * (1.0f - sun));

}



float baseSalienceForKind(PerceptFocusKind kind) {

  switch (kind) {

    case PerceptFocusKind::Threat:

      return 1.15f;

    case PerceptFocusKind::Mate:

      return 0.9f;

    case PerceptFocusKind::Food:

      return 1.0f;

    default:

      return 0.0f;

  }

}



void addNoisyCandidate(std::vector<PerceptCandidate>& out, PerceptFocusKind kind, float relBearing,

                       float range01, float sunIntensity, std::mt19937& rng) {

  if (kind == PerceptFocusKind::None) {

    return;

  }



  const float sun = clamp01(sunIntensity);

  if (chaosBernoulli(kPerceptFalseNegativeNightRate * (1.0f - sun), rng)) {

    return;

  }



  std::normal_distribution<float> noise(0.0f, perceptNoiseSigma(sunIntensity));

  float noisyBearing = normalizeAngle(relBearing + noise(rng));

  if (std::abs(noisyBearing) > kPerceptorFocusHalfAngle) {

    return;

  }



  const float noisyRange = clamp01(range01 + noise(rng) * 0.08f);

  const float rangeFactor =
      std::max(kPerceptorRangeSalienceFloor, 1.0f - noisyRange * 0.75f);
  const float salience = rangeFactor * baseSalienceForKind(kind);

  if (salience <= 1.0e-4f) {

    return;

  }



  PerceptCandidate candidate;

  candidate.kind = kind;

  candidate.relBearing = noisyBearing;

  candidate.range01 = noisyRange;

  candidate.salience = salience;

  out.push_back(candidate);

}



float organismHungerPrior(const InteroceptionPrior& prior) {

  return prior.bodyHunger;

}



void scanFieldEnergon(const Organism& self, const SkeletonNode& perceptor, float gazeHeading,

                      float senseRadius, const EnergonField& energon, float sunIntensity,

                      std::uint64_t simTick, std::mt19937& rng,

                      const InteroceptionPrior& prior,

                      std::vector<PerceptCandidate>& out) {

  const bool selfMateReady = prior.selfMateReady;

  const float hunger = prior.bodyHunger;

  energon.forEachBlobNear(

      perceptor.worldX, perceptor.worldZ, senseRadius, [&](const EnergonBlob& blob) {

        if (blob.remaining == 0) {

          return;

        }

        float t = 0.0f;

        const float distSq =

            energonPointSegmentDistanceSq(perceptor.worldX, perceptor.worldZ, blob, t);

        if (distSq > senseRadius * senseRadius) {

          return;

        }

        const float closestX = blob.tailX + t * (blob.headX - blob.tailX);

        const float closestZ = blob.tailZ + t * (blob.headZ - blob.tailZ);

        float relBearing = 0.0f;

        float range01 = 0.0f;

        if (!pointInFocusCone(perceptor.worldX, perceptor.worldZ, gazeHeading,

                              kPerceptorFocusHalfAngle, senseRadius, closestX, closestZ,

                              relBearing, range01)) {

          return;

        }

        const CloacaBand band = cloacaBandFromBlob(blob);

        if (band != CloacaBand::None) {

          switch (band) {

            case CloacaBand::Distress:

              if (hunger > 0.5f) {

                addNoisyCandidate(out, PerceptFocusKind::Food, relBearing, range01, sunIntensity,

                                  rng);

              } else {

                addNoisyCandidate(out, PerceptFocusKind::Threat, relBearing, range01, sunIntensity,

                                  rng);

              }

              break;

            case CloacaBand::Mate:

              if (selfMateReady) {

                addNoisyCandidate(out, PerceptFocusKind::Mate, relBearing, range01, sunIntensity,

                                  rng);

              }

              break;

            case CloacaBand::Baseline:

              break;

            default:

              break;

          }

          return;

        }

        if (blob.origin == EnergonOrigin::Sunfall || blob.origin == EnergonOrigin::Fragment) {

          addNoisyCandidate(out, PerceptFocusKind::Food, relBearing, range01, sunIntensity, rng);

        }

      });

}



void scanOrganisms(const Organism& self, const SkeletonNode& perceptor, float gazeHeading,

                   float senseRadius, const std::vector<Organism>& population, float sunIntensity,

                   std::uint64_t simTick, std::mt19937& rng, const InteroceptionPrior& prior,

                   std::vector<PerceptCandidate>& out) {

  if (!prior.selfMateReady) {

    return;

  }

  const float broadRadius = senseRadius * 1.5f;

  const float broadRadiusSq = broadRadius * broadRadius;

  for (const Organism& other : population) {

    if (!other.alive || other.id == self.id) {

      continue;

    }

    const float dx = other.rootWorldX() - perceptor.worldX;

    const float dz = other.rootWorldZ() - perceptor.worldZ;

    if (dx * dx + dz * dz > broadRadiusSq) {

      continue;

    }

    for (const SkeletonNode& node : other.nodes) {

      if (!node.alive || node.neuron == NeuronType::None) {

        continue;

      }

      float relBearing = 0.0f;

      float range01 = 0.0f;

      if (!pointInFocusCone(perceptor.worldX, perceptor.worldZ, gazeHeading,

                            kPerceptorFocusHalfAngle, senseRadius, node.worldX, node.worldZ,

                            relBearing, range01)) {

        continue;

      }

      addNoisyCandidate(out, PerceptFocusKind::Mate, relBearing, range01, sunIntensity, rng);

    }

  }

}



void scanBlocks(const BarrenWorld& world, const SkeletonNode& perceptor, float gazeHeading,

                float senseRadius, float cellSize, float halfExtent, float sunIntensity,

                std::mt19937& rng, std::vector<PerceptCandidate>& out) {

  const float fx = std::sin(gazeHeading);

  const float fz = std::cos(gazeHeading);

  const float samples[] = {0.35f, 0.6f, 0.85f, 1.0f};

  for (float fraction : samples) {

    const float probeX = perceptor.worldX + fx * senseRadius * fraction;

    const float probeZ = perceptor.worldZ + fz * senseRadius * fraction;

    float clampedX = probeX;

    float clampedZ = probeZ;

    clampWorldPosition(clampedX, clampedZ, halfExtent, cellSize * 0.25f);

    const bool atBoundary =

        std::abs(clampedX - probeX) > 1.0e-3f || std::abs(clampedZ - probeZ) > 1.0e-3f;

    const bool dry = !world.isWetWorld(clampedX, clampedZ, cellSize);

    if (!atBoundary && !dry) {

      continue;

    }

    float relBearing = 0.0f;

    float range01 = 0.0f;

    if (!pointInFocusCone(perceptor.worldX, perceptor.worldZ, gazeHeading,

                          kPerceptorFocusHalfAngle, senseRadius, clampedX, clampedZ, relBearing,

                          range01)) {

      continue;

    }

    addNoisyCandidate(out, PerceptFocusKind::Threat, relBearing, range01, sunIntensity, rng);

  }

}



InteroceptionPrior gatherInteroception(const Organism& organism, const SkeletonNode& perceptor,

                                       std::uint64_t simTick) {

  InteroceptionPrior prior;

  forEachInboundAxon(organism, perceptor.id, 0, false, [&](const InboundAxon& inbound) {
    if (!isNeuronConfidenceByte(inbound.axon.lastReceived.byte)) {
      return;
    }

    const float level = confidenceToUnit(inbound.axon.lastReceived.byte) * inbound.weight;

    if (inbound.src.neuron == NeuronType::Mouth) {
      prior.satiation = std::max(prior.satiation, level);
      prior.hunger = std::max(prior.hunger,
                              (1.0f - confidenceToUnit(inbound.axon.lastReceived.byte)) *
                                  inbound.weight);
    } else if (inbound.src.neuron == NeuronType::Actuator) {
      prior.movementSmear = std::max(prior.movementSmear, level);
    }
  });

  prior.hunger = clamp01(prior.hunger);

  prior.satiation = clamp01(prior.satiation);

  prior.movementSmear = clamp01(prior.movementSmear);

  prior.perceptorFuelBytes = static_cast<std::uint32_t>(perceptor.store.size());
  const std::size_t perceptorCap =
      std::max<std::size_t>(peripheralStoreCapBytes(organism), 1u);
  prior.perceptorFuelUnit =
      clamp01(static_cast<float>(prior.perceptorFuelBytes) / static_cast<float>(perceptorCap));
  prior.maxScanPaymentBytes = perceptorScanPaymentBytes(organism, perceptor, true);
  prior.canAffordMaxScanPayment = prior.perceptorFuelBytes >= prior.maxScanPaymentBytes;

  if (const SkeletonNode* mouth = findNeuronNode(organism, NeuronType::Mouth)) {
    prior.bodyHunger = 1.0f - confidenceToUnit(mouthFuelConfidence(*mouth));
  } else {
    prior.bodyHunger = 0.5f;
  }
  prior.bodyHunger = clamp01(prior.bodyHunger);

  CampBodyInteroception bodyInteroception;
  gatherCampBodyInteroception(organism, simTick, bodyInteroception);
  prior.selfMateReady = bodyInteroception.mateReady;

  return prior;

}



struct PriorFocus {
  PerceptFocusKind kind = PerceptFocusKind::None;
  bool locked = false;
};

float perceptCandidateGoNoGoScore(const PerceptCandidate& candidate, const InteroceptionPrior& prior,
                                  float jitter) {
  const float smear = 1.0f - prior.movementSmear * 0.25f;
  const float salience = candidate.salience * jitter * smear;
  switch (candidate.kind) {
    case PerceptFocusKind::Food: {
      const float go = salience * (0.45f + prior.hunger * 0.85f);
      const float nogo = salience * prior.satiation * 0.55f;
      return go - nogo;
    }
    case PerceptFocusKind::Threat:
      return salience * (1.05f + prior.satiation * 0.1f);
    case PerceptFocusKind::Mate:
      return salience * (0.75f + prior.hunger * 0.15f);
    default:
      return 0.0f;
  }
}

float bestFoodGoNoGoScore(const std::vector<PerceptCandidate>& candidates,
                          const InteroceptionPrior& prior) {
  float best = 0.0f;
  for (const PerceptCandidate& candidate : candidates) {
    if (candidate.kind != PerceptFocusKind::Food) {
      continue;
    }
    best = std::max(best, perceptCandidateGoNoGoScore(candidate, prior, 1.0f));
  }
  return best;
}

LocalFocus focusFromCandidate(const PerceptCandidate& candidate) {
  LocalFocus focus;
  focus.kind = candidate.kind;
  focus.relBearing = candidate.relBearing;
  focus.range01 = candidate.range01;
  focus.salience = candidate.salience;
  focus.locked = true;
  return focus;
}



LocalFocus integrateFocus(const std::vector<PerceptCandidate>& candidates,

                          const InteroceptionPrior& prior, PriorFocus priorFocus,

                          std::mt19937& rng) {

  LocalFocus focus;

  float bestScore = 0.0f;

  LocalFocus bestCandidate;

  bool hasBest = false;

  float kindScores[4] = {};

  LocalFocus kindFocus[4];



  for (const PerceptCandidate& candidate : candidates) {

    const float score =
        perceptCandidateGoNoGoScore(candidate, prior, chaosJitterFloat(1.0f, rng));

    const int kindIdx = static_cast<int>(candidate.kind);

    if (kindIdx >= 1 && kindIdx <= 3 && score >= kindScores[kindIdx]) {

      kindScores[kindIdx] = score;

      kindFocus[kindIdx] = focusFromCandidate(candidate);

    }

    if (score > bestScore) {

      bestScore = score;

      bestCandidate = focusFromCandidate(candidate);

      hasBest = true;

    }

  }



  const float acquireThreshold = kPerceptorFocusLockThreshold + prior.movementSmear * 0.05f;

  const float releaseThreshold = kPerceptorFocusReleaseThreshold;



  if (priorFocus.locked && priorFocus.kind != PerceptFocusKind::None) {

    const int holdIdx = static_cast<int>(priorFocus.kind);

    if (kindScores[holdIdx] >= releaseThreshold) {

      return kindFocus[holdIdx];

    }

  }



  if (hasBest && bestScore >= acquireThreshold) {

    return bestCandidate;

  }



  focus.salience = bestScore;

  return focus;

}



void applyFoodTemporalGradient(float& base, float deltaSalience, bool deltaValid,
                               bool foodChannelActive) {
  if (!deltaValid || !foodChannelActive) {
    return;
  }
  base += deltaSalience * kPerceptTemporalGradientGain;
}

std::uint8_t focusToConfidence(const LocalFocus& focus, const InteroceptionPrior& prior,
                               float foodSalienceDelta, bool foodSalienceDeltaValid,
                               bool foodChannelActive, std::mt19937& rng) {

  constexpr float kNeutral = static_cast<float>(kNeuronConfidenceNeutral);

  float base = kNeutral;



  if (focus.locked) {

    switch (focus.kind) {

      case PerceptFocusKind::Food:

        base = kNeutral + focus.salience * 3.0f * (0.35f + prior.hunger * 0.65f);

        base -= prior.satiation * 1.2f;

        applyFoodTemporalGradient(base, foodSalienceDelta, foodSalienceDeltaValid,
                                  foodChannelActive);

        break;

      case PerceptFocusKind::Mate:

        base = kNeutral + focus.salience * 2.2f;

        break;

      case PerceptFocusKind::Threat:

        base = kNeutral - focus.salience * 3.6f;

        break;

      default:

        break;

    }

  } else if (prior.hunger > 0.35f && prior.satiation < 0.2f) {

    base = kNeutral + prior.hunger * 0.6f;

    applyFoodTemporalGradient(base, foodSalienceDelta, foodSalienceDeltaValid,
                              foodChannelActive);

  }



  base += (chaosJitterMultiplier(rng) - 1.0f) * 0.35f;

  const int rounded = static_cast<int>(std::lround(base));

  return static_cast<std::uint8_t>(std::clamp(rounded, 0, static_cast<int>(kNeuronConfidenceMax)));

}



void resetPerceptorNode(SkeletonNode& perceptor) {

  perceptor.focusKind = PerceptFocusKind::None;

  perceptor.focusBearing = 0.0f;

  perceptor.focusRange = 1.0f;

  perceptor.focusSalience = 0.0f;

  perceptor.focusLocked = false;

  perceptor.perceptConfidence = 0;

}



void syncOrganismPerceptMirror(Organism& organism, const SkeletonNode& perceptor) {

  organism.lastPerceptConfidence = perceptor.perceptConfidence;

  organism.lastPerceptDiurnalConfidence = perceptor.perceptDiurnalConfidence;

  organism.lastPerceptSunIntensity = perceptor.perceptSunIntensity;

  organism.lastPerceptFocusKind = perceptor.focusKind;

  organism.lastPerceptBearing = perceptor.focusBearing;

  organism.lastPerceptRange = perceptor.focusRange;

}



void emitPerceptorOutboundSignals(Organism& organism, std::uint32_t perceptorId,

                                  SkeletonNode& perceptor, std::uint64_t simTick) {

  static constexpr NeuronType kAllowedDst[] = {NeuronType::Mouth, NeuronType::Actuator,

                                               NeuronType::Computer};

  for (NeuralAxon& axon : organism.neuralAxons) {

    if (axon.srcNodeId != perceptorId) {

      continue;

    }

    const SkeletonNode* dst = organism.findNode(axon.dstNodeId);

    if (dst == nullptr || !dst->alive || axon.uncappedNodeId == axon.dstNodeId ||

        axon.uncappedNodeId == axon.srcNodeId) {

      continue;

    }

    bool allowed = false;

    for (NeuronType allowedDst : kAllowedDst) {

      if (dst->neuron == allowedDst) {

        allowed = true;

        break;

      }

    }

    if (!allowed) {

      continue;

    }

    const std::uint8_t confidence =

        perceptorOutboundConfidenceForDst(perceptor, dst->neuron);

    if (!isNeuronConfidenceByte(confidence)) {

      continue;

    }

    writeAxonConfidence(axon, confidence, simTick);

    perceptor.lastEmittedByte = confidence;

  }

}



bool emitTorporDiurnalOnly(Organism& organism, SkeletonNode& perceptor, float sunIntensity,

                           std::uint64_t simTick, std::mt19937& rng) {

  if (!organism.isCampNom() ||

      organism.famineUnit < kCoordinatorDeepTorporFamineThreshold) {

    return false;

  }

  const float depth =

      clamp01((organism.famineUnit - kCoordinatorDeepTorporFamineThreshold) /

              (1.0f - kCoordinatorDeepTorporFamineThreshold));

  const float skipProb = depth * kTorporScanSkipMaxProbability;

  if (!chaosBernoulli(skipProb, rng)) {

    return false;

  }

  perceptor.perceptDiurnalConfidence = diurnalLightConfidence(sunIntensity);

  perceptor.perceptSunIntensity = sunIntensity;

  organism.lastPerceptScanPaid = false;

  organism.lastPerceptBytesPaid = 0;

  syncOrganismPerceptMirror(organism, perceptor);

  emitPerceptorOutboundSignals(organism, perceptor.id, perceptor, simTick);

  return true;

}



void runPerceptorForNode(Organism& organism, SkeletonNode& perceptor, const BarrenWorld& world,

                         const EnergonField& energon, float cellSize, float halfExtent,

                         const std::vector<Organism>& population, std::uint64_t simTick,

                         float sunIntensity) {

  const PriorFocus priorFocus{perceptor.focusKind, perceptor.focusLocked};

  resetPerceptorNode(perceptor);



  // v1: gaze coupled to body heading; decoupled per-P gaze is reserved for duplicate P.

  perceptor.gazeHeading = organism.heading;



  std::mt19937 rng =

      chaosSpawnRng(simTick, static_cast<std::uint64_t>(organism.id) ^

                                (static_cast<std::uint64_t>(perceptor.id) << 16) ^ kChaosSaltNom);



  if (emitTorporDiurnalOnly(organism, perceptor, sunIntensity, simTick, rng)) {

    return;

  }



  InteroceptionPrior prior = gatherInteroception(organism, perceptor, simTick);



  if (!prior.canAffordMaxScanPayment) {

    return;

  }



  perceptor.perceptDiurnalConfidence = diurnalLightConfidence(sunIntensity);

  perceptor.perceptSunIntensity = sunIntensity;



  const float effectiveRadius =

      cellSize * organism.senseRadiusFactor * diurnalRadiusScale(sunIntensity);



  std::vector<PerceptCandidate> candidates;

  candidates.reserve(16);

  scanFieldEnergon(organism, perceptor, perceptor.gazeHeading, effectiveRadius, energon,

                   sunIntensity, simTick, rng, prior, candidates);

  scanOrganisms(organism, perceptor, perceptor.gazeHeading, effectiveRadius, population,

                sunIntensity, simTick, rng, prior, candidates);

  if (!organism.disableTerrainThreatScan) {
    scanBlocks(world, perceptor, perceptor.gazeHeading, effectiveRadius, cellSize, halfExtent,
               sunIntensity, rng, candidates);
  }



  const std::uint32_t bytesDue =
      perceptorScanPaymentBytes(organism, perceptor, !candidates.empty());
  seedPerceptorScanPaymentInteroception(prior, bytesDue);

  if (!prior.canAffordPayment) {

    return;

  }



  neuronConsumeBack(perceptor, bytesDue);

  organism.lastPerceptScanPaid = true;

  organism.lastPerceptBytesPaid = bytesDue;



  const float bestFoodScore = bestFoodGoNoGoScore(candidates, prior);
  const bool foodChannelActive =
      bestFoodScore > 0.0f || perceptor.perceptPriorFoodSalienceValid;
  float foodSalienceDelta = 0.0f;
  bool foodSalienceDeltaValid = false;
  if (perceptor.perceptPriorFoodSalienceValid) {
    foodSalienceDelta = bestFoodScore - perceptor.perceptPriorFoodSalience;
    foodSalienceDeltaValid = true;
  }

  const LocalFocus focus = integrateFocus(candidates, prior, priorFocus, rng);

  const std::uint8_t confidence =
      focusToConfidence(focus, prior, foodSalienceDelta, foodSalienceDeltaValid,
                        foodChannelActive, rng);



  perceptor.focusKind = focus.kind;

  perceptor.focusBearing = focus.relBearing;

  perceptor.focusRange = focus.range01;

  perceptor.focusSalience = focus.salience;

  perceptor.focusLocked = focus.locked;

  perceptor.perceptConfidence = confidence;

  perceptor.lastEmittedByte = confidence;



  syncOrganismPerceptMirror(organism, perceptor);

  emitPerceptorOutboundSignals(organism, perceptor.id, perceptor, simTick);

  bool hadFoodCandidate = false;
  for (const PerceptCandidate& candidate : candidates) {
    if (candidate.kind == PerceptFocusKind::Food) {
      hadFoodCandidate = true;
      break;
    }
  }

  PerceptorTrustEvent trustEvent;
  trustEvent.scanPaid = true;
  trustEvent.hadFoodCandidate = hadFoodCandidate;
  trustEvent.focusLocked = focus.locked;
  trustEvent.focusKind = focus.kind;
  trustEvent.confidence = confidence;
  applyCampPerceptorTrustLearning(organism, perceptor.id, trustEvent, simTick);

  perceptor.perceptPriorFoodSalience = bestFoodScore;
  perceptor.perceptPriorFoodSalienceValid = true;

}

}  // namespace

void runPerceptorPhase(Organism& organism, const BarrenWorld& world, const EnergonField& energon,

                       float cellSize, float halfExtent, const std::vector<Organism>& population,

                       std::uint64_t simTick, float sunIntensity) {

  if (!organism.alive || !organismUsesCampNeuronPhases(organism)) {

    return;

  }



  organism.lastPerceptConfidence = 0;

  organism.lastPerceptDiurnalConfidence = 0;

  organism.lastPerceptSunIntensity = 0.0f;

  organism.lastPerceptFocusKind = PerceptFocusKind::None;

  organism.lastPerceptBearing = 0.0f;

  organism.lastPerceptRange = 0.0f;

  organism.lastPerceptScanPaid = false;

  organism.lastPerceptBytesPaid = 0;



  for (SkeletonNode& node : organism.nodes) {

    if (!node.alive || node.neuron != NeuronType::Perceptor) {

      continue;

    }

    runPerceptorForNode(organism, node, world, energon, cellSize, halfExtent, population,

                        simTick, sunIntensity);

  }

}



}  // namespace evolab

