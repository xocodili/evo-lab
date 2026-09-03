#pragma once

#include <cstdint>
#include <span>
#include <unordered_map>

namespace evolab::engine::kinematics {

inline constexpr std::size_t kInvalidNodeSpanIndex = static_cast<std::size_t>(-1);

// O(1) id → span index for a fixed node buffer. Build once per tick / constraint pass.
template <typename NodeLike>
class NodeSpanIndex {
public:
  NodeSpanIndex() = default;

  explicit NodeSpanIndex(std::span<const NodeLike> nodes) { rebuild(nodes); }

  template <typename NodeRange>
  void rebuild(const NodeRange& nodes) {
    indexById_.clear();
    indexById_.reserve(nodes.size());
    for (std::size_t i = 0; i < nodes.size(); ++i) {
      indexById_.emplace(nodes[i].id, i);
    }
  }

  std::size_t indexOf(std::uint32_t nodeId) const {
    const auto it = indexById_.find(nodeId);
    if (it == indexById_.end()) {
      return kInvalidNodeSpanIndex;
    }
    return it->second;
  }

  bool contains(std::uint32_t nodeId) const {
    return indexById_.find(nodeId) != indexById_.end();
  }

private:
  std::unordered_map<std::uint32_t, std::size_t> indexById_;
};

template <typename NodeRange>
std::size_t findNodeSpanIndexLinear(const NodeRange& nodes, std::uint32_t nodeId) {
  for (std::size_t i = 0; i < nodes.size(); ++i) {
    if (nodes[i].id == nodeId) {
      return i;
    }
  }
  return kInvalidNodeSpanIndex;
}

}  // namespace evolab::engine::kinematics
