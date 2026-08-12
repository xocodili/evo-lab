#include "sim/BarrenWorld.hpp"
#include "sim/CellPopulation.hpp"
#include "sim/DayCycle.hpp"
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
  cells.seedNoms(world, evolab::kWorldCellSize, evolab::kTerrainHeightScale, 3, config.seed);
  evolab::DayCycle dayCycle(1800.f);
  auto t0 = std::chrono::steady_clock::now();
  for (int i = 0; i < 500; ++i) {
    world.tick();
    energon.tick(world, dayCycle.sunIntensity(world.tickCount()), evolab::kWorldCellSize, evolab::kTerrainHeightScale);
    cells.tick(world, energon, evolab::kWorldCellSize, evolab::kTerrainHeightScale);
  }
  auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-t0).count();
  std::cout << "3 noms 500 ticks ms=" << ms << std::endl;
  return 0;
}
