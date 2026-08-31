#include "sim/BarrenWorld.hpp"
#include "sim/CellPopulation.hpp"
#include "sim/DayCycle.hpp"
#include "sim/Energon.hpp"
#include "sim/EnergonRain.hpp"
#include "sim/EnergonStats.hpp"
#include "sim/SimConfig.hpp"
#include "sim/WorldConstants.hpp"
#include <iostream>
#include <algorithm>
int main() {
  evolab::SimConfig config;
  evolab::BarrenWorld world(config.seed, config.resolution, evolab::makeTideFromConfig(config));
  evolab::DayCycle dayCycle(evolab::kVisualDayCyclePeriodTicks);
  evolab::EnergonConfig ec;
  ec.populationScaledRain = true;
  ec.rainPopulationBaseline = config.nomCount;
  ec.maxBlobs = 6000;
  ec.spawnRateMax = 10.0f;
  evolab::EnergonField energon(config.seed, ec);
  evolab::CellPopulation cells;
  cells.seedNoms(world, evolab::kWorldCellSize, evolab::kTerrainHeightScale, config.nomCount, config.seed);
  cells.installFeedbagReproductionOracle(world, evolab::kWorldCellSize, evolab::kTerrainHeightScale, world.tickCount());

  float maxFullnessDay = 0, maxBytes = 0;
  int throttleDay = 0, daylight = 0, nightMinBlobs = 999999, dayMaxBlobs = 0;
  int blobsAtMinBytes = 0, minBytes = 999999;
  int dawnSpawns = 0;
  float prevSun = 0;
  for (int tick = 0; tick < 6000; ++tick) {
    world.tick();
    float sun = dayCycle.sunIntensity(world.tickCount());
    energon.tick(world, sun, evolab::kWorldCellSize, evolab::kTerrainHeightScale, cells.liveCampNomCount());
    cells.tick(world, energon, evolab::kWorldCellSize, evolab::kTerrainHeightScale, sun);
    auto st = evolab::computeEnergonStats(energon);
    auto& ss = energon.lastSunfallTickStats();
    maxBytes = std::max(maxBytes, st.wetEdibleBytes);
    if (st.wetEdibleBytes < minBytes) { minBytes = st.wetEdibleBytes; blobsAtMinBytes = st.blobCount; }
    if (sun > 0) {
      ++daylight;
      maxFullnessDay = std::max(maxFullnessDay, ss.fieldFullness);
      if (ss.spawnProbability < 0.99f) ++throttleDay;
      dayMaxBlobs = std::max(dayMaxBlobs, st.blobCount);
    } else {
      nightMinBlobs = std::min(nightMinBlobs, st.blobCount);
    }
    if (prevSun <= 0.01f && sun > 0.01f) {
      dawnSpawns = ss.spawnedBlobs;
      std::cout << "DAWN tick=" << tick+1 << " blobs=" << st.blobCount << " bytes=" << st.wetEdibleBytes
                << " fullness=" << ss.fieldFullness << " spawnProb=" << ss.spawnProbability
                << " spawned=" << ss.spawnedBlobs << " nominal=" << ss.nominalExpected << " sun=" << sun << "\n";
    }
    prevSun = sun;
  }
  std::cout << "SUMMARY maxBytes=" << maxBytes << " maxFullnessDay=" << maxFullnessDay
            << " throttleDayTicks=" << throttleDay << "/" << daylight
            << " dayMaxBlobs=" << dayMaxBlobs << " nightMinBlobs=" << nightMinBlobs
            << " minBytes=" << minBytes << " blobsAtMinBytes=" << blobsAtMinBytes << "\n";
  return 0;
}
