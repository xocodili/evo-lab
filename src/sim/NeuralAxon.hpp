#pragma once

#include "sim/CellConstants.hpp"

#include <array>
#include <cstdint>
#include <random>

namespace evolab {

class Organism;
struct SkeletonNode;
enum class NeuronType : std::uint8_t;

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

  // Partial topology (R0 HGT): open end at dead neuron pose; 0 = fully capped.
  std::uint32_t uncappedNodeId = 0;
  float uncappedWorldX = 0.0f;
  float uncappedWorldZ = 0.0f;
  std::uint8_t uncappedNeuronTypeRaw = 0;
  std::uint16_t transitArrearsTicks = 0;

  NeuralAxon() { trustBelieveByConfidence.fill(kTrustBaseline); }
};

bool axonIsDangling(const NeuralAxon& axon);
bool axonEndpointLive(const Organism& organism, const NeuralAxon& axon, bool isSrc);
std::uint32_t axonLiveEndNodeId(const Organism& organism, const NeuralAxon& axon);
void axonUncappedWorldPos(const NeuralAxon& axon, float& wx, float& wz);
void transitionAxonsOnNeuronDeath(Organism& organism, const SkeletonNode& deadNode);

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
bool axonMarkedForTransitPrune(const NeuralAxon& axon);

// Developmental baseline (100%) plus ±3% jitter on axon-side parameters only.
void initializeDevelopmentalAxonTrust(NeuralAxon& axon, std::mt19937& rng);

}  // namespace evolab
