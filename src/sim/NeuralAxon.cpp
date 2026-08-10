#include "sim/NeuralAxon.hpp"

#include "sim/CellConstants.hpp"
#include "sim/Chaos.hpp"

namespace evolab {

float axonTrustScale(std::uint16_t trust) {
  return static_cast<float>(trust) / static_cast<float>(kTrustBaseline);
}

bool axonMarkedForPruning(const NeuralAxon& axon) {
  return axon.trustBelieve == 0 && axon.trustFeed == 0;
}

void initializeDevelopmentalAxonTrust(NeuralAxon& axon, std::mt19937& rng) {
  axon.trustBelieve = chaosJitterTrust(kTrustBaseline, rng);
  axon.trustFeed = chaosJitterTrust(kTrustBaseline, rng);
  axon.etaSignal = chaosJitterFloat(kDefaultNeuralAxonEta, rng);
  axon.etaEnergy = chaosJitterFloat(kDefaultNeuralAxonEta, rng);
}

}  // namespace evolab
