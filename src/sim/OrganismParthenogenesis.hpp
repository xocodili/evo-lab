#pragma once

#include "sim/Organism.hpp"

#include <cstdint>
#include <random>
#include <vector>

namespace evolab {

class BarrenWorld;

struct ParthenogenesisPassOptions {
  float structuralRateOverride = -1.0f;
  bool skipEligibilityChecks = false;
};

struct ParthenogenesisResult {
  bool spawned = false;
  bool aborted = false;
  std::uint32_t bytesSpent = 0;
  std::uint32_t childId = 0;
  Organism child;
};

bool campGenotypeValid(const Organism& organism);

// Spawn gate: legal genotype + developmental axons + three hub muscle arms.
bool campSpawnMorphologyValid(const Organism& organism);
std::uint32_t estimateParthenogenesisCostCamp();
std::uint32_t estimateParthenogenesisRequiredHubBytes();
bool eligibleForParthenogenesis(const Organism& organism, const BarrenWorld& world,
                                float cellSize, std::uint64_t simTick);

ParthenogenesisResult attemptParthenogenesis(Organism& parent, const BarrenWorld& world,
                                             float cellSize, float heightScale,
                                             std::uint64_t simTick, std::uint32_t& nextOrganismId,
                                             const ParthenogenesisPassOptions& options = {});

void tickParthenogenesisPass(std::vector<Organism>& population, const BarrenWorld& world,
                             float cellSize, float heightScale, std::uint64_t simTick,
                             std::uint32_t& nextOrganismId,
                             const ParthenogenesisPassOptions& options = {});

// Test/oracle helper — production path uses unified morphogenesis in attemptParthenogenesis.
Organism cloneCampChildFromParent(const Organism& parent, std::uint32_t childId, float wx,
                                  float wz, float wy, std::uint64_t simTick, std::mt19937& rng,
                                  float structuralRate);

}  // namespace evolab
