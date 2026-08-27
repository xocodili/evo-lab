#include "sim/OrganismComputer.hpp"

#include "sim/CloacaSignal.hpp"
#include "sim/CellConstants.hpp"
#include "sim/NeuralAxon.hpp"
#include "sim/NeuronFuel.hpp"
#include "sim/NeuronSignal.hpp"
#include "sim/NeuronStem.hpp"
#include "sim/NeuronTrust.hpp"
#include "sim/OrganismNeuron.hpp"

#include <algorithm>
#include <cmath>

namespace evolab {

namespace {

int confidenceMatchScore(std::uint8_t observed, std::uint8_t expected) {
  if (!isNeuronConfidenceByte(observed) || !isNeuronConfidenceByte(expected)) {
    return 0;
  }
  return static_cast<int>(kNeuronConfidenceMax) -
         std::abs(static_cast<int>(observed) - static_cast<int>(expected));
}

}  // namespace

ComputerInteroception gatherComputerInteroception(const Organism& organism,
                                                  std::uint32_t computerId,
                                                  std::uint64_t simTick) {
  ComputerInteroception prior;

  forEachInboundAxon(organism, computerId, simTick, true, [&](const InboundAxon& inbound) {
    if (!isNeuronConfidenceByte(inbound.axon.lastReceived.byte)) {
      return;
    }
    const std::uint8_t byte = inbound.axon.lastReceived.byte;
    switch (inbound.src.neuron) {
      case NeuronType::Perceptor:
        prior.fromPerceptor = byte;
        break;
      case NeuronType::Mouth:
        prior.fromMouth = byte;
        break;
      case NeuronType::Actuator:
        prior.fromActuator = byte;
        break;
      default:
        break;
    }
  });

  return prior;
}

void digestMouthToComputer(Organism& organism) {
  if (!organismHasCampTopology(organism)) {
    return;
  }
  SkeletonNode* mouth = organism.findNode(kCampMouthId);
  if (mouth == nullptr || !mouth->alive) {
    return;
  }

  const std::size_t mouthSurplus =
      mouth->store.size() > kNeuronStoreMaxBytes ? mouth->store.size() - kNeuronStoreMaxBytes : 0;
  const std::size_t moveCount = std::min(mouthSurplus, hubStoreAcceptanceRemaining(organism));
  for (std::size_t i = 0; i < moveCount; ++i) {
    std::uint8_t byte = 0;
    if (!neuronPopBackForConvey(*mouth, byte)) {
      break;
    }
    hubStorePush(organism, byte);
  }
}

void tickComputerPhase(Organism& organism, EnergonField& field, std::uint64_t simTick) {
  if (!organismHasCampTopology(organism)) {
    return;
  }
  SkeletonNode* computer = findNeuronNode(organism, NeuronType::Computer);
  if (computer == nullptr) {
    return;
  }

  const ComputerInteroception interoception =
      gatherComputerInteroception(organism, computer->id, simTick);

  int matchTotal =
      confidenceMatchScore(interoception.fromPerceptor, organism.computerRegister[0]) +
      confidenceMatchScore(interoception.fromMouth, organism.computerRegister[1]) +
      confidenceMatchScore(interoception.fromActuator, organism.computerRegister[2]);
  const int matchMax = static_cast<int>(kNeuronConfidenceMax) * 3;
  organism.lastComputerMatchScore =
      matchMax > 0 ? static_cast<float>(matchTotal) / static_cast<float>(matchMax) : 0.0f;

  organism.lastHubSignalExpelledThisTick = false;
  organism.lastCloacaBandExpelled = CloacaBand::None;
  bool expelled = false;
  const CloacaBand band = chooseCloacaBand(organism, simTick);
  if (band != CloacaBand::None) {
    expelled = expelCloacaVent(organism, field, *computer, band);
    if (expelled) {
      organism.lastHubSignalExpelledThisTick = true;
      organism.lastCloacaBandExpelled = band;
    }
  }

  organism.computerFeedGain =
      std::clamp(organism.lastComputerMatchScore, kComputerMinDispatchGain, 1.0f);

  ComputerTrustEvent trustEvent;
  trustEvent.matchScore = organism.lastComputerMatchScore;
  trustEvent.expelled = expelled;
  applyCampComputerTrustLearning(organism, computer->id, interoception, trustEvent, simTick);
}

}  // namespace evolab
