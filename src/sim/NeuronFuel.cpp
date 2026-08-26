#include "sim/NeuronFuel.hpp"

#include "sim/CellConstants.hpp"
#include "sim/Organism.hpp"

#include <algorithm>

namespace evolab {

void neuronStorePush(SkeletonNode& node, std::uint8_t byte) {
  node.store.push_back(byte);
}

void neuronConsumeBack(SkeletonNode& node, std::size_t count) {
  if (count == 0 || node.store.empty()) {
    return;
  }
  const std::size_t removeCount = std::min(node.store.size(), count);
  node.store.erase(node.store.end() - static_cast<std::ptrdiff_t>(removeCount), node.store.end());
}

std::size_t neuronStoreSurplus(const SkeletonNode& node) {
  if (node.store.size() <= kNeuronStoreMaxBytes) {
    return 0;
  }
  return node.store.size() - kNeuronStoreMaxBytes;
}

bool neuronPopBackForConvey(SkeletonNode& node, std::uint8_t& byte) {
  if (node.store.empty()) {
    return false;
  }
  byte = node.store.back();
  node.store.pop_back();
  return true;
}

std::size_t neuronStoreAcceptanceRemaining(const SkeletonNode& node) {
  if (node.store.size() >= kNeuronStoreMaxBytes) {
    return 0;
  }
  return kNeuronStoreMaxBytes - node.store.size();
}

}  // namespace evolab
