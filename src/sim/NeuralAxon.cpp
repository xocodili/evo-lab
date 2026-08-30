#include "sim/NeuralAxon.hpp"

#include "sim/CampTopology.hpp"
#include "sim/CellConstants.hpp"
#include "sim/Chaos.hpp"
#include "sim/NeuronSignal.hpp"
#include "sim/Organism.hpp"

#include <algorithm>
#include <cmath>

namespace evolab {

float axonTrustScale(std::uint16_t trust) {
  return static_cast<float>(trust) / static_cast<float>(kTrustBaseline);
}

void setAllBelieveTrust(NeuralAxon& axon, std::uint16_t trust) {
  axon.trustBelieveByConfidence.fill(trust);
}

std::uint16_t axonBelieveTrustForByte(const NeuralAxon& axon, std::uint8_t byte) {
  if (isNeuronConfidenceByte(byte)) {
    return axon.trustBelieveByConfidence[byte];
  }
  return axonMaxBelieveTrust(axon);
}

std::uint16_t axonMaxBelieveTrust(const NeuralAxon& axon) {
  return *std::max_element(axon.trustBelieveByConfidence.begin(),
                           axon.trustBelieveByConfidence.end());
}

bool axonBelieveChannelDead(const NeuralAxon& axon) {
  for (std::uint16_t trust : axon.trustBelieveByConfidence) {
    if (trust != 0) {
      return false;
    }
  }
  return true;
}

void nudgeBelieveTrustBin(NeuralAxon& axon, std::uint8_t confidenceByte, int delta) {
  if (!isNeuronConfidenceByte(confidenceByte) || delta == 0) {
    return;
  }
  const int updated =
      static_cast<int>(axon.trustBelieveByConfidence[confidenceByte]) + delta;
  axon.trustBelieveByConfidence[confidenceByte] = static_cast<std::uint16_t>(
      std::clamp(updated, static_cast<int>(kTrustMin), static_cast<int>(kTrustMax)));
}

void nudgeTrustFeed(NeuralAxon& axon, int delta, std::mt19937& rng) {
  if (delta == 0) {
    return;
  }
  const float jitter = chaosJitterMultiplier(rng);
  const int adjusted = static_cast<int>(std::lround(static_cast<float>(delta) * jitter));
  if (adjusted == 0) {
    return;
  }
  const int updated = static_cast<int>(axon.trustFeed) + adjusted;
  axon.trustFeed = static_cast<std::uint16_t>(
      std::clamp(updated, 0, static_cast<int>(kTrustMax)));
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
  return axonTrustScale(axonMaxBelieveTrust(axon)) * axon.etaSignal >= kNeuralAxonMinGateScale;
}

bool axonMarkedForPruning(const NeuralAxon& axon) {
  if (isCampDevelopmentalAxonEdge(axon.srcNodeId, axon.dstNodeId)) {
    return false;
  }
  return (axonBelieveChannelDead(axon) && axon.trustFeed == 0) ||
         axonMarkedForTransitPrune(axon);
}

bool axonMarkedForTransitPrune(const NeuralAxon& axon) {
  return axon.transitArrearsTicks >= kNeuronBasalGraceTicks;
}

bool axonIsDangling(const NeuralAxon& axon) {
  return axon.uncappedNodeId != 0;
}

bool axonEndpointLive(const Organism& organism, const NeuralAxon& axon, bool isSrc) {
  const std::uint32_t nodeId = isSrc ? axon.srcNodeId : axon.dstNodeId;
  if (axon.uncappedNodeId == nodeId) {
    return false;
  }
  const SkeletonNode* node = organism.findNode(nodeId);
  return node != nullptr && node->alive;
}

std::uint32_t axonLiveEndNodeId(const Organism& organism, const NeuralAxon& axon) {
  if (axonEndpointLive(organism, axon, true)) {
    return axon.srcNodeId;
  }
  if (axonEndpointLive(organism, axon, false)) {
    return axon.dstNodeId;
  }
  return 0;
}

void axonUncappedWorldPos(const Organism& organism, const NeuralAxon& axon, float& wx, float& wz) {
  if (axon.uncappedNodeId != 0) {
    const SkeletonNode* node = organism.findNode(axon.uncappedNodeId);
    if (node != nullptr) {
      wx = node->worldX;
      wz = node->worldZ;
      return;
    }
  }
  wx = axon.uncappedWorldX;
  wz = axon.uncappedWorldZ;
}

void transitionAxonsOnNeuronDeath(Organism& organism, const SkeletonNode& deadNode) {
  for (auto it = organism.neuralAxons.begin(); it != organism.neuralAxons.end();) {
    NeuralAxon& axon = *it;
    const bool touchesDead =
        axon.srcNodeId == deadNode.id || axon.dstNodeId == deadNode.id;
    if (!touchesDead) {
      ++it;
      continue;
    }

    const std::uint32_t otherId =
        axon.srcNodeId == deadNode.id ? axon.dstNodeId : axon.srcNodeId;
    const SkeletonNode* other = organism.findNode(otherId);
    if (other == nullptr || !other->alive || axon.uncappedNodeId != 0) {
      it = organism.neuralAxons.erase(it);
      continue;
    }

    axon.uncappedNodeId = deadNode.id;
    axon.uncappedWorldX = deadNode.worldX;
    axon.uncappedWorldZ = deadNode.worldZ;
    axon.uncappedNeuronTypeRaw = static_cast<std::uint8_t>(deadNode.neuron);
    axon.transitArrearsTicks = 0;
    ++it;
  }
}

void initializeDevelopmentalAxonTrust(NeuralAxon& axon, std::mt19937& rng) {
  for (std::uint16_t& trust : axon.trustBelieveByConfidence) {
    trust = chaosJitterTrust(kTrustBaseline, rng);
  }
  axon.trustFeed = chaosJitterTrust(kTrustMin, rng);
  axon.etaSignal = chaosJitterFloat(kDefaultNeuralAxonEta, rng);
  axon.etaEnergy = chaosJitterFloat(kDefaultNeuralAxonEta, rng);
}

}  // namespace evolab
