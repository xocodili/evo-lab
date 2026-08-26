#pragma once

#include <cstddef>
#include <cstdint>

namespace evolab {

struct SkeletonNode;

void neuronStorePush(SkeletonNode& node, std::uint8_t byte);
void neuronConsumeBack(SkeletonNode& node, std::size_t count);
std::size_t neuronStoreSurplus(const SkeletonNode& node);
bool neuronPopBackForConvey(SkeletonNode& node, std::uint8_t& byte);
std::size_t neuronStoreAcceptanceRemaining(const SkeletonNode& node);

}  // namespace evolab
