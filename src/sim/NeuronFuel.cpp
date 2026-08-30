#include "sim/NeuronFuel.hpp"

#include "sim/CampTopology.hpp"
#include "sim/CellConstants.hpp"
#include "sim/NeuronStem.hpp"

#include <algorithm>
#include <cmath>

namespace evolab {

SkeletonNode* findComputerHubNode(Organism& organism) {
  if (organism.computerNodeId != 0) {
    return organism.findNode(organism.computerNodeId);
  }
  return findNeuronNode(organism, NeuronType::Computer, false);
}

const SkeletonNode* findComputerHubNode(const Organism& organism) {
  return findComputerHubNode(const_cast<Organism&>(organism));
}

std::size_t totalOrganismFuelBytes(const Organism& organism) {
  std::size_t total = 0;
  for (const SkeletonNode& node : organism.nodes) {
    if (node.alive) {
      total += node.store.size();
    }
  }
  return total;
}

std::size_t computerHubFuelBytes(const Organism& organism) {
  const SkeletonNode* computer = findComputerHubNode(organism);
  return computer != nullptr ? computer->store.size() : 0;
}

std::size_t peripheralStoreCapBytes(const Organism& organism) {
  const auto cap = static_cast<std::size_t>(std::lround(
      static_cast<float>(kNeuronStoreMaxBytes) * organism.peripheralStoreCapFactor));
  return std::max<std::size_t>(kMouthConveyReserveBytes, cap);
}

std::size_t hubStoreCapBytes(const Organism& organism) {
  const auto cap = static_cast<std::size_t>(std::lround(
      static_cast<float>(kComputerHubStoreMaxBytes) * organism.hubStoreCapFactor));
  return std::max<std::size_t>(kComputerHubReserveBytes, cap);
}

std::size_t stemStoreCapBytes(const Organism& organism) {
  (void)organism;
  return kStemCellStorageMaxBytes;
}

std::size_t nodeStoreNominalCap(const Organism& organism, const SkeletonNode& node) {
  if (node.neuron == NeuronType::Computer) {
    return hubStoreCapBytes(organism);
  }
  if (node.neuron == NeuronType::None) {
    return stemStoreCapBytes(organism);
  }
  return peripheralStoreCapBytes(organism);
}

std::size_t nodeStoreReserveBytes(const SkeletonNode& node) {
  if (node.neuron == NeuronType::Computer) {
    return kComputerHubReserveBytes;
  }
  return 0;
}

void initStemNodeStore(SkeletonNode& node, std::size_t storageBytes) {
  const std::size_t bytes =
      std::min(storageBytes, static_cast<std::size_t>(kStemCellStorageMaxBytes));
  node.store.assign(bytes, 0);
}

void initPeripheralNodeStore(SkeletonNode& node, std::size_t storageBytes,
                             const Organism& organism) {
  const std::size_t bytes =
      std::min(storageBytes, peripheralStoreCapBytes(organism));
  node.store.assign(bytes, 0);
}

void initComputerHubStore(SkeletonNode& computer, std::size_t storageBytes,
                          const Organism& organism) {
  const std::size_t bytes = std::min(storageBytes, hubStoreCapBytes(organism));
  computer.store.assign(bytes, 0);
}

void promoteStemStoreToComputerHub(SkeletonNode& computer, SkeletonNode& stemRoot) {
  computer.store = std::move(stemRoot.store);
  stemRoot.store.clear();
}

void neuronStorePush(Organism& organism, SkeletonNode& node, std::uint8_t byte) {
  if (node.store.size() >= nodeStoreNominalCap(organism, node)) {
    return;
  }
  node.store.push_back(byte);
}

void neuronConsumeBack(SkeletonNode& node, std::size_t count) {
  if (count == 0 || node.store.empty()) {
    return;
  }
  const std::size_t removeCount = std::min(node.store.size(), count);
  node.store.erase(node.store.end() - static_cast<std::ptrdiff_t>(removeCount), node.store.end());
}

std::size_t neuronStoreSurplus(const Organism& organism, const SkeletonNode& node) {
  const std::size_t reserve = nodeStoreReserveBytes(node);
  if (node.neuron == NeuronType::Computer) {
    if (node.store.size() <= reserve) {
      return 0;
    }
    return node.store.size() - reserve;
  }
  const std::size_t cap = peripheralStoreCapBytes(organism);
  if (node.store.size() <= cap) {
    return 0;
  }
  return node.store.size() - cap;
}

bool neuronPopBackForConvey(SkeletonNode& node, std::uint8_t& byte) {
  if (node.store.empty()) {
    return false;
  }
  byte = node.store.back();
  node.store.pop_back();
  return true;
}

std::size_t neuronStoreAcceptanceRemaining(const Organism& organism, const SkeletonNode& node) {
  const std::size_t cap = nodeStoreNominalCap(organism, node);
  if (node.store.size() >= cap) {
    return 0;
  }
  return cap - node.store.size();
}

void hubStorePush(Organism& organism, std::uint8_t byte) {
  SkeletonNode* computer = findComputerHubNode(organism);
  if (computer == nullptr) {
    return;
  }
  if (computer->store.size() >= hubStoreCapBytes(organism)) {
    return;
  }
  computer->store.push_back(byte);
}

bool hubStorePopBack(Organism& organism, std::uint8_t& byte) {
  SkeletonNode* computer = findComputerHubNode(organism);
  if (computer == nullptr || computer->store.empty()) {
    return false;
  }
  byte = computer->store.back();
  computer->store.pop_back();
  return true;
}

bool hubStoreConsumeBack(Organism& organism, std::size_t count) {
  SkeletonNode* computer = findComputerHubNode(organism);
  if (computer == nullptr || count == 0 || computer->store.size() < count) {
    return false;
  }
  computer->store.erase(computer->store.end() - static_cast<std::ptrdiff_t>(count),
                        computer->store.end());
  return true;
}

std::size_t hubStoreSurplus(const Organism& organism) {
  const SkeletonNode* computer = findComputerHubNode(organism);
  if (computer == nullptr) {
    return 0;
  }
  return neuronStoreSurplus(organism, *computer);
}

std::size_t hubStoreAcceptanceRemaining(const Organism& organism) {
  const SkeletonNode* computer = findComputerHubNode(organism);
  if (computer == nullptr) {
    return 0;
  }
  return neuronStoreAcceptanceRemaining(organism, *computer);
}

void assignComputerHubFuel(Organism& organism, std::size_t bytes, std::uint8_t fillByte) {
  SkeletonNode* computer = findComputerHubNode(organism);
  if (computer == nullptr) {
    return;
  }
  initComputerHubStore(*computer, bytes, organism);
  if (fillByte != 0) {
    for (std::uint8_t& byte : computer->store) {
      byte = fillByte;
    }
  }
}

}  // namespace evolab
