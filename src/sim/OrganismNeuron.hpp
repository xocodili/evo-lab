#pragma once

#include "sim/NeuralAxon.hpp"
#include "sim/Organism.hpp"
#include "sim/PerceptorFocus.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <functional>

namespace evolab {

// Shared math and axon helpers for all neuron types (stem metabolism lives in OrganismDetail).

inline float clamp01(float value) { return std::clamp(value, 0.0f, 1.0f); }

const SkeletonNode* findFirstNeuronNode(const Organism& organism, NeuronType type,
                                        bool requireAlive = true);

struct PerceptorMirror {
  bool locked = false;
  float salience = 0.0f;
  PerceptFocusKind focusKind = PerceptFocusKind::None;
  float focusBearing = 0.0f;
  float gazeHeading = 0.0f;
};

PerceptorMirror readPerceptorMirror(const Organism& organism);

float perceptorGain(bool locked, float salience);

// P outbound confidence byte → signed valence in [-1, 1] (4 = neutral).
float perceptorValenceFromConfidence(std::uint8_t byte);

float inboundAxonTrustWeight(const NeuralAxon& axon, std::uint8_t signalByte);

void accumulateApproachFlee(float& approach, float& flee, std::uint8_t confidenceByte,
                            float weight, float gain);

struct InboundAxon {
  const NeuralAxon& axon;
  const SkeletonNode& src;
  float weight;
};

void forEachInboundAxon(const Organism& organism, std::uint32_t dstNodeId, std::uint64_t simTick,
                        bool requireAlignedTick, const std::function<void(const InboundAxon&)>& fn);

// Tick alignment for inbound axons (see DESIGN-NOTES §2.5 tick order):
// P/M same-tick for emit→read within a phase; M and A accept prior tick when read early next tick.
bool inboundAxonTickEligible(NeuronType srcType, std::uint64_t receivedTick, std::uint64_t simTick,
                             bool requireAlignedTick);

float turnToward(float current, float target, float maxStep);

// Emit 0–7 confidence on outbound axons from srcNodeId. When allowedDst is non-null, only
// deliver to destinations whose NeuronType appears in [allowedDst, allowedDst + allowedCount).
void emitOutboundConfidence(Organism& organism, std::uint32_t srcNodeId, std::uint8_t confidence,
                            std::uint64_t simTick, const NeuronType* allowedDst = nullptr,
                            std::size_t allowedCount = 0);

}  // namespace evolab
