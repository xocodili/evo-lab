#include "sim/NeuronTrust.hpp"

#include "sim/CellConstants.hpp"
#include "sim/Chaos.hpp"
#include "sim/NeuralAxon.hpp"
#include "sim/NeuronSignal.hpp"
#include "sim/OrganismComputer.hpp"
#include "sim/Organism.hpp"
#include "sim/OrganismNeuron.hpp"

#include <cmath>
#include <random>

namespace evolab {

namespace {

constexpr float kTrustOutcomeNeutralBand = 0.05f;
constexpr float kMinLearnValence = 0.12f;
constexpr float kMinLearnDisplacement = 0.0015f;

bool eligibleInboundAxon(const Organism& organism, const NeuralAxon& axon, std::uint32_t dstNodeId,
                         std::uint64_t simTick) {
  if (axon.dstNodeId != dstNodeId || !axon.lastReceived.valid || !axonSignalGateOpen(axon) ||
      !isNeuronConfidenceByte(axon.lastReceived.byte)) {
    return false;
  }
  const SkeletonNode* src = organism.findNode(axon.srcNodeId);
  if (src == nullptr || !src->alive) {
    return false;
  }
  return inboundAxonTickEligible(src->neuron, axon.lastReceived.tick, simTick, true);
}

void applyBelieveTrustFromOutcome(NeuralAxon& axon, std::uint8_t byte, float outcome,
                                  float expected) {
  if (std::abs(outcome) <= kTrustOutcomeNeutralBand) {
    return;
  }
  const std::uint8_t rpe = predictionErrorByte(outcome, expected);
  const int delta = trustDeltaFromPredictionError(rpe);
  if (delta != 0) {
    nudgeBelieveTrustBin(axon, byte, delta);
  }
}

float mouthOutcomeForPerceptorByte(const MouthTrustEvent& event, float valence) {
  const bool approachDominant = valence > kMinLearnValence;
  const bool fleeDominant = valence < -kMinLearnValence;

  if (event.ate) {
    if (approachDominant) {
      return 1.0f;
    }
    if (fleeDominant) {
      return -1.0f;
    }
    return 0.0f;
  }

  if (!event.hadFoodContact) {
    return 0.0f;
  }

  if (event.feedSuppressed) {
    if (fleeDominant) {
      return 1.0f;
    }
    if (approachDominant) {
      return -1.0f;
    }
  }

  return 0.0f;
}

float mouthOutcomeForActuatorByte(const MouthTrustEvent& event, std::uint8_t byte) {
  const float activity = confidenceToUnit(byte);
  if (!event.hadFoodContact) {
    return 0.0f;
  }
  if (event.feedSuppressed && activity > 0.35f) {
    return -0.5f;
  }
  return 0.0f;
}

float actuatorOutcomeForPerceptorByte(const ActuatorInteroception& interoception,
                                      const MotorIntent& intent, float displacement,
                                      float valence) {
  if (!interoception.perceptorLocked) {
    return 0.0f;
  }

  const bool approachDominant = valence > kMinLearnValence;
  const bool fleeDominant = valence < -kMinLearnValence;

  if (intent.strokeBytes > 0 && displacement >= kMinLearnDisplacement) {
    if (approachDominant && !fleeDominant) {
      return 1.0f;
    }
    if (fleeDominant) {
      return -1.0f;
    }
  }

  if (intent.motorSuppressed && approachDominant && interoception.mouthConfidence < 0.55f) {
    return -0.75f;
  }

  return 0.0f;
}

float actuatorOutcomeForMouthByte(const ActuatorInteroception& interoception,
                                  const MotorIntent& intent, float displacement,
                                  std::uint8_t byte) {
  const float satiation = confidenceToUnit(byte);
  const float brake = confidenceToUnit(kMouthInhibitActuatorConfidence);
  const bool mouthBrake = satiation >= brake;

  if (mouthBrake && intent.strokeBytes == 0 && interoception.approach < 0.2f) {
    return 1.0f;
  }

  if (mouthBrake && intent.motorSuppressed && interoception.approach > 0.35f &&
      displacement < kMinLearnDisplacement) {
    return -0.75f;
  }

  return 0.0f;
}

float perceptorOutcomeForMouthByte(const PerceptorTrustEvent& event, std::uint8_t byte) {
  if (!event.scanPaid) {
    return 0.0f;
  }

  const float satiation = confidenceToUnit(byte);
  const float brake = confidenceToUnit(kMouthInhibitActuatorConfidence);
  const bool mouthFull = satiation >= brake;

  if (mouthFull) {
    if (event.focusLocked && event.focusKind == PerceptFocusKind::Food) {
      return -1.0f;
    }
    return 1.0f;
  }

  if (satiation < 0.35f && event.hadFoodCandidate) {
    if (event.focusLocked && event.focusKind == PerceptFocusKind::Food) {
      return 1.0f;
    }
    return -0.5f;
  }

  return 0.0f;
}

float perceptorOutcomeForActuatorByte(const PerceptorTrustEvent& event, std::uint8_t byte) {
  if (!event.scanPaid) {
    return 0.0f;
  }

  const float activity = confidenceToUnit(byte);
  if (activity <= 0.35f) {
    return 0.0f;
  }

  if (!event.focusLocked || event.focusKind != PerceptFocusKind::Food) {
    return 0.5f;
  }

  return -0.5f;
}

float computerOutcomeForSourceByte(const ComputerTrustEvent& event, std::uint8_t byte,
                                   std::uint8_t expected) {
  if (!isNeuronConfidenceByte(byte) || !isNeuronConfidenceByte(expected)) {
    return 0.0f;
  }
  const int score = static_cast<int>(kNeuronConfidenceMax) -
                    std::abs(static_cast<int>(byte) - static_cast<int>(expected));
  const float unit =
      static_cast<float>(score) / static_cast<float>(kNeuronConfidenceMax);
  if (event.matchScore >= 0.65f) {
    return unit * 2.0f - 1.0f;
  }
  if (event.matchScore <= 0.25f) {
    return -(unit * 2.0f - 1.0f) * 0.5f;
  }
  if (event.expelled && unit > 0.5f) {
    return 0.5f;
  }
  return 0.0f;
}

}  // namespace

void applyFeedTrustFromTransfer(NeuralAxon& axon, int bytesMoved, std::uint64_t simTick,
                                std::uint32_t organismId) {
  if (bytesMoved <= 0) {
    return;
  }

  std::mt19937 rng = chaosSpawnRng(simTick, static_cast<std::uint64_t>(organismId) ^ 0xFEEDu);
  const std::uint8_t rpe = predictionErrorByte(1.0f, 0.0f);
  const int delta = trustDeltaFromPredictionError(rpe);
  if (delta != 0) {
    nudgeTrustFeed(axon, delta, rng);
  }
}

void applyCampMouthTrustLearning(Organism& organism, std::uint32_t mouthId,
                                const MouthTrustEvent& event, std::uint64_t simTick) {
  for (NeuralAxon& axon : organism.neuralAxons) {
    if (!eligibleInboundAxon(organism, axon, mouthId, simTick)) {
      continue;
    }

    const SkeletonNode* src = organism.findNode(axon.srcNodeId);
    if (src == nullptr || !src->alive) {
      continue;
    }

    const std::uint8_t byte = axon.lastReceived.byte;
    float outcome = 0.0f;
    if (src->neuron == NeuronType::Perceptor) {
      outcome = mouthOutcomeForPerceptorByte(event, perceptorValenceFromConfidence(byte));
    } else if (src->neuron == NeuronType::Actuator) {
      outcome = mouthOutcomeForActuatorByte(event, byte);
    }

    applyBelieveTrustFromOutcome(axon, byte, outcome, 0.0f);
  }
}

void applyCampActuatorTrustLearning(Organism& organism, std::uint32_t actuatorId,
                                   const ActuatorInteroception& interoception,
                                   const MotorIntent& intent, float displacement,
                                   std::uint64_t simTick) {
  for (NeuralAxon& axon : organism.neuralAxons) {
    if (!eligibleInboundAxon(organism, axon, actuatorId, simTick)) {
      continue;
    }

    const SkeletonNode* src = organism.findNode(axon.srcNodeId);
    if (src == nullptr || !src->alive) {
      continue;
    }

    const std::uint8_t byte = axon.lastReceived.byte;
    float outcome = 0.0f;
    if (src->neuron == NeuronType::Perceptor) {
      outcome = actuatorOutcomeForPerceptorByte(interoception, intent, displacement,
                                                perceptorValenceFromConfidence(byte));
    } else if (src->neuron == NeuronType::Mouth) {
      outcome = actuatorOutcomeForMouthByte(interoception, intent, displacement, byte);
    }

    applyBelieveTrustFromOutcome(axon, byte, outcome, 0.0f);
  }
}

void applyCampPerceptorTrustLearning(Organism& organism, std::uint32_t perceptorId,
                                    const PerceptorTrustEvent& event, std::uint64_t simTick) {
  for (NeuralAxon& axon : organism.neuralAxons) {
    if (!eligibleInboundAxon(organism, axon, perceptorId, simTick)) {
      continue;
    }

    const SkeletonNode* src = organism.findNode(axon.srcNodeId);
    if (src == nullptr || !src->alive) {
      continue;
    }

    const std::uint8_t byte = axon.lastReceived.byte;
    float outcome = 0.0f;
    if (src->neuron == NeuronType::Mouth) {
      outcome = perceptorOutcomeForMouthByte(event, byte);
    } else if (src->neuron == NeuronType::Actuator) {
      outcome = perceptorOutcomeForActuatorByte(event, byte);
    }

    applyBelieveTrustFromOutcome(axon, byte, outcome, 0.0f);
  }
}

void applyCampComputerTrustLearning(Organism& organism, std::uint32_t computerId,
                                    const ComputerInteroception& interoception,
                                    const ComputerTrustEvent& event, std::uint64_t simTick) {
  (void)interoception;
  for (NeuralAxon& axon : organism.neuralAxons) {
    if (!eligibleInboundAxon(organism, axon, computerId, simTick)) {
      continue;
    }

    const SkeletonNode* src = organism.findNode(axon.srcNodeId);
    if (src == nullptr || !src->alive) {
      continue;
    }

    const std::uint8_t byte = axon.lastReceived.byte;
    float outcome = 0.0f;
    switch (src->neuron) {
      case NeuronType::Perceptor:
        outcome = computerOutcomeForSourceByte(event, byte, organism.computerRegister[0]);
        break;
      case NeuronType::Mouth:
        outcome = computerOutcomeForSourceByte(event, byte, organism.computerRegister[1]);
        break;
      case NeuronType::Actuator:
        outcome = computerOutcomeForSourceByte(event, byte, organism.computerRegister[2]);
        break;
      default:
        break;
    }

    applyBelieveTrustFromOutcome(axon, byte, outcome, 0.0f);
  }
}

}  // namespace evolab
