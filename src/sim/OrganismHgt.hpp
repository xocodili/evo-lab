#pragma once

#include "sim/Organism.hpp"

#include <cstdint>
#include <vector>

namespace evolab {

// Optional overrides for dock pass (production uses defaults; tests may set dockRateOverride).
struct HgtDockPassOptions {
  // When in [0, 1], replaces kAxonDockRate for this pass only.
  float dockRateOverride = -1.0f;
};

void tickAxonTransitBasal(Organism& organism);
void tickHgtDockPass(std::vector<Organism>& population, float cellSize, std::uint64_t simTick,
                     const HgtDockPassOptions& options = {});

int countDanglingAxons(const Organism& organism);
int countForeignInsertions(const Organism& recipient, std::size_t baselineAxonCount);

}  // namespace evolab
