#include "sim/OrganismNeuron.hpp"

#include "engine/kinematics/Math.hpp"
#include "sim/CellConstants.hpp"
#include "sim/NeuronSignal.hpp"

#include <algorithm>
#include <cmath>

namespace evolab {

namespace {

using evolab::engine::kinematics::normalizeAngle;

bool dstNeuronAllowed(NeuronType dst, const NeuronType* allowedDst, std::size_t allowedCount) {
  if (allowedDst == nullptr || allowedCount == 0) {
    return true;
  }
  for (std::size_t i = 0; i < allowedCount; ++i) {
    if (allowedDst[i] == dst) {
      return true;
    }
  }
  return false;
}

}  // namespace

const SkeletonNode* findFirstNeuronNode(const Organism& organism, NeuronType type,
                                        bool requireAlive) {
  for (const SkeletonNode& node : organism.nodes) {
    if (node.neuron != type) {
      continue;
    }
    if (requireAlive && !node.alive) {
      continue;
    }
    return &node;
  }
  return nullptr;
}

AggregatedPerceptSignals aggregatePerceptorInboundSignals(const Organism& organism,
                                                          std::uint32_t dstNodeId,
                                                          std::uint64_t simTick,
                                                          bool requireAlignedTick) {
  AggregatedPerceptSignals signals;
  float bestScore = 0.0f;
  float winnerValence = 0.0f;
  std::uint8_t winnerByte = kNeuronConfidenceNeutral;
  const SkeletonNode* bestSrc = nullptr;

  forEachInboundAxon(organism, dstNodeId, simTick, requireAlignedTick,
                     [&](const InboundAxon& inbound) {
                       if (inbound.src.neuron != NeuronType::Perceptor) {
                         return;
                       }
                       if (!isNeuronConfidenceByte(inbound.axon.lastReceived.byte)) {
                         return;
                       }

                       const float valence =
                           perceptorValenceFromConfidence(inbound.axon.lastReceived.byte);
                       const float score = std::abs(valence) * inbound.weight;
                       if (score > bestScore) {
                         bestScore = score;
                         winnerValence = valence;
                         winnerByte = inbound.axon.lastReceived.byte;
                         bestSrc = &inbound.src;
                       }
                     });

  const int byteDeviation =
      std::abs(static_cast<int>(winnerByte) - static_cast<int>(kNeuronConfidenceNeutral));
  signals.perceptorSalience = clamp01(bestScore);
  signals.perceptorLocked =
      byteDeviation >= 2 && bestScore >= kOrganismCampReflexMinValence;
  if (signals.perceptorLocked) {
    if (winnerValence < 0.0f) {
      signals.focusKind = PerceptFocusKind::Threat;
    } else if (winnerValence > 0.0f) {
      signals.focusKind = PerceptFocusKind::Food;
    }
  }
  if (bestSrc != nullptr) {
    signals.focusBearing = bestSrc->focusBearing;
    signals.gazeHeading = bestSrc->gazeHeading;
  }

  const float gain = perceptorGain(signals.perceptorLocked, signals.perceptorSalience);
  forEachInboundAxon(organism, dstNodeId, simTick, requireAlignedTick,
                     [&](const InboundAxon& inbound) {
                       if (inbound.src.neuron != NeuronType::Perceptor) {
                         return;
                       }
                       if (!isNeuronConfidenceByte(inbound.axon.lastReceived.byte)) {
                         return;
                       }
                       accumulateApproachFlee(signals.approach, signals.flee,
                                              inbound.axon.lastReceived.byte, inbound.weight,
                                              gain);
                     });

  signals.approach = clamp01(signals.approach);
  signals.flee = clamp01(signals.flee);
  return signals;
}

float perceptorGain(bool locked, float salience) {
  if (!locked) {
    return 0.35f;
  }
  return 0.5f + salience * 0.5f;
}

float perceptorValenceFromConfidence(std::uint8_t byte) {
  return (static_cast<float>(byte) - 3.5f) / 3.5f;
}

float inboundAxonTrustWeight(const NeuralAxon& axon, std::uint8_t signalByte) {
  return clamp01(axonTrustScale(axonBelieveTrustForByte(axon, signalByte)) * axon.etaSignal);
}

void accumulateApproachFlee(float& approach, float& flee, std::uint8_t confidenceByte,
                            float weight, float gain) {
  const float valence = perceptorValenceFromConfidence(confidenceByte);
  if (valence > 0.0f) {
    approach = std::max(approach, valence * weight * gain);
  } else {
    flee = std::max(flee, (-valence) * weight * gain);
  }
}

bool inboundAxonTickEligible(NeuronType srcType, std::uint64_t receivedTick, std::uint64_t simTick,
                             bool requireAlignedTick) {
  if (!requireAlignedTick) {
    return true;
  }
  if (receivedTick == simTick) {
    return true;
  }
  // M emits at pre-advect; A at end of advect — both arrive before next perceive/feed.
  if ((srcType == NeuronType::Actuator || srcType == NeuronType::Mouth) && simTick > 0 &&
      receivedTick + 1 == simTick) {
    return true;
  }
  return false;
}

void forEachInboundAxon(const Organism& organism, std::uint32_t dstNodeId, std::uint64_t simTick,
                        bool requireAlignedTick,
                        const std::function<void(const InboundAxon&)>& fn) {
  for (const NeuralAxon& axon : organism.neuralAxons) {
    if (axon.dstNodeId != dstNodeId || !axon.lastReceived.valid || !axonSignalGateOpen(axon)) {
      continue;
    }

    const SkeletonNode* src = organism.findNode(axon.srcNodeId);
    if (src == nullptr || !src->alive) {
      continue;
    }
    if (!inboundAxonTickEligible(src->neuron, axon.lastReceived.tick, simTick,
                                 requireAlignedTick)) {
      continue;
    }

    fn(InboundAxon{axon, *src, inboundAxonTrustWeight(axon, axon.lastReceived.byte)});
  }
}

float turnToward(float current, float target, float maxStep) {
  float delta = normalizeAngle(target - current);
  if (std::abs(delta) <= maxStep) {
    return normalizeAngle(target);
  }
  return normalizeAngle(current + (delta > 0.0f ? maxStep : -maxStep));
}

void emitOutboundConfidence(Organism& organism, std::uint32_t srcNodeId, std::uint8_t confidence,
                            std::uint64_t simTick, const NeuronType* allowedDst,
                            std::size_t allowedCount) {
  if (!isNeuronConfidenceByte(confidence)) {
    return;
  }

  for (NeuralAxon& axon : organism.neuralAxons) {
    if (axon.srcNodeId != srcNodeId) {
      continue;
    }
    const     SkeletonNode* dst = organism.findNode(axon.dstNodeId);
    if (dst == nullptr || !dst->alive || axon.uncappedNodeId == axon.dstNodeId ||
        !dstNeuronAllowed(dst->neuron, allowedDst, allowedCount)) {
      continue;
    }
    if (axon.uncappedNodeId == axon.srcNodeId) {
      continue;
    }
    writeAxonConfidence(axon, confidence, simTick);
  }
}

}  // namespace evolab
