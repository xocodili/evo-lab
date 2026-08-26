#include "sim/NeuronSignal.hpp"

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
      return "fuel/satiation";
    case NeuronType::Actuator:
      return "flagella activity";
    default:
      return "confidence";
  }
}

}  // namespace evolab
