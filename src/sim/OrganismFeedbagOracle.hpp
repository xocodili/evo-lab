#pragma once

#include "sim/Energon.hpp"
#include "sim/Organism.hpp"

namespace evolab {

class BarrenWorld;

void prepareFeedbagOracleAxons(Organism& organism);
void ensureAbundantFoodAtMouth(EnergonField& field, const SkeletonNode& mouth, float cellSize);
void installFeedbagReproductionOracle(Organism& organism, std::uint64_t simTick);
bool tickFeedbagOracleHooks(Organism& organism, EnergonField& energon, float cellSize);

}  // namespace evolab
