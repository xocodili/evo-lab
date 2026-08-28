#include "sim/OrganismHgt.hpp"

#include "sim/CampTopology.hpp"
#include "sim/CellConstants.hpp"
#include "sim/Chaos.hpp"
#include "sim/NeuralAxon.hpp"
#include "sim/NeuronFuel.hpp"
#include "sim/NeuronStem.hpp"

#include <cmath>
#include <random>

namespace evolab {

namespace {

bool tryPayBytesFromNode(Organism& organism, SkeletonNode& node, std::uint32_t cost) {
  if (cost == 0) {
    return true;
  }
  std::vector<std::uint8_t>* pool = neuronFuelPool(organism, node);
  if (pool == nullptr || pool->size() < cost) {
    return false;
  }
  if (pool == &node.store) {
    neuronConsumeBack(node, cost);
  } else {
    consumeFuelBack(*pool, cost);
  }
  return true;
}

SkeletonNode* axonTransitPayer(Organism& organism, const NeuralAxon& axon) {
  if (axonEndpointLive(organism, axon, false)) {
    return organism.findNode(axon.dstNodeId);
  }
  if (axonEndpointLive(organism, axon, true)) {
    return organism.findNode(axon.srcNodeId);
  }
  return nullptr;
}

bool axonShouldPayTransitBasal(const Organism& organism, const NeuralAxon& axon) {
  if (axon.uncappedNodeId == 0) {
    return axonEndpointLive(organism, axon, true) && axonEndpointLive(organism, axon, false);
  }
  return axonLiveEndNodeId(organism, axon) != 0;
}

float distSqXZ(float ax, float az, float bx, float bz) {
  const float dx = ax - bx;
  const float dz = az - bz;
  return dx * dx + dz * dz;
}

NeuronType uncappedNeuronType(const NeuralAxon& axon) {
  return static_cast<NeuronType>(axon.uncappedNeuronTypeRaw);
}

bool compatibleDockTarget(const NeuralAxon& axon, const Organism& recipient,
                          const SkeletonNode& target, std::uint32_t liveEndId) {
  if (!target.alive || target.neuron == NeuronType::None) {
    return false;
  }
  if (target.id == liveEndId) {
    return false;
  }
  if (recipient.neuralAxons.size() >= kAxonChannelCapacity) {
    return false;
  }
  const NeuronType expected = uncappedNeuronType(axon);
  if (expected != NeuronType::None && target.neuron != expected) {
    return false;
  }
  return true;
}

bool tryPayInsertionCost(Organism& organism, SkeletonNode& node) {
  return tryPayBytesFromNode(organism, node, kHgtInsertionCostBytes);
}

void completeInternalDock(Organism& organism, NeuralAxon& axon, std::uint32_t dockNodeId) {
  if (axon.uncappedNodeId == axon.srcNodeId) {
    axon.srcNodeId = dockNodeId;
  } else if (axon.uncappedNodeId == axon.dstNodeId) {
    axon.dstNodeId = dockNodeId;
  }
  axon.uncappedNodeId = 0;
  axon.uncappedNeuronTypeRaw = 0;
  axon.transitArrearsTicks = 0;
}

const SkeletonNode* findNeuronOfType(const Organism& organism, NeuronType type) {
  for (const SkeletonNode& node : organism.nodes) {
    if (node.alive && node.neuron == type) {
      return &node;
    }
  }
  return nullptr;
}

NeuralAxon cloneAxonMotif(const NeuralAxon& stub, std::uint32_t newSrc, std::uint32_t newDst) {
  NeuralAxon edge = stub;
  edge.srcNodeId = newSrc;
  edge.dstNodeId = newDst;
  edge.uncappedNodeId = 0;
  edge.uncappedNeuronTypeRaw = 0;
  edge.transitArrearsTicks = 0;
  edge.pendingSend = {};
  edge.lastReceived = {};
  edge.lastSentByte = 0;
  return edge;
}

bool completeForeignDock(Organism& donor, NeuralAxon& stub, Organism& recipient,
                         std::uint32_t dockNodeId) {
  const std::uint32_t liveEndId = axonLiveEndNodeId(donor, stub);
  const SkeletonNode* liveEnd = donor.findNode(liveEndId);
  SkeletonNode* dockNode = recipient.findNode(dockNodeId);
  if (liveEnd == nullptr || dockNode == nullptr) {
    return false;
  }

  const NeuronType liveType = liveEnd->neuron;
  const SkeletonNode* recipientLive = findNeuronOfType(recipient, liveType);
  if (recipientLive == nullptr) {
    return false;
  }

  const bool uncappedIsSrc = stub.uncappedNodeId == stub.srcNodeId;
  const std::uint32_t newSrc = uncappedIsSrc ? dockNodeId : recipientLive->id;
  const std::uint32_t newDst = uncappedIsSrc ? recipientLive->id : dockNodeId;

  if (recipient.findNeuralAxon(newSrc, newDst) != nullptr) {
    return false;
  }

  if (!tryPayInsertionCost(recipient, *dockNode)) {
    return false;
  }

  recipient.neuralAxons.push_back(cloneAxonMotif(stub, newSrc, newDst));
  stub.transitArrearsTicks = kNeuronBasalGraceTicks;
  setAllBelieveTrust(stub, 0);
  stub.trustFeed = 0;
  return true;
}

float effectiveDockRate(const HgtDockPassOptions& options) {
  if (options.dockRateOverride >= 0.0f && options.dockRateOverride <= 1.0f) {
    return options.dockRateOverride;
  }
  return kAxonDockRate;
}

bool attemptUncappedDock(Organism& owner, NeuralAxon& axon, Organism& recipient,
                          SkeletonNode& dockTarget, std::uint64_t simTick, float dockRate) {
  const std::uint32_t liveEndId = axonLiveEndNodeId(owner, axon);
  if (liveEndId == 0 ||
      !compatibleDockTarget(axon, recipient, dockTarget, liveEndId)) {
    return false;
  }

  std::mt19937 rng = chaosSpawnRng(simTick, static_cast<std::uint64_t>(owner.id) ^
                                                static_cast<std::uint64_t>(dockTarget.id) ^
                                                kChaosSaltHgtDock);
  if (!chaosBernoulli(dockRate, rng)) {
    return false;
  }

  if (&owner == &recipient) {
    const std::uint32_t newSrc =
        axon.uncappedNodeId == axon.srcNodeId ? dockTarget.id : axon.srcNodeId;
    const std::uint32_t newDst =
        axon.uncappedNodeId == axon.dstNodeId ? dockTarget.id : axon.dstNodeId;
    if (owner.findNeuralAxon(newSrc, newDst) != nullptr) {
      return false;
    }
    if (!tryPayInsertionCost(owner, dockTarget)) {
      return false;
    }
    completeInternalDock(owner, axon, dockTarget.id);
    return true;
  }

  return completeForeignDock(owner, axon, recipient, dockTarget.id);
}

}  // namespace

void tickAxonTransitBasal(Organism& organism) {
  if (!organism.alive) {
    return;
  }

  for (NeuralAxon& axon : organism.neuralAxons) {
    if (!axonShouldPayTransitBasal(organism, axon)) {
      continue;
    }

    SkeletonNode* payer = axonTransitPayer(organism, axon);
    if (payer == nullptr) {
      ++axon.transitArrearsTicks;
      continue;
    }

    if (tryPayBytesFromNode(organism, *payer, kAxonTransitBasalCostPerTick)) {
      axon.transitArrearsTicks = 0;
    } else {
      ++axon.transitArrearsTicks;
    }
  }
}

void tickHgtDockPass(std::vector<Organism>& population, float cellSize, std::uint64_t simTick,
                     const HgtDockPassOptions& options) {
  const float dockRadius = cellSize * kAxonDockRadiusFactor;
  const float dockRadiusSq = dockRadius * dockRadius;
  const float dockRate = effectiveDockRate(options);

  for (Organism& owner : population) {
    if (!owner.alive) {
      continue;
    }

    for (NeuralAxon& axon : owner.neuralAxons) {
      if (!axonIsDangling(axon)) {
        continue;
      }

      float uncappedX = 0.0f;
      float uncappedZ = 0.0f;
      axonUncappedWorldPos(axon, uncappedX, uncappedZ);

      for (Organism& recipient : population) {
        if (!recipient.alive) {
          continue;
        }

        for (SkeletonNode& node : recipient.nodes) {
          if (!node.alive || node.neuron == NeuronType::None) {
            continue;
          }
          if (distSqXZ(uncappedX, uncappedZ, node.worldX, node.worldZ) > dockRadiusSq) {
            continue;
          }
          if (attemptUncappedDock(owner, axon, recipient, node, simTick, dockRate)) {
            break;
          }
        }

        if (!axonIsDangling(axon)) {
          break;
        }
      }
    }
  }
}

int countDanglingAxons(const Organism& organism) {
  int count = 0;
  for (const NeuralAxon& axon : organism.neuralAxons) {
    if (axonIsDangling(axon)) {
      ++count;
    }
  }
  return count;
}

int countForeignInsertions(const Organism& recipient, std::size_t baselineAxonCount) {
  if (recipient.neuralAxons.size() <= baselineAxonCount) {
    return 0;
  }
  return static_cast<int>(recipient.neuralAxons.size() - baselineAxonCount);
}

}  // namespace evolab
