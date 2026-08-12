#include "sim/BarrenWorld.hpp"
#include "sim/CellPopulation.hpp"
#include "sim/Energon.hpp"
#include "sim/SimConfig.hpp"
#include "sim/WorldConstants.hpp"
#include <chrono>
#include <iostream>
int main() {
  evolab::SimConfig config;
  evolab::BarrenWorld world(config.seed, config.resolution, evolab::makeTideFromConfig(config));
  evolab::EnergonField energon(config.seed, {});
  evolab::CellPopulation cells;
  cells.seedNoms(world, evolab::kWorldCellSize, evolab::kTerrainHeightScale, 60, config.seed);
  auto t0 = std::chrono::steady_clock::now();
  for (evolab::Organism& o : const_cast<std::vector<evolab::Organism>&>(cells.organisms())) {
    o.feed(energon, evolab::kWorldCellSize);
  }
  auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-t0).count();
  std::cout << "feed only ms=" << ms << " blobs=" << energon.blobs().size() << std::endl;
  return 0;
}
