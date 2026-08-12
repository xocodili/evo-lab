#include "sim/BarrenWorld.hpp"
#include "sim/CellPopulation.hpp"
#include "sim/DayCycle.hpp"
#include "sim/Energon.hpp"
#include "sim/SimConfig.hpp"
#include "sim/WorldConstants.hpp"
#include <chrono>
#include <iostream>
int main() {
  using clock = std::chrono::steady_clock;
  evolab::SimConfig config;
  evolab::BarrenWorld world(config.seed, config.resolution, evolab::makeTideFromConfig(config));
  evolab::EnergonConfig ec; ec.spawnRateMax = 14.f; ec.maxBlobs = 2200;
  evolab::EnergonField energon(config.seed, ec);
  evolab::CellPopulation cells;
  cells.seedNoms(world, evolab::kWorldCellSize, evolab::kTerrainHeightScale, 60, config.seed);
  evolab::DayCycle dayCycle(1800.f);
  auto t0 = clock::now();
  for (int i = 0; i < 5000; ++i) {
    world.tick();
    energon.tick(world, dayCycle.sunIntensity(world.tickCount()), evolab::kWorldCellSize, evolab::kTerrainHeightScale);
    cells.tick(world, energon, evolab::kWorldCellSize, evolab::kTerrainHeightScale);
  }
  auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(clock::now()-t0).count();
  std::cout << "5000 ticks ms=" << ms << " blobs=" << energon.blobs().size() << " orgs=" << cells.organisms().size() << std::endl;
  return 0;
}
