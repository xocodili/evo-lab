#pragma once

#include "sim/CellConstants.hpp"

#include <array>
#include <cstdint>
#include <random>

namespace evolab {

struct SignalPacket {
  std::uint8_t byte = 0;
  bool valid = false;
  std::uint64_t tick = 0;
};

// Directed neural edge: byte signal + energon transfer. Trust uses fixed-point (256 = 100%).
// believe trust is indexed by universal confidence byte 0–7 (three-factor / tag-specific plasticity).
struct NeuralAxon {
  std::uint32_t srcNodeId = 0;
  std::uint32_t dstNodeId = 0;
  std::array<std::uint16_t, kNeuronConfidenceBinCount> trustBelieveByConfidence{};
  std::uint16_t trustFeed = kTrustBaseline;
  float etaSignal = 0.88f;
  float etaEnergy = 0.88f;
  SignalPacket pendingSend;
  SignalPacket lastReceived;
  std::uint8_t lastSentByte = 0;

  NeuralAxon() { trustBelieveByConfidence.fill(kTrustBaseline); }
};

float axonTrustScale(std::uint16_t trust);

void setAllBelieveTrust(NeuralAxon& axon, std::uint16_t trust);
std::uint16_t axonBelieveTrustForByte(const NeuralAxon& axon, std::uint8_t byte);
std::uint16_t axonMaxBelieveTrust(const NeuralAxon& axon);
bool axonBelieveChannelDead(const NeuralAxon& axon);
void nudgeBelieveTrustBin(NeuralAxon& axon, std::uint8_t confidenceByte, int delta);
void nudgeTrustFeed(NeuralAxon& axon, int delta, std::mt19937& rng);

int axonFeedBandwidth(const NeuralAxon& axon);
bool axonSignalGateOpen(const NeuralAxon& axon);

bool axonMarkedForPruning(const NeuralAxon& axon);

// Developmental baseline (100%) plus ±3% jitter on axon-side parameters only.
void initializeDevelopmentalAxonTrust(NeuralAxon& axon, std::mt19937& rng);

}  // namespace evolab
