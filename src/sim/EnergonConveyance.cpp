#include "sim/EnergonConveyance.hpp"

#include "sim/CellConstants.hpp"
#include "sim/NeuralAxon.hpp"
#include "sim/NeuronFuel.hpp"
#include "sim/NeuronSignal.hpp"
#include "sim/NeuronTrust.hpp"
#include "sim/Organism.hpp"
#include "sim/OrganismNeuron.hpp"

#include <algorithm>
#include <array>
#include <cmath>

namespace evolab {

namespace {

constexpr int kMaxOutboundRoutes = 8;

struct OutboundRoute {
  NeuralAxon* axon = nullptr;
  SkeletonNode* dst = nullptr;
  float weight = 0.0f;
  int bandwidth = 0;
};

std::uint8_t routeSignalByte(const Organism& organism, const SkeletonNode& src) {
  switch (src.neuron) {
    case NeuronType::Mouth:
      return mouthFuelConfidence(src);
    case NeuronType::Perceptor:
      return src.perceptConfidence;
    case NeuronType::Actuator:
      return organism.lastActuatorOutboundSignal;
    default:
      return kNeuronConfidenceNeutral;
  }
}

float feedRouteWeight(const NeuralAxon& axon, std::uint8_t signalByte) {
  if (!axonSignalGateOpen(axon)) {
    return 0.0f;
  }
  const int bandwidth = axonFeedBandwidth(axon);
  if (bandwidth <= 0) {
    return 0.0f;
  }
  const float believe = inboundAxonTrustWeight(axon, signalByte);
  const float feed = axonTrustScale(axon.trustFeed) * axon.etaEnergy;
  return believe * feed * static_cast<float>(bandwidth);
}

int collectOutboundRoutes(Organism& organism, const SkeletonNode& src, std::uint8_t signalByte,
                          OutboundRoute* routes) {
  int routeCount = 0;
  for (NeuralAxon& axon : organism.neuralAxons) {
    if (axon.srcNodeId != src.id || routeCount >= kMaxOutboundRoutes) {
      continue;
    }
    SkeletonNode* dst = organism.findNode(axon.dstNodeId);
    if (dst == nullptr || !dst->alive) {
      continue;
    }
    const float weight = feedRouteWeight(axon, signalByte);
    if (weight <= 0.0f) {
      continue;
    }
    routes[routeCount++] = OutboundRoute{&axon, dst, weight, axonFeedBandwidth(axon)};
  }
  return routeCount;
}

int applyHopLoss(int bytes, float eta) {
  if (bytes <= 0) {
    return 0;
  }
  return std::max(0, static_cast<int>(std::lround(static_cast<float>(bytes) * eta)));
}

int conveyAlongAxon(Organism& organism, NeuralAxon& axon, SkeletonNode& src, SkeletonNode& dst,
                    int bytesToSend, std::uint64_t simTick) {
  if (bytesToSend <= 0) {
    return 0;
  }

  const bool toMouth = dst.neuron == NeuronType::Mouth;

  std::array<std::uint8_t, kAxonChannelCapacity> payload{};
  int payloadCount = 0;

  for (int i = 0; i < bytesToSend && payloadCount < static_cast<int>(kAxonChannelCapacity); ++i) {
    std::uint8_t byte = 0;
    if (!neuronPopBackForConvey(src, byte)) {
      break;
    }
    payload[static_cast<std::size_t>(payloadCount++)] = byte;
  }

  if (payloadCount <= 0) {
    return 0;
  }

  const int deliveredCount = applyHopLoss(payloadCount, axon.etaEnergy);

  for (int i = deliveredCount; i < payloadCount; ++i) {
    neuronStorePush(src, payload[static_cast<std::size_t>(i)]);
  }

  int deliverable = 0;
  if (toMouth) {
    deliverable = deliveredCount;
  } else {
    deliverable = std::min(deliveredCount, static_cast<int>(neuronStoreAcceptanceRemaining(dst)));
    for (int i = 0; i < deliverable; ++i) {
      neuronStorePush(dst, payload[static_cast<std::size_t>(i)]);
    }
    for (int i = deliverable; i < deliveredCount; ++i) {
      neuronStorePush(src, payload[static_cast<std::size_t>(i)]);
    }
  }

  if (deliverable > 0) {
    applyFeedTrustFromTransfer(axon, deliverable, simTick, organism.id);
  }
  return deliverable;
}

void conveyFromNode(Organism& organism, SkeletonNode& src, std::uint64_t simTick) {
  const std::size_t surplus = neuronStoreSurplus(src);
  if (surplus == 0) {
    return;
  }

  OutboundRoute routes[kMaxOutboundRoutes];
  const int routeCount =
      collectOutboundRoutes(organism, src, routeSignalByte(organism, src), routes);
  if (routeCount <= 0) {
    return;
  }

  float totalWeight = 0.0f;
  for (int i = 0; i < routeCount; ++i) {
    totalWeight += routes[i].weight;
  }
  if (totalWeight <= 0.0f) {
    return;
  }

  int remaining = static_cast<int>(surplus);
  for (int i = 0; i < routeCount && remaining > 0; ++i) {
    OutboundRoute& route = routes[i];
    int share = static_cast<int>(
        std::lround(static_cast<float>(surplus) * (route.weight / totalWeight)));
    if (i + 1 == routeCount) {
      share = remaining;
    }
    share = std::clamp(share, 0, remaining);
    share = std::min(share, route.bandwidth);
    if (share <= 0) {
      continue;
    }
    conveyAlongAxon(organism, *route.axon, src, *route.dst, share, simTick);
    remaining -= share;
  }
}

SkeletonNode* findNeuron(Organism& organism, NeuronType type) {
  for (SkeletonNode& node : organism.nodes) {
    if (node.alive && node.neuron == type) {
      return &node;
    }
  }
  return nullptr;
}

bool organismHasConveySurplus(const Organism& organism) {
  for (const SkeletonNode& node : organism.nodes) {
    if (node.alive && neuronStoreSurplus(node) > 0) {
      return true;
    }
  }
  return false;
}

}  // namespace

void conveyPmaEnergon(Organism& organism, EnergonField& field, std::uint64_t simTick) {
  (void)field;
  if (!organism.isPmaNom() || !organismHasConveySurplus(organism)) {
    return;
  }

  SkeletonNode* conveyNodes[] = {findNeuron(organism, NeuronType::Mouth),
                                 findNeuron(organism, NeuronType::Perceptor),
                                 findNeuron(organism, NeuronType::Actuator)};

  for (int pass = 0; pass < 2; ++pass) {
    for (SkeletonNode* node : conveyNodes) {
      if (node != nullptr) {
        conveyFromNode(organism, *node, simTick);
      }
    }
  }
}

}  // namespace evolab
