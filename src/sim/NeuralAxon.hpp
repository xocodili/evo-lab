#pragma once

#include <cstdint>

#include <random>

namespace evolab {

struct SignalPacket {
  std::uint8_t byte = 0;
  bool valid = false;
  std::uint64_t tick = 0;
};

// Directed neural edge: byte signal + energon transfer. Trust uses fixed-point (256 = 100%).
struct NeuralAxon {
  std::uint32_t srcNodeId = 0;
  std::uint32_t dstNodeId = 0;
  std::uint16_t trustBelieve = 256;
  std::uint16_t trustFeed = 256;
  float etaSignal = 0.88f;
  float etaEnergy = 0.88f;
  SignalPacket pendingSend;
  SignalPacket lastReceived;
  std::uint8_t lastSentByte = 0;
};

float axonTrustScale(std::uint16_t trust);

int axonFeedBandwidth(const NeuralAxon& axon);
bool axonSignalGateOpen(const NeuralAxon& axon);

bool axonMarkedForPruning(const NeuralAxon& axon);

// Developmental baseline (100%) plus ±3% jitter on axon-side parameters only.
void initializeDevelopmentalAxonTrust(NeuralAxon& axon, std::mt19937& rng);

}  // namespace evolab
