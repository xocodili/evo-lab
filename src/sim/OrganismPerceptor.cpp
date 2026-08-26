#include "sim/OrganismPerceptor.hpp"



#include "sim/BarrenWorld.hpp"

#include "sim/CellConstants.hpp"

#include "sim/Chaos.hpp"

#include "sim/Energon.hpp"

#include "sim/EnergonString.hpp"

#include "sim/NeuralAxon.hpp"

#include "sim/Organism.hpp"

#include "sim/OrganismInternal.hpp"

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

};



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

  const float salience = (1.0f - noisyRange) * baseSalienceForKind(kind);

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



void scanFood(const SkeletonNode& perceptor, float gazeHeading, float senseRadius,

              const EnergonField& energon, float sunIntensity, std::mt19937& rng,

              std::vector<PerceptCandidate>& out) {

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

        addNoisyCandidate(out, PerceptFocusKind::Food, relBearing, range01, sunIntensity, rng);

      });

}



void scanOrganisms(const Organism& self, const SkeletonNode& perceptor, float gazeHeading,

                   float senseRadius, const std::vector<Organism>& population, float sunIntensity,

                   std::mt19937& rng, std::vector<PerceptCandidate>& out) {

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



InteroceptionPrior gatherInteroception(const Organism& organism, std::uint32_t perceptorId) {

  InteroceptionPrior prior;

  forEachInboundAxon(organism, perceptorId, 0, false, [&](const InboundAxon& inbound) {
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

  return prior;

}



float candidateCompetitionWeight(const PerceptCandidate& candidate,

                                 const InteroceptionPrior& prior, std::mt19937& rng) {

  float weight = candidate.salience;

  switch (candidate.kind) {

    case PerceptFocusKind::Food:

      weight *= 0.45f + prior.hunger * 0.85f;

      weight *= 1.0f - prior.satiation * 0.55f;

      break;

    case PerceptFocusKind::Mate:

      weight *= 0.75f + prior.hunger * 0.15f;

      break;

    case PerceptFocusKind::Threat:

      weight *= 1.05f + prior.satiation * 0.1f;

      break;

    default:

      return 0.0f;

  }

  weight *= 1.0f - prior.movementSmear * 0.25f;

  weight *= chaosJitterFloat(1.0f, rng);

  return weight;

}



LocalFocus integrateFocus(const std::vector<PerceptCandidate>& candidates,

                          const InteroceptionPrior& prior, std::mt19937& rng) {

  LocalFocus focus;

  float bestWeight = 0.0f;

  for (const PerceptCandidate& candidate : candidates) {

    const float weight = candidateCompetitionWeight(candidate, prior, rng);

    if (weight > bestWeight) {

      bestWeight = weight;

      focus.kind = candidate.kind;

      focus.relBearing = candidate.relBearing;

      focus.range01 = candidate.range01;

      focus.salience = candidate.salience;

      focus.locked = true;

    }

  }



  if (!focus.locked) {

    return focus;

  }



  const float lockThreshold = 0.08f + prior.movementSmear * 0.05f;

  if (bestWeight < lockThreshold) {

    focus.locked = false;

    focus.kind = PerceptFocusKind::None;

    focus.salience = bestWeight;

  }

  return focus;

}



std::uint8_t focusToConfidence(const LocalFocus& focus, const InteroceptionPrior& prior,

                               std::mt19937& rng) {

  constexpr float kNeutral = static_cast<float>(kNeuronConfidenceNeutral);

  float base = kNeutral;



  if (focus.locked) {

    switch (focus.kind) {

      case PerceptFocusKind::Food:

        base = kNeutral + focus.salience * 3.0f * (0.35f + prior.hunger * 0.65f);

        base -= prior.satiation * 1.2f;

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

  organism.lastPerceptFocusKind = perceptor.focusKind;

  organism.lastPerceptBearing = perceptor.focusBearing;

  organism.lastPerceptRange = perceptor.focusRange;

}



void emitPerceptSignals(Organism& organism, std::uint32_t perceptorId, std::uint8_t confidence,

                        std::uint64_t simTick) {

  emitOutboundConfidence(organism, perceptorId, confidence, simTick);

}



void runPerceptorForNode(Organism& organism, SkeletonNode& perceptor, const BarrenWorld& world,

                         const EnergonField& energon, float cellSize, float halfExtent,

                         const std::vector<Organism>& population, std::uint64_t simTick,

                         float sunIntensity) {

  resetPerceptorNode(perceptor);



  // v1: gaze coupled to body heading; decoupled per-P gaze is reserved for duplicate P.

  perceptor.gazeHeading = organism.heading;



  if (perceptor.store.size() < kPerceptorScanCostPerTick) {

    return;

  }



  std::mt19937 rng =

      chaosSpawnRng(simTick, static_cast<std::uint64_t>(organism.id) ^

                                (static_cast<std::uint64_t>(perceptor.id) << 16) ^ kChaosSaltNom);



  const float effectiveRadius =

      cellSize * organism.senseRadiusFactor * diurnalRadiusScale(sunIntensity);



  std::vector<PerceptCandidate> candidates;

  candidates.reserve(16);

  scanFood(perceptor, perceptor.gazeHeading, effectiveRadius, energon, sunIntensity, rng,

           candidates);

  scanOrganisms(organism, perceptor, perceptor.gazeHeading, effectiveRadius, population,

                sunIntensity, rng, candidates);

  scanBlocks(world, perceptor, perceptor.gazeHeading, effectiveRadius, cellSize, halfExtent,

             sunIntensity, rng, candidates);



  std::uint32_t bytesDue = kPerceptorScanCostPerTick;

  if (!candidates.empty()) {

    bytesDue += kPerceptorTransductionCostPerTick;

  }

  if (perceptor.store.size() < bytesDue) {

    return;

  }



  neuronConsumeBack(perceptor, bytesDue);

  organism.lastPerceptScanPaid = true;

  organism.lastPerceptBytesPaid = bytesDue;



  const InteroceptionPrior prior = gatherInteroception(organism, perceptor.id);

  const LocalFocus focus = integrateFocus(candidates, prior, rng);

  const std::uint8_t confidence = focusToConfidence(focus, prior, rng);



  perceptor.focusKind = focus.kind;

  perceptor.focusBearing = focus.relBearing;

  perceptor.focusRange = focus.range01;

  perceptor.focusSalience = focus.salience;

  perceptor.focusLocked = focus.locked;

  perceptor.perceptConfidence = confidence;

  perceptor.lastEmittedByte = confidence;



  syncOrganismPerceptMirror(organism, perceptor);

  emitPerceptSignals(organism, perceptor.id, confidence, simTick);

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
  applyPmaPerceptorTrustLearning(organism, perceptor.id, trustEvent, simTick);

}

}  // namespace

bool organismHasPmaTopology(const Organism& organism) {

  if (!organism.hasPerceptorNeurons() || !organism.hasMouthNeurons() ||

      !organism.hasActuatorNeurons()) {

    return false;

  }

  if (organism.perceptorCount() != 1 || organism.mouthCount() != 1 ||

      organism.actuatorCount() != 1 || organism.nodes.size() != 3) {

    return false;

  }

  return organism.findNeuralAxon(1, 2) != nullptr && organism.findNeuralAxon(1, 3) != nullptr &&

         organism.findNeuralAxon(2, 3) != nullptr && organism.findNeuralAxon(3, 2) != nullptr &&

         organism.findNeuralAxon(2, 1) != nullptr && organism.findNeuralAxon(3, 1) != nullptr;

}

void runPerceptorPhase(Organism& organism, const BarrenWorld& world, const EnergonField& energon,

                       float cellSize, float halfExtent, const std::vector<Organism>& population,

                       std::uint64_t simTick, float sunIntensity) {

  if (!organism.alive || !organismHasPmaTopology(organism)) {

    return;

  }



  organism.lastPerceptConfidence = 0;

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

