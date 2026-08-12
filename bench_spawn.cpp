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
  evolab::EnergonConfig ec; ec.spawnRateMax = 14.f; ec.maxBlobs = 2200;
  evolab::EnergonField energon(config.seed, ec);
  evolab::CellPopulation cells;
  cells.seedNoms(world, evolab::kWorldCellSize, evolab::kTerrainHeightScale, 60, config.seed);
  for (int i = 0; i < 100; ++i) { world.tick(); energon.tick(world, 1.f, evolab::kWorldCellSize, evolab::kTerrainHeightScale); }
  std::cout << "blobs=" << energon.blobs().size() << std::endl;
  auto t0 = std::chrono::steady_clock::now();
  cells.tick(world, energon, evolab::kWorldCellSize, evolab::kTerrainHeightScale);
  auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-t0).count();
  std::cout << "1 tick after 100 spawn ticks ms=" << ms << std::endl;
  return 0;
}
