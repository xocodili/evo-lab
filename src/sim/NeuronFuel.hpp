#pragma once

#include "sim/Organism.hpp"

#include <cstddef>
#include <cstdint>

namespace evolab {

struct SkeletonNode;

void neuronStorePush(SkeletonNode& node, std::uint8_t byte);
void neuronConsumeBack(SkeletonNode& node, std::size_t count);
std::size_t neuronStoreSurplus(const SkeletonNode& node);
bool neuronPopBackForConvey(SkeletonNode& node, std::uint8_t& byte);
std::size_t neuronStoreAcceptanceRemaining(const SkeletonNode& node);

// Computer hub (bodyStorage on CAMP noms).
void hubStorePush(Organism& organism, std::uint8_t byte);
bool hubStorePopBack(Organism& organism, std::uint8_t& byte);
std::size_t hubStoreSurplus(const Organism& organism);
std::size_t hubStoreAcceptanceRemaining(const Organism& organism);

}  // namespace evolab
