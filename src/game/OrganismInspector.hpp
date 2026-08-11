#pragma once

#include "sim/Organism.hpp"

#include <cstdint>
#include <string>

namespace evolab::game {

std::string formatOrganismArchitectureLabel(const Organism& organism, std::uint64_t simTick = 0);
std::string formatOrganismHoverSummary(const Organism& organism);

}  // namespace evolab::game
