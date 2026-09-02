#include "sim/OrganismComputer.hpp"

#include "sim/CampNeuronGating.hpp"
#include "sim/CellConstants.hpp"
#include "sim/CloacaSignal.hpp"
#include "sim/CellConstants.hpp"
#include "sim/NeuralAxon.hpp"
#include "sim/NeuronFuel.hpp"
#include "sim/NeuronSignal.hpp"
#include "sim/NeuronStem.hpp"
#include "sim/NeuronCoordinator.hpp"
#include "sim/NeuronTrust.hpp"
#include "sim/OrganismNeuron.hpp"
#include "sim/PerceptorFocus.hpp"

#include <algorithm>
#include <cmath>

namespace evolab {

namespace {

float computeComputerMatchScore(const ComputerInteroception& interoception,
                                const std::array<std::uint8_t, kComputerRegisterBytes>& reg) {
  const int matchTotal =
      neuronConfidenceMatchScore(interoception.fromPerceptor, reg[0]) +
      neuronConfidenceMatchScore(interoception.fromMouth, reg[1]) +
      neuronConfidenceMatchScore(interoception.fromActuator, reg[2]);
  const int matchMax = static_cast<int>(kNeuronConfidenceMax) * 3;
  return matchMax > 0 ? static_cast<float>(matchTotal) / static_cast<float>(matchMax) : 0.0f;
}

float computeComputerFeedGain(const ComputerInteroception& interoception, float matchScore,
                                float ctaPe) {
  const float hubUnit = interoception.hubSatiationUnit;
  const float matchGo = matchScore;
  const float reserveNoGo = interoception.hubAtReserveFloor ? (1.0f - matchGo) : 0.0f;
  const float repleteNoGo = campHubRepleteNoGo(hubUnit);
  const float ctaNoGo = clamp01(std::abs(ctaPe) * kComputerCtaDisagreementGain);
  const float dispatchDrive = clamp01(matchGo - reserveNoGo - repleteNoGo * 0.35f - ctaNoGo);
  float gain = dispatchDrive;
  if (gain > 1.0e-4f) {
    gain = std::max(gain, kComputerMinDispatchGain * matchGo);
  }
  return std::clamp(gain, 0.0f, 1.0f);
}

void tickOneComputer(Organism& organism, SkeletonNode& computer, EnergonField& field,
                     std::uint64_t simTick, bool allowCloacaExpulsion) {
  const ComputerInteroception interoception =
      gatherComputerInteroception(organism, computer.id, simTick);

  computer.lastComputerMatchScore = computeComputerMatchScore(interoception, computer.computerRegister);
  const float ctaPe = campComputerCtaPredictionError(interoception);
  computer.lastComputerPredictionError = ctaPe;
  const float conservation = interoception.conservationExportScale;
  computer.computerFeedGain =
      computeComputerFeedGain(interoception, computer.lastComputerMatchScore, ctaPe) * conservation;
  computer.computerFeedGain =
      applyMiniCToComputerDispatch(computer.computerFeedGain, computer.coordinatorDutyScale,
                                   conservation);

  if (allowCloacaExpulsion) {
    organism.lastHubSignalExpelledThisTick = false;
    organism.lastCloacaBandExpelled = CloacaBand::None;
    const CloacaBand band = chooseCloacaBandFromInteroception(interoception);
    if (band != CloacaBand::None) {
      const bool expelled = expelCloacaVent(organism, field, computer, band);
      if (expelled) {
        organism.lastHubSignalExpelledThisTick = true;
        organism.lastCloacaBandExpelled = band;
      }
    }
  }

  ComputerTrustEvent trustEvent;
  trustEvent.matchScore = computer.lastComputerMatchScore;
  trustEvent.predictionError = ctaPe;
  trustEvent.expelled = organism.lastHubSignalExpelledThisTick;
  applyCampComputerTrustLearning(organism, computer.id, interoception, trustEvent, simTick,
                                   computer.computerRegister);
}

}  // namespace

CloacaBand chooseCloacaBandFromInteroception(const ComputerInteroception& interoception) {
  if (interoception.distress && interoception.distressVentAffordable) {
    return CloacaBand::Distress;
  }
  if (interoception.mateReady) {
    return CloacaBand::Mate;
  }
  if (interoception.hubSatiationUnit >= confidenceToUnit(kComputerSatiationConfidence) &&
      interoception.baselineVentAffordable) {
    return CloacaBand::Baseline;
  }
  return CloacaBand::None;
}

void seedComputerProprioInteroception(const Organism& organism, std::uint64_t simTick,
                                      ComputerInteroception& prior) {
  prior.hubFuelBytes = static_cast<std::uint32_t>(computerHubFuelBytes(organism));
  const std::size_t hubCap = std::max<std::size_t>(hubStoreCapBytes(organism), 1u);
  prior.hubFuelUnit =
      clamp01(static_cast<float>(prior.hubFuelBytes) / static_cast<float>(hubCap));
  prior.hubSatiationUnit = confidenceToUnit(hubFuelConfidence(prior.hubFuelBytes));
  prior.hubAtReserveFloor = prior.hubFuelBytes <= kComputerHubReserveBytes;
  prior.conservationExportScale = organism.hubConservationExportScale;
  CampBodyInteroception body;
  gatherCampBodyInteroception(organism, simTick, body);
  prior.distress = body.distress;
  prior.mateReady = body.mateReady;
  prior.distressVentAffordable = prior.hubFuelBytes >= kCloacaVentCostDistress;
  prior.baselineVentAffordable =
      prior.hubFuelBytes >= kComputerHubReserveBytes + kCloacaVentCostBaseline;
}

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

