#include "sim/NeuralAxon.hpp"

#include "sim/Chaos.hpp"
#include "sim/NeuronSignal.hpp"

#include <algorithm>
#include <cmath>

namespace evolab {

float axonTrustScale(std::uint16_t trust) {
  return static_cast<float>(trust) / static_cast<float>(kTrustBaseline);
}

void setAllBelieveTrust(NeuralAxon& axon, std::uint16_t trust) {
  axon.trustBelieveByConfidence.fill(trust);
}

std::uint16_t axonBelieveTrustForByte(const NeuralAxon& axon, std::uint8_t byte) {
  if (isNeuronConfidenceByte(byte)) {
    return axon.trustBelieveByConfidence[byte];
  }
  return axonMaxBelieveTrust(axon);
}

std::uint16_t axonMaxBelieveTrust(const NeuralAxon& axon) {
  return *std::max_element(axon.trustBelieveByConfidence.begin(),
                           axon.trustBelieveByConfidence.end());
}

bool axonBelieveChannelDead(const NeuralAxon& axon) {
  for (std::uint16_t trust : axon.trustBelieveByConfidence) {
    if (trust != 0) {
      return false;
    }
  }
  return true;
}

void nudgeBelieveTrustBin(NeuralAxon& axon, std::uint8_t confidenceByte, int delta) {
  if (!isNeuronConfidenceByte(confidenceByte) || delta == 0) {
    return;
  }
  const int updated =
      static_cast<int>(axon.trustBelieveByConfidence[confidenceByte]) + delta;
  axon.trustBelieveByConfidence[confidenceByte] = static_cast<std::uint16_t>(
      std::clamp(updated, static_cast<int>(kTrustMin), static_cast<int>(kTrustMax)));
}

void nudgeTrustFeed(NeuralAxon& axon, int delta, std::mt19937& rng) {
  if (delta == 0) {
    return;
  }
  const float jitter = chaosJitterMultiplier(rng);
  const int adjusted = static_cast<int>(std::lround(static_cast<float>(delta) * jitter));
  if (adjusted == 0) {
    return;
  }
  const int updated = static_cast<int>(axon.trustFeed) + adjusted;
  axon.trustFeed = static_cast<std::uint16_t>(
      std::clamp(updated, 0, static_cast<int>(kTrustMax)));
}

int axonFeedBandwidth(const NeuralAxon& axon) {
  const float scale = axonTrustScale(axon.trustFeed) * axon.etaEnergy;
  if (scale < kNeuralAxonMinGateScale) {
    return 0;
  }
  const int bandwidth =
      static_cast<int>(std::floor(static_cast<float>(kAxonChannelCapacity) * scale));
  return std::clamp(bandwidth, 0, static_cast<int>(kAxonChannelCapacity));
}

bool axonSignalGateOpen(const NeuralAxon& axon) {
  return axonTrustScale(axonMaxBelieveTrust(axon)) * axon.etaSignal >= kNeuralAxonMinGateScale;
}

bool axonMarkedForPruning(const NeuralAxon& axon) {
  return axonBelieveChannelDead(axon) && axon.trustFeed == 0;
}

void initializeDevelopmentalAxonTrust(NeuralAxon& axon, std::mt19937& rng) {
  for (std::uint16_t& trust : axon.trustBelieveByConfidence) {
    trust = chaosJitterTrust(kTrustBaseline, rng);
  }
  axon.trustFeed = chaosJitterTrust(kTrustMin, rng);
  axon.etaSignal = chaosJitterFloat(kDefaultNeuralAxonEta, rng);
  axon.etaEnergy = chaosJitterFloat(kDefaultNeuralAxonEta, rng);
}

}  // namespace evolab
