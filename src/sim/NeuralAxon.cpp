#include "sim/NeuralAxon.hpp"

#include "sim/CellConstants.hpp"
#include "sim/Chaos.hpp"

#include <algorithm>
#include <cmath>

namespace evolab {

float axonTrustScale(std::uint16_t trust) {
  return static_cast<float>(trust) / static_cast<float>(kTrustBaseline);
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
  return axonTrustScale(axon.trustBelieve) * axon.etaSignal >= kNeuralAxonMinGateScale;
}

bool axonMarkedForPruning(const NeuralAxon& axon) {
  return axon.trustBelieve == 0 && axon.trustFeed == 0;
}

void initializeDevelopmentalAxonTrust(NeuralAxon& axon, std::mt19937& rng) {
  axon.trustBelieve = chaosJitterTrust(kTrustBaseline, rng);
  // Feed channel starts gated at developmental minimum (see makeDevelopmentalAxon).
  axon.trustFeed = chaosJitterTrust(kTrustMin, rng);
  axon.etaSignal = chaosJitterFloat(kDefaultNeuralAxonEta, rng);
  axon.etaEnergy = chaosJitterFloat(kDefaultNeuralAxonEta, rng);
}

}  // namespace evolab
