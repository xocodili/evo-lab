#include "app/SmokeTest.hpp"

#include "sim/BarrenWorld.hpp"
#include "sim/CellConstants.hpp"
#include "sim/CellPopulation.hpp"
#include "sim/DayCycle.hpp"
#include "sim/Energon.hpp"
#include "sim/EnergonRain.hpp"
#include "sim/SessionDebugLog.hpp"
#include "sim/SimConfig.hpp"
#include "sim/WorldConstants.hpp"

#include <algorithm>
#include <iostream>

namespace evolab {

namespace {

void seedPopulation(CellPopulation& cells, const SimConfig& config, const BarrenWorld& world,
                    float cellSize, float heightScale) {
  switch (config.archetype) {
    case SeedArchetype::StemCell:
      cells.seedStemCells(world, cellSize, heightScale, config.nomCount, config.seed);
      break;
    case SeedArchetype::Actuator:
      cells.seedActuatorOrganisms(world, cellSize, heightScale, config.nomCount, config.seed);
      break;
    case SeedArchetype::Nom:
    default:
      cells.seedNoms(world, cellSize, heightScale, config.nomCount, config.seed);
      break;
  }
}

}  // namespace

int runHeadlessSmoke(const CliArgs& args) {
  BarrenWorld world(args.seed, args.resolution);

  const WetnessStats startStats = world.wetnessStats();
  if (startStats.wetCells == 0 || startStats.dryCells == 0) {
    std::cerr << "smoke fail: world lacks both land and water\n";
    return 1;
  }

  const float checksum0 = world.heightChecksum();
  int waterLevelChanges = 0;
  float prevWater = world.waterLevel();

  for (int frame = 0; frame < args.frames; ++frame) {
    world.tick();
    const float water = world.waterLevel();
    if (water != prevWater) {
      ++waterLevelChanges;
      prevWater = water;
    }
  }

  if (waterLevelChanges == 0) {
    std::cerr << "smoke fail: tide did not change water level over " << args.frames << " frames\n";
    return 1;
  }

  const WetnessStats endStats = world.wetnessStats();

  BarrenWorld world2(args.seed, args.resolution);
  if (world2.heightChecksum() != checksum0) {
    std::cerr << "smoke fail: heightmap not deterministic for seed " << args.seed << '\n';
    return 1;
  }

  std::cout << "smoke ok: seed=" << args.seed << " frames=" << args.frames << " resolution="
            << args.resolution << " wet=" << endStats.wetCells << " dry=" << endStats.dryCells
            << " checksum=" << checksum0 << '\n';
  return 0;
}

int runHeadlessDebugSession(const CliArgs& args) {
  const SimConfig config = simConfigFromCli(args);
  BarrenWorld world(config.seed, config.resolution, makeTideFromConfig(config));
  DayCycle dayCycle(kVisualDayCyclePeriodTicks);
  EnergonConfig energonConfig;
  energonConfig.populationScaledRain = true;
  energonConfig.rainPopulationBaseline = config.nomCount;
  energonConfig.maxBlobs = std::max(4000, config.nomCount * 100);
  EnergonField energon(config.seed, energonConfig);
  CellPopulation cells;

  seedPopulation(cells, config, world, kWorldCellSize, kTerrainHeightScale);
  cells.installFeedbagReproductionOracle(world, kWorldCellSize, kTerrainHeightScale,
                                         world.tickCount());

  SessionDebugLog log;
  const std::string logPath = sessionDebugLogPath(".");
  if (!log.open(".", args.debugIntervalMs, config.seed, seedArchetypeLabel(config.archetype))) {
    std::cerr << "debug session fail: could not open " << logPath << '\n';
    return 1;
  }

  std::cout << "debug session: logging to " << logPath << " every " << args.debugIntervalMs
            << "ms (" << log.intervalMs() << ")\n";
  std::cout << "debug session: running " << args.frames << " ticks (~"
            << (static_cast<double>(args.frames) / static_cast<double>(kTicksPerStemCellDay))
            << " fuel-days)\n";
  std::cout.flush();

  const int progressInterval =
      static_cast<int>(std::max<std::uint32_t>(1u, kTicksPerStemCellDay / 4u));  // ~6 visual hours
  int nextProgress = progressInterval;

  for (int frame = 0; frame < args.frames; ++frame) {
    world.tick();
    const float sun = dayCycle.sunIntensity(world.tickCount());
    const int rainPopulation = static_cast<int>(cells.organisms().size());
    energon.tick(world, sun, kWorldCellSize, kTerrainHeightScale, rainPopulation);
    cells.tick(world, energon, kWorldCellSize, kTerrainHeightScale, sun);
    log.onSimTick(world.tickCount(), world, energon, cells, sun);

    if (frame + 1 >= nextProgress) {
      const CellPopulationStats stats = cells.stats();
      const double fuelDays =
          static_cast<double>(world.tickCount()) / static_cast<double>(kTicksPerStemCellDay);
      std::cout << "progress: tick=" << world.tickCount() << " (~" << fuelDays
                << " fuel-days) pop=" << stats.organisms << " camp=" << stats.campNomOrganisms
                << " degraded=" << stats.degradedNomOrganisms
                << " births=" << log.birthsDetectedSoFar() << '\n';
      std::cout.flush();
      nextProgress += progressInterval;
    }
  }

  const SessionDebugSummary summary = log.finalize();
  const SessionDebugSummary analyzed = analyzeSessionDebugLog(logPath);

  std::cout << "debug session summary: ticks=" << args.frames << " max_pop="
            << summary.maxPopulation << " births=" << summary.birthsDetected
            << " first_birth_tick=" << summary.firstBirthTick
            << " first_child_id=" << summary.firstChildId << " oracle_id=" << summary.oracleId
            << '\n';

  if (config.archetype == SeedArchetype::Nom && summary.birthsDetected == 0 &&
      args.frames >= 200) {
    std::cerr << "debug session warning: no parthenogenesis birth detected in " << args.frames
              << " ticks\n";
  }

  if (!analyzed.ok) {
    std::cerr << "debug session fail: log analysis failed for " << logPath << '\n';
    return 1;
  }

  return 0;
}

}  // namespace evolab
