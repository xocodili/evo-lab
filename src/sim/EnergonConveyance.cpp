#include "sim/EnergonConveyance.hpp"

#include "sim/CampTopology.hpp"
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
    case NeuronType::Computer:
      return hubFuelConfidence(organism.bodyStorage.size());
    default:
      return kNeuronConfidenceNeutral;
  }
}

float computerDispatchWeight(const SkeletonNode& computer, NeuronType dstType) {
  std::uint8_t slot = 1;
  switch (dstType) {
    case NeuronType::Perceptor:
      slot = computer.computerRegister[4];
      break;
    case NeuronType::Mouth:
      slot = computer.computerRegister[5];
      break;
    case NeuronType::Actuator:
      slot = computer.computerRegister[6];
      break;
    default:
      return 0.0f;
  }
  if (slot == 0) {
    slot = 1;
  }
  return confidenceToUnit(slot);
}

float feedRouteWeight(const Organism& organism, const NeuralAxon& axon, const SkeletonNode& src,
                      const SkeletonNode& dst, std::uint8_t signalByte) {
  if (!axonSignalGateOpen(axon)) {
    return 0.0f;
  }
  const int bandwidth = axonFeedBandwidth(axon);
  if (bandwidth <= 0) {
    return 0.0f;
  }
  const float believe = inboundAxonTrustWeight(axon, signalByte);
  float feed = axonTrustScale(axon.trustFeed) * axon.etaEnergy;
  if (src.neuron == NeuronType::Computer) {
    feed *= src.computerFeedGain * computerDispatchWeight(src, dst.neuron);
  }
  return believe * feed * static_cast<float>(bandwidth);
}

int collectOutboundRoutes(Organism& organism, const SkeletonNode& src, std::uint8_t signalByte,
                          OutboundRoute* routes) {
  int routeCount = 0;
  for (NeuralAxon& axon : organism.neuralAxons) {
    if (axon.srcNodeId != src.id || routeCount >= kMaxOutboundRoutes ||
        axon.uncappedNodeId == axon.srcNodeId) {
      continue;
    }
    SkeletonNode* dst = organism.findNode(axon.dstNodeId);
    if (dst == nullptr || !dst->alive) {
      continue;
    }
    const float weight = feedRouteWeight(organism, axon, src, *dst, signalByte);
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

bool hubPopBack(Organism& organism, std::uint8_t& byte) {
  return hubStorePopBack(organism, byte);
}

void hubPushBack(Organism& organism, std::uint8_t byte) {
  hubStorePush(organism, byte);
}

int conveyAlongAxonFromHub(Organism& organism, NeuralAxon& axon, SkeletonNode& src,
                             SkeletonNode& dst, int bytesToSend, std::uint64_t simTick) {
  if (bytesToSend <= 0) {
    return 0;
  }

  const bool toMouth = dst.neuron == NeuronType::Mouth;

  std::array<std::uint8_t, kAxonChannelCapacity> payload{};
  int payloadCount = 0;

  for (int i = 0; i < bytesToSend && payloadCount < static_cast<int>(kAxonChannelCapacity); ++i) {
    std::uint8_t byte = 0;
    if (!hubPopBack(organism, byte)) {
      break;
    }
    payload[static_cast<std::size_t>(payloadCount++)] = byte;
  }

  if (payloadCount <= 0) {
    return 0;
  }

  const int deliveredCount = applyHopLoss(payloadCount, axon.etaEnergy);

  for (int i = deliveredCount; i < payloadCount; ++i) {
    hubPushBack(organism, payload[static_cast<std::size_t>(i)]);
  }

  int deliverable = 0;
  if (toMouth) {
    deliverable = std::min(deliveredCount, static_cast<int>(neuronStoreAcceptanceRemaining(dst)));
    for (int i = 0; i < deliverable; ++i) {
      neuronStorePush(dst, payload[static_cast<std::size_t>(i)]);
    }
    for (int i = deliverable; i < deliveredCount; ++i) {
      hubPushBack(organism, payload[static_cast<std::size_t>(i)]);
    }
  } else {
    deliverable = std::min(deliveredCount, static_cast<int>(neuronStoreAcceptanceRemaining(dst)));
    for (int i = 0; i < deliverable; ++i) {
      neuronStorePush(dst, payload[static_cast<std::size_t>(i)]);
    }
    for (int i = deliverable; i < deliveredCount; ++i) {
      hubPushBack(organism, payload[static_cast<std::size_t>(i)]);
    }
  }

  if (deliverable > 0) {
    applyFeedTrustFromTransfer(axon, deliverable, simTick, organism.id);
  }
  (void)src;
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

void conveyFromComputerHub(Organism& organism, SkeletonNode& computer, std::uint64_t simTick) {
  const std::size_t surplus = hubStoreSurplus(organism);
  if (surplus == 0) {
    return;
  }

  OutboundRoute routes[kMaxOutboundRoutes];
  const int routeCount =
      collectOutboundRoutes(organism, computer, routeSignalByte(organism, computer), routes);
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
    conveyAlongAxonFromHub(organism, *route.axon, computer, *route.dst, share, simTick);
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
  if (hubStoreSurplus(organism) > 0) {
    return true;
  }
  for (const SkeletonNode& node : organism.nodes) {
    if (node.alive && neuronStoreSurplus(node) > 0) {
      return true;
    }
  }
  return false;
}

}  // namespace

void conveyCampEnergon(Organism& organism, EnergonField& field, std::uint64_t simTick) {
  (void)field;
  if (!organismUsesCampNeuronPhases(organism) || !organismHasConveySurplus(organism)) {
    return;
  }

  for (int pass = 0; pass < 2; ++pass) {
    for (SkeletonNode& node : organism.nodes) {
      if (!node.alive) {
        continue;
      }
      switch (node.neuron) {
        case NeuronType::Computer:
          conveyFromComputerHub(organism, node, simTick);
          break;
        case NeuronType::Mouth:
        case NeuronType::Perceptor:
        case NeuronType::Actuator:
          conveyFromNode(organism, node, simTick);
          break;
        default:
          break;
      }
    }
  }
}

}  // namespace evolab