  seedComputerProprioInteroception(organism, simTick, prior);

  return prior;
}

float campComputerCtaPredictionError(const ComputerInteroception& interoception) {
  const float expected = perceptorValenceFromConfidence(interoception.fromPerceptor);
  const float outcome = perceptorValenceFromConfidence(interoception.fromMouth);
  return std::clamp(outcome - expected, -1.0f, 1.0f);
}

void initComputerNodeRegister(SkeletonNode& computer) {
  const std::uint8_t proto = computer.coordinatorRegister[0];
  computer.computerRegister = {proto,
                               proto,
                               proto,
                               kNeuronConfidenceNeutral,
                               1u,
                               1u,
                               1u,
                               0u};
  computer.lastComputerMatchScore = 0.0f;
  computer.lastComputerPredictionError = 0.0f;
  computer.computerFeedGain = 1.0f;
}

void guardComputerNodeRegister(SkeletonNode& computer) {
  if (computer.computerRegister[4] == 0) {
    computer.computerRegister[4] = 1;
  }
  if (computer.computerRegister[5] == 0) {
    computer.computerRegister[5] = 1;
  }
  if (computer.computerRegister[6] == 0) {
    computer.computerRegister[6] = 1;
  }
}

void syncOrganismComputerTelemetry(Organism& organism) {
  float maxMatch = 0.0f;
  float maxGain = 0.0f;
  float peFromDominant = 0.0f;
  bool anyComputer = false;

  for (const SkeletonNode& node : organism.nodes) {
    if (!node.alive || node.neuron != NeuronType::Computer) {
      continue;
    }
    anyComputer = true;
    maxMatch = std::max(maxMatch, node.lastComputerMatchScore);
    if (node.computerFeedGain >= maxGain) {
      maxGain = node.computerFeedGain;
      peFromDominant = node.lastComputerPredictionError;
    }
  }

  if (!anyComputer) {
    organism.lastComputerMatchScore = 0.0f;
    organism.lastComputerPredictionError = 0.0f;
    organism.computerFeedGain = 1.0f;
    return;
  }

  organism.lastComputerMatchScore = maxMatch;
  organism.computerFeedGain = maxGain;
  organism.lastComputerPredictionError = peFromDominant;
}

void digestMouthToComputer(Organism& organism) {
  if (!organismUsesCampNeuronPhases(organism)) {
    return;
  }
  if (findNeuronNode(organism, NeuronType::Computer) == nullptr) {
    return;
  }

  for (SkeletonNode& mouth : organism.nodes) {
    if (!mouth.alive || mouth.neuron != NeuronType::Mouth) {
      continue;
    }

    const std::size_t peripheralCap = peripheralStoreCapBytes(organism);
    const std::size_t mouthSurplus =
        mouth.store.size() > peripheralCap ? mouth.store.size() - peripheralCap : 0;
    const std::size_t moveCount = std::min(mouthSurplus, hubStoreAcceptanceRemaining(organism));
    for (std::size_t i = 0; i < moveCount; ++i) {
      std::uint8_t byte = 0;
      if (!neuronPopBackForConvey(mouth, byte)) {
        break;
      }
      hubStorePush(organism, byte);
    }
    reconcileMouthChewFill(mouth);
  }
}

void tickComputerPhase(Organism& organism, EnergonField& field, std::uint64_t simTick) {
  if (!organismUsesCampNeuronPhases(organism)) {
    return;
  }

  refreshStemSurplusExportScales(organism, StemSurplusRefreshPoint::PreComputer);

  bool cloacaPending = true;
  for (SkeletonNode& node : organism.nodes) {
    if (!node.alive || node.neuron != NeuronType::Computer) {
      continue;
    }
    tickOneComputer(organism, node, field, simTick, cloacaPending);
    if (organism.lastHubSignalExpelledThisTick) {
      cloacaPending = false;
    }
  }

  syncOrganismComputerTelemetry(organism);
}

}  // namespace evolab
