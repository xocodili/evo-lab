#pragma once

#include <cstdint>

namespace evolab {

class BarrenWorld;
class EnergonField;
class Organism;

struct OrganismTickContext {
  const BarrenWorld& world;
  const EnergonField& energon;
  float cellSize = 0.0f;
  float heightScale = 0.0f;
  float halfExtent = 0.0f;
  std::uint64_t simTick = 0;
};

void runOrganismPreAdvectHooks(Organism& organism, const OrganismTickContext& ctx);
void runOrganismAdvect(Organism& organism, const OrganismTickContext& ctx);

}  // namespace evolab
