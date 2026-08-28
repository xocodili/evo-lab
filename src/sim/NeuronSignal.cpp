#include "sim/NeuronSignal.hpp"

#include "sim/CloacaSignal.hpp"
#include "sim/Organism.hpp"

#include <algorithm>
#include <cmath>

namespace evolab {

std::uint8_t fuelStoreToConfidence(std::size_t storeBytes, std::uint32_t fullBytes) {
  if (fullBytes == 0) {
    return 0;
  }
  const float fill =
      std::clamp(static_cast<float>(storeBytes) / static_cast<float>(fullBytes), 0.0f, 1.0f);
  return static_cast<std::uint8_t>(
      std::lround(fill * static_cast<float>(kNeuronConfidenceMax)));
}

std::uint8_t mouthFuelConfidence(const SkeletonNode& mouth) {
  const float recent =
      static_cast<float>(std::min(mouth.store.size(), static_cast<std::size_t>(kMouthLocalStoreMaxBytes))) /
      static_cast<float>(kMouthLocalStoreMaxBytes);
  const float reserve = static_cast<float>(
      std::min(mouth.store.size(), static_cast<std::size_t>(kNeuronConfidenceFullFuelBytes))) /
                        static_cast<float>(kNeuronConfidenceFullFuelBytes);
  const float fill = std::clamp(0.55f * recent + 0.45f * reserve, 0.0f, 1.0f);
  return static_cast<std::uint8_t>(std::lround(fill * static_cast<float>(kNeuronConfidenceMax)));
}

namespace {

enum class MouthDietCategory : std::uint8_t {
  Sunfall = 0,
  Fragment,
  CloacaDistress,
  CloacaBaseline,
  CloacaMate,
  Other
};

MouthDietCategory mouthDietCategoryForBite(EnergonOrigin origin, CloacaBand cloacaBand) {
  if (origin == EnergonOrigin::Cloaca) {
    switch (cloacaBand) {
      case CloacaBand::Distress:
        return MouthDietCategory::CloacaDistress;
      case CloacaBand::Baseline:
        return MouthDietCategory::CloacaBaseline;
      case CloacaBand::Mate:
        return MouthDietCategory::CloacaMate;
      default:
        return MouthDietCategory::Other;
    }
  }
  if (origin == EnergonOrigin::Sunfall) {
    return MouthDietCategory::Sunfall;
  }
  if (origin == EnergonOrigin::Fragment) {
    return MouthDietCategory::Fragment;
  }
  return MouthDietCategory::Other;
}

float& mouthDietEmaForCategory(SkeletonNode& mouth, MouthDietCategory category) {
  switch (category) {
    case MouthDietCategory::Sunfall:
      return mouth.mouthDietSunfallEma;
    case MouthDietCategory::Fragment:
      return mouth.mouthDietFragmentEma;
    case MouthDietCategory::CloacaDistress:
      return mouth.mouthDietCloacaDistressEma;
    case MouthDietCategory::CloacaBaseline:
      return mouth.mouthDietCloacaBaselineEma;
    case MouthDietCategory::CloacaMate:
      return mouth.mouthDietCloacaMateEma;
    default:
      return mouth.mouthDietFragmentEma;
  }
}

float mouthDietShare(const SkeletonNode& mouth, float SkeletonNode::*field) {
  const float total = mouth.mouthDietSunfallEma + mouth.mouthDietFragmentEma +
                      mouth.mouthDietCloacaDistressEma + mouth.mouthDietCloacaBaselineEma +
                      mouth.mouthDietCloacaMateEma;
  if (total <= 1.0e-6f) {
    return 0.0f;
  }
  return std::clamp(mouth.*field / total, 0.0f, 1.0f);
}

}  // namespace

void recordMouthDietBite(SkeletonNode& mouth, EnergonOrigin origin, CloacaBand cloacaBand) {
  const MouthDietCategory category = mouthDietCategoryForBite(origin, cloacaBand);
  float& ema = mouthDietEmaForCategory(mouth, category);
  ema = (1.0f - kMouthDietEmaAlpha) * ema + kMouthDietEmaAlpha;
}

std::uint8_t mouthOutboundConfidence(const SkeletonNode& mouth) {
  const std::uint8_t fuel = mouthFuelConfidence(mouth);
  const float distressShare = mouthDietShare(mouth, &SkeletonNode::mouthDietCloacaDistressEma);
  const float sunfallShare = mouthDietShare(mouth, &SkeletonNode::mouthDietSunfallEma);
  const float mateShare = mouthDietShare(mouth, &SkeletonNode::mouthDietCloacaMateEma);
  const float hasDiet = mouth.mouthDietSunfallEma + mouth.mouthDietFragmentEma +
                        mouth.mouthDietCloacaDistressEma + mouth.mouthDietCloacaBaselineEma +
                        mouth.mouthDietCloacaMateEma;

  if (hasDiet <= 1.0e-6f) {
    return fuel;
  }

  // Conditioned taste aversion analogue: distress-heavy diet triggers gag reflex (low outbound).
  if (distressShare >= kMouthDietGagDistressThreshold) {
    return static_cast<std::uint8_t>(std::min(fuel, static_cast<std::uint8_t>(2)));
  }

  float unit = confidenceToUnit(fuel);

  if (sunfallShare >= kMouthDietPalatableSunfallThreshold) {
    unit = std::min(1.0f, unit + 0.12f);
  } else if (mateShare > 0.5f && sunfallShare < 0.2f) {
    // Mate-band cloaca is a signal, not palatable food — dampen satiation broadcast.
    unit = std::max(0.0f, unit - 0.18f);
  } else {
    unit = std::clamp(unit * (1.0f - distressShare * 0.75f), 0.0f, 1.0f);
  }

  return static_cast<std::uint8_t>(
      std::lround(std::clamp(unit, 0.0f, 1.0f) * static_cast<float>(kNeuronConfidenceMax)));
}

std::uint8_t hubFuelConfidence(std::size_t hubBytes, std::uint32_t fullBytes) {
  return fuelStoreToConfidence(hubBytes, fullBytes);
}

std::uint8_t encodeNeuronOutboundConfidence(const Organism& organism, NeuronType type,
                                            const SkeletonNode& node) {
  switch (type) {
    case NeuronType::Mouth:
      return mouthOutboundConfidence(node);
    case NeuronType::Perceptor:
      return node.perceptConfidence;
    case NeuronType::Actuator:
      return actuatorActivityConfidence(organism.lastStrokePaid, organism.lastStrokeBytesPaid);
    case NeuronType::Computer:
      return hubFuelConfidence(organism.bodyStorage.size());
    default:
      return kNeuronConfidenceNeutral;
  }
}

std::uint8_t actuatorActivityConfidence(bool strokePaid, std::uint32_t strokeBytesPaid) {
  if (!strokePaid || strokeBytesPaid == 0) {
    return 0;
  }
  const float activity = std::clamp(static_cast<float>(strokeBytesPaid) /
                                        static_cast<float>(kActuatorStrokeCostPerTick),
                                    0.0f, 1.0f);
  return static_cast<std::uint8_t>(std::lround(activity * static_cast<float>(kNeuronConfidenceMax)));
}

std::uint8_t predictionErrorByte(float outcome, float expected) {
  const float pe = std::clamp(outcome - expected, -1.0f, 1.0f);
  const int rpe = static_cast<int>(std::lround(
      static_cast<float>(kNeuronConfidenceNeutral) +
      static_cast<float>(kNeuronConfidenceNeutral) * pe));
  return static_cast<std::uint8_t>(
      std::clamp(rpe, 0, static_cast<int>(kNeuronConfidenceMax)));
}

int trustDeltaFromPredictionError(std::uint8_t rpeByte) {
  const int centered =
      static_cast<int>(rpeByte) - static_cast<int>(kNeuronConfidenceNeutral);
  if (centered == 0) {
    return 0;
  }
  const int rawMagnitude =
      std::max(1, (std::abs(centered) * static_cast<int>(kTrustLearnStep)) / 3);
  const int magnitude = std::min(static_cast<int>(kTrustLearnStep), rawMagnitude);
  return (centered > 0 ? 1 : -1) * magnitude;
}

void writeAxonConfidence(NeuralAxon& axon, std::uint8_t confidence, std::uint64_t simTick) {
  if (!isNeuronConfidenceByte(confidence)) {
    return;
  }
  axon.lastSentByte = confidence;
  axon.lastReceived.valid = true;
  axon.lastReceived.byte = confidence;
  axon.lastReceived.tick = simTick;
}

const char* neuronConfidenceRoleLabel(NeuronType source) {
  switch (source) {
    case NeuronType::Perceptor:
      return "approach/avoid";
    case NeuronType::Mouth:
      return "fuel/diet satiation";
    case NeuronType::Actuator:
      return "flagella activity";
    case NeuronType::Computer:
      return "hub/satiation";
    default:
      return "confidence";
  }
}

}  // namespace evolab
