#include "sim/SessionDebugLog.hpp"

#include "sim/BarrenWorld.hpp"
#include "sim/CellPopulation.hpp"
#include "sim/DayCycle.hpp"
#include "sim/Energon.hpp"
#include "sim/EnergonRain.hpp"
#include "sim/SimConfig.hpp"
#include "sim/WorldConstants.hpp"

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>
#include <string>

namespace {

void runLoggedSession(const std::string& logDirectory, int ticks, int debugIntervalMs) {
  evolab::BarrenWorld world(42, 64);
  evolab::DayCycle dayCycle(evolab::kVisualDayCyclePeriodTicks);
  evolab::EnergonConfig energonConfig;
  energonConfig.populationScaledRain = true;
  energonConfig.maxBlobs = 4800;
  evolab::EnergonField energon(42, energonConfig);
  evolab::CellPopulation cells;
  cells.seedNoms(world, evolab::kWorldCellSize, evolab::kTerrainHeightScale, 60, 42);
  cells.installFeedbagReproductionOracle(world, evolab::kWorldCellSize, evolab::kTerrainHeightScale,
                                         world.tickCount());

  evolab::SessionDebugLog log;
  REQUIRE(log.open(logDirectory, debugIntervalMs, 42, "nom"));

  for (int tick = 0; tick < ticks; ++tick) {
    world.tick();
    const float sun = dayCycle.sunIntensity(world.tickCount());
    energon.tick(world, sun, evolab::kWorldCellSize, evolab::kTerrainHeightScale,
                 static_cast<int>(cells.organisms().size()));
    cells.tick(world, energon, evolab::kWorldCellSize, evolab::kTerrainHeightScale, sun);
    log.onSimTick(world.tickCount(), world, energon, cells, sun);
  }

  const evolab::SessionDebugSummary summary = log.finalize();
  REQUIRE(summary.logOpened);
  REQUIRE(summary.maxPopulation >= 60);
}

}  // namespace

TEST_CASE("session debug log records oracle birth", "[debug][session]") {
  const std::string logDirectory =
      (std::filesystem::temp_directory_path() / "evo-lab-debug-test-run").string();
  const std::string logPath = evolab::sessionDebugLogPath(logDirectory);
  std::filesystem::remove(logPath);

  runLoggedSession(logDirectory, 400, 100);

  const evolab::SessionDebugSummary analyzed = evolab::analyzeSessionDebugLog(logPath);
  REQUIRE(analyzed.ok);
  REQUIRE(analyzed.birthsDetected >= 1);
  REQUIRE(analyzed.firstBirthTick > 0);
  REQUIRE(analyzed.firstBirthTick <= 200);
  REQUIRE(analyzed.maxPopulation >= 61);
  REQUIRE(analyzed.firstChildId >= 61);

  std::filesystem::remove(logPath);
}

TEST_CASE("session debug log replaces previous file on open", "[debug][session]") {
  const std::string logDirectory =
      (std::filesystem::temp_directory_path() / "evo-lab-debug-test-replace").string();
  const std::string logPath = evolab::sessionDebugLogPath(logDirectory);
  {
    evolab::SessionDebugLog log;
    REQUIRE(log.open(logDirectory, 1000, 7, "nom"));
    log.finalize();
  }

  {
    evolab::SessionDebugLog log;
    REQUIRE(log.open(logDirectory, 1000, 8, "nom"));
    log.finalize();
  }

  const evolab::SessionDebugSummary analyzed = evolab::analyzeSessionDebugLog(logPath);
  REQUIRE(analyzed.ok);
  REQUIRE(std::filesystem::exists(logPath));

  std::filesystem::remove(logPath);
}
