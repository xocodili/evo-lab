#include "sim/BarrenWorld.hpp"
#include "sim/CampTopology.hpp"
#include "sim/CellPopulation.hpp"
#include "sim/DayCycle.hpp"
#include "sim/Energon.hpp"
#include "sim/EnergonRain.hpp"
#include "sim/EnergonStats.hpp"
#include "sim/NeuronFuel.hpp"
#include "sim/NeuronStem.hpp"
#include "sim/SimConfig.hpp"
#include "sim/WorldConstants.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>
#include <limits>
#include <memory>
#include <sstream>
#include <unordered_map>
#include <vector>

namespace {

constexpr int kTankMarathonTicks = 6000;
constexpr int kTankUltraMarathonTicks = kTankMarathonTicks * 3;
constexpr int kTankMarathonSampleInterval = 1000;
constexpr int kTankUltraMarathonSampleInterval = 2000;

evolab::EnergonConfig makeVisualTankEnergonConfig(int nomCount) {
  evolab::EnergonConfig config;
  config.populationScaledRain = true;
  config.rainPopulationBaseline = nomCount;
  config.spawnRateMax = 10.0f;
  config.maxBlobs = std::max(4000, nomCount * 100);
  return config;
}

int countAliveCampNoms(const evolab::CellPopulation& cells) {
  return cells.liveCampNomCount();
}

int countSunfallBlobs(const evolab::EnergonField& field) {
  int count = 0;
  for (const evolab::EnergonBlob& blob : field.blobs()) {
    if (blob.origin == evolab::EnergonOrigin::Sunfall) {
      ++count;
    }
  }
  return count;
}

struct TankSample {
  int tick = 0;
  float sun = 0.0f;
  int alive = 0;
  int blobs = 0;
  int wet = 0;
  int dry = 0;
  int falling = 0;
  int sunfallBlobs = 0;
  int wetEdibleBytes = 0;
  float fullness = 0.0f;
  float spawnProbability = 0.0f;
  float nominalExpected = 0.0f;
  float adjustedExpected = 0.0f;
  int spawnedThisTick = 0;
  std::uint64_t cumulativeSpawns = 0;
  float avgFamineUnit = 0.0f;
  float avgCoordinatorDuty = 0.0f;
  std::uint64_t popCumBites = 0;
  std::size_t avgHubBytes = 0;
  std::size_t minHubBytes = 0;
  std::size_t maxHubBytes = 0;
  int eatHubDropTicks = 0;
};

struct SeedSpawnRecord {
  std::uint32_t id = 0;
  float spawnX = 0.0f;
  float spawnZ = 0.0f;
  bool landAdjacent = false;
};

struct SpawnLuckSummary {
  int seedCohort = 0;
  int seedSurvivors = 0;
  int totalAlive = 0;
  int offspringAlive = 0;
  int feedbagAlive = 0;
  float survivorMeanX = 0.0f;
  float survivorMeanZ = 0.0f;
  float deadMeanX = 0.0f;
  float deadMeanZ = 0.0f;
  float survivorMeanRadius = 0.0f;
  float deadMeanRadius = 0.0f;
};

struct TankRunResult {
  int tickCount = 0;
  int aliveStart = 0;
  int aliveEnd = 0;
  int aliveMin = 0;
  int maxDry = 0;
  int maxWet = 0;
  int minWetEdibleBytes = 0;
  int maxWetEdibleBytes = 0;
  float avgSpawnProb = 0.0f;
  float avgFamineUnit = 0.0f;
  float avgCoordinatorDuty = 0.0f;
  float lowFoodAvgFamine = 0.0f;
  float lowFoodAvgDuty = 0.0f;
  float highFoodAvgFamine = 0.0f;
  float highFoodAvgDuty = 0.0f;
  float spawnsPerDaylightTick = 0.0f;
  std::uint64_t cumulativeSpawns = 0;
  int lowFoodSamples = 0;
  int highFoodSamples = 0;
  int daylightTicks = 0;
  std::vector<TankSample> samples;
  SpawnLuckSummary spawnLuck;
  int totalEatHubDropTicks = 0;
};

float averageLiveCamperFamine(const evolab::CellPopulation& cells) {
  float sum = 0.0f;
  int count = 0;
  for (const evolab::Organism& organism : cells.organisms()) {
    if (!organism.alive || !organism.isCampNom() || organism.feedbagOracle) {
      continue;
    }
    sum += organism.famineUnit;
    ++count;
  }
  return count > 0 ? sum / static_cast<float>(count) : 0.0f;
}

float averageLiveCamperCoordinatorDuty(const evolab::CellPopulation& cells) {
  float sum = 0.0f;
  int count = 0;
  for (const evolab::Organism& organism : cells.organisms()) {
    if (!organism.alive || !organism.isCampNom() || organism.feedbagOracle) {
      continue;
    }
    sum += organism.coordinatorMinNodeDuty;
    ++count;
  }
  return count > 0 ? sum / static_cast<float>(count) : 1.0f;
}

struct CamperFuelLedger {
  bool initialized = false;
  std::size_t prevHub = 0;
  std::size_t prevMouth = 0;
  std::uint64_t cumBites = 0;
  std::size_t hubPeak = 0;
  std::size_t hubMin = std::numeric_limits<std::size_t>::max();
  std::uint64_t cumHubVentTicks = 0;
  std::uint64_t eatHubDropTicks = 0;
};

const char* camperCohortLabel(const evolab::Organism& organism) {
  if (organism.feedbagOracle) {
    return "feedbag";
  }
  if (organism.createdAtTick != 0) {
    return "offspring";
  }
  return "seed";
}

const evolab::SkeletonNode* liveMouthNode(const evolab::Organism& organism) {
  return evolab::findNeuronNode(organism, evolab::NeuronType::Mouth, true);
}

const evolab::SkeletonNode* liveCampNode(const evolab::Organism& organism, evolab::NeuronType type) {
  return evolab::findNeuronNode(organism, type, true);
}

float liveNodeEquilibriumScale(const evolab::Organism& organism, evolab::NeuronType type) {
  const evolab::SkeletonNode* node = liveCampNode(organism, type);
  return node != nullptr ? node->equilibriumExportScale : 0.0f;
}

void writeFuelLedgerHeader(std::ostream& out) {
  out << "# camper fuel ledger: bites consumed vs C hub storage per tick\n"
      << "# cohort: seed | offspring | feedbag\n"
      << "# eatHubDrop=1 when ate this tick but hub bytes fell (same-tick vent/dispatch/basal > digest)\n"
      << "# mouthDelta>0 on ate ticks usually means bite credited to M wallet before digest moves surplus to C\n"
      << "# eqP/eqM/eqA/eqC = Black Queen export scale per neuron (0=retain, 1=full surplus share)\n"
      << "tick\tsun\tid\tcohort\tate\tcumBites\thub\thubDelta\tmouth\tmouthDelta\tchew\ttotal\t"
         "famine\thubVent\tfeedSupp\teatHubDrop\teqP\teqM\teqA\teqC\n";
}

void logCamperFuelTick(std::ostream& out, int tick, float sun, const evolab::Organism& organism,
                       CamperFuelLedger& ledger) {
  if (!organism.alive || !organism.isCampNom()) {
    return;
  }

  const evolab::SkeletonNode* mouth = liveMouthNode(organism);
  const bool ate = mouth != nullptr && mouth->ateThisTick;
  if (ate) {
    ++ledger.cumBites;
  }

  const std::size_t hub = organism.computerHubFuelBytes();
  const std::size_t mouthBytes = mouth != nullptr ? mouth->store.size() : 0;
  int hubDelta = 0;
  int mouthDelta = 0;
  if (!ledger.initialized) {
    ledger.initialized = true;
    ledger.prevHub = hub;
    ledger.prevMouth = mouthBytes;
  } else {
    hubDelta = static_cast<int>(hub) - static_cast<int>(ledger.prevHub);
    mouthDelta = static_cast<int>(mouthBytes) - static_cast<int>(ledger.prevMouth);
  }
  ledger.hubPeak = std::max(ledger.hubPeak, hub);
  ledger.hubMin = std::min(ledger.hubMin, hub);
  if (organism.lastHubSignalExpelledThisTick) {
    ++ledger.cumHubVentTicks;
  }

  const bool eatHubDrop = ate && hubDelta < 0;
  if (eatHubDrop) {
    ++ledger.eatHubDropTicks;
  }

  const std::uint32_t chewFill = mouth != nullptr ? mouth->mouthChewFill : 0;

  out << tick << '\t' << sun << '\t' << organism.id << '\t' << camperCohortLabel(organism) << '\t'
      << (ate ? 1 : 0) << '\t' << ledger.cumBites << '\t' << hub << '\t' << hubDelta << '\t'
      << mouthBytes << '\t' << mouthDelta << '\t' << chewFill << '\t' << organism.totalFuelBytes()
      << '\t' << organism.famineUnit << '\t' << (organism.lastHubSignalExpelledThisTick ? 1 : 0)
      << '\t' << (organism.lastMouthFeedSuppressed ? 1 : 0) << '\t' << (eatHubDrop ? 1 : 0)
      << '\t' << liveNodeEquilibriumScale(organism, evolab::NeuronType::Perceptor) << '\t'
      << liveNodeEquilibriumScale(organism, evolab::NeuronType::Mouth) << '\t'
      << liveNodeEquilibriumScale(organism, evolab::NeuronType::Actuator) << '\t'
      << organism.hubConservationExportScale << '\n';

  ledger.prevHub = hub;
  ledger.prevMouth = mouthBytes;
}

struct PopFuelSnapshot {
  std::uint64_t popCumBites = 0;
  std::size_t hubSum = 0;
  std::size_t hubMin = std::numeric_limits<std::size_t>::max();
  std::size_t hubMax = 0;
  int eatHubDropTicks = 0;
  int alive = 0;
};

PopFuelSnapshot snapshotPopulationFuel(
    const evolab::CellPopulation& cells,
    std::unordered_map<std::uint32_t, CamperFuelLedger>& ledgers) {
  PopFuelSnapshot snap;
  for (const evolab::Organism& organism : cells.organisms()) {
    if (!organism.alive || !organism.isCampNom() || organism.feedbagOracle) {
      continue;
    }
    ++snap.alive;
    CamperFuelLedger& ledger = ledgers[organism.id];
    snap.popCumBites += ledger.cumBites;
    const std::size_t hub = organism.computerHubFuelBytes();
    snap.hubSum += hub;
    snap.hubMin = std::min(snap.hubMin, hub);
    snap.hubMax = std::max(snap.hubMax, hub);
    snap.eatHubDropTicks += static_cast<int>(ledger.eatHubDropTicks);
  }
  if (snap.hubMin == std::numeric_limits<std::size_t>::max()) {
    snap.hubMin = 0;
  }
  return snap;
}

std::vector<SeedSpawnRecord> captureSeedCohort(const evolab::CellPopulation& cells) {
  std::vector<SeedSpawnRecord> records;
  for (const evolab::Organism& organism : cells.organisms()) {
    if (organism.feedbagOracle || !organism.isCampNom() || organism.createdAtTick != 0) {
      continue;
    }
    SeedSpawnRecord record;
    record.id = organism.id;
    record.spawnX = organism.rootWorldX();
    record.spawnZ = organism.rootWorldZ();
    record.landAdjacent = organism.landAdjacent;
    records.push_back(record);
  }
  return records;
}

SpawnLuckSummary analyzeSeedLuck(const evolab::CellPopulation& cells,
                                 const std::vector<SeedSpawnRecord>& seeds) {
  SpawnLuckSummary summary;
  summary.seedCohort = static_cast<int>(seeds.size());
  std::unordered_map<std::uint32_t, bool> aliveById;
  for (const evolab::Organism& organism : cells.organisms()) {
    if (!organism.alive || !organism.isCampNom() || organism.feedbagOracle) {
      continue;
    }
    aliveById[organism.id] = true;
    if (organism.createdAtTick != 0) {
      ++summary.offspringAlive;
    }
  }
  summary.totalAlive = static_cast<int>(aliveById.size());

  float survivorX = 0.0f;
  float survivorZ = 0.0f;
  float survivorRadius = 0.0f;
  float deadX = 0.0f;
  float deadZ = 0.0f;
  float deadRadius = 0.0f;
  int deadCount = 0;

  for (const SeedSpawnRecord& seed : seeds) {
    const bool alive = aliveById.find(seed.id) != aliveById.end();
    const float radius = std::sqrt(seed.spawnX * seed.spawnX + seed.spawnZ * seed.spawnZ);
    if (alive) {
      ++summary.seedSurvivors;
      survivorX += seed.spawnX;
      survivorZ += seed.spawnZ;
      survivorRadius += radius;
    } else {
      ++deadCount;
      deadX += seed.spawnX;
      deadZ += seed.spawnZ;
      deadRadius += radius;
    }
  }

  if (summary.seedSurvivors > 0) {
    summary.survivorMeanX = survivorX / static_cast<float>(summary.seedSurvivors);
    summary.survivorMeanZ = survivorZ / static_cast<float>(summary.seedSurvivors);
    summary.survivorMeanRadius =
        survivorRadius / static_cast<float>(summary.seedSurvivors);
  }
  if (deadCount > 0) {
    summary.deadMeanX = deadX / static_cast<float>(deadCount);
    summary.deadMeanZ = deadZ / static_cast<float>(deadCount);
    summary.deadMeanRadius = deadRadius / static_cast<float>(deadCount);
  }
  return summary;
}

evolab::CellPopulationStats runVisualSessionStats(int tickCount) {
  evolab::SimConfig config;
  evolab::BarrenWorld world(config.seed, config.resolution, evolab::makeTideFromConfig(config));
  evolab::DayCycle dayCycle(evolab::kVisualDayCyclePeriodTicks);
  const evolab::EnergonConfig energonConfig = makeVisualTankEnergonConfig(config.nomCount);
  evolab::EnergonField energon(config.seed, energonConfig);
  evolab::CellPopulation cells;
  cells.seedNoms(world, evolab::kWorldCellSize, evolab::kTerrainHeightScale, config.nomCount,
                 config.seed);
  cells.installFeedbagReproductionOracle(world, evolab::kWorldCellSize, evolab::kTerrainHeightScale,
                                       world.tickCount());

  for (int tick = 0; tick < tickCount; ++tick) {
    world.tick();
    const float sun = dayCycle.sunIntensity(world.tickCount());
    energon.tick(world, sun, evolab::kWorldCellSize, evolab::kTerrainHeightScale,
                 cells.liveCampNomCount());
    cells.tick(world, energon, evolab::kWorldCellSize, evolab::kTerrainHeightScale, sun);
  }
  return cells.stats();
}

TankRunResult runTankSimulation(int tickCount, int sampleInterval,
                                const char* fuelLedgerPath = nullptr) {
  evolab::SimConfig config;
  evolab::BarrenWorld world(config.seed, config.resolution, evolab::makeTideFromConfig(config));
  evolab::DayCycle dayCycle(evolab::kVisualDayCyclePeriodTicks);
  const evolab::EnergonConfig energonConfig = makeVisualTankEnergonConfig(config.nomCount);
  evolab::EnergonField energon(config.seed, energonConfig);
  evolab::CellPopulation cells;
  cells.seedNoms(world, evolab::kWorldCellSize, evolab::kTerrainHeightScale, config.nomCount,
                 config.seed);
  cells.installFeedbagReproductionOracle(world, evolab::kWorldCellSize, evolab::kTerrainHeightScale,
                                       world.tickCount());

  const std::vector<SeedSpawnRecord> seedCohort = captureSeedCohort(cells);
  TankRunResult result;
  result.tickCount = tickCount;
  result.aliveStart = countAliveCampNoms(cells);
  result.aliveMin = result.aliveStart;
  result.minWetEdibleBytes = std::numeric_limits<int>::max();

  constexpr int kLowFoodBytes = 500;
  constexpr int kHighFoodBytes = 2500;
  float spawnProbSum = 0.0f;
  float famineSum = 0.0f;
  float dutySum = 0.0f;

  std::unordered_map<std::uint32_t, CamperFuelLedger> fuelLedgers;
  std::unique_ptr<std::ofstream> fuelLog;
  if (fuelLedgerPath != nullptr && fuelLedgerPath[0] != '\0') {
    fuelLog = std::make_unique<std::ofstream>(fuelLedgerPath, std::ios::out | std::ios::trunc);
    if (*fuelLog) {
      writeFuelLedgerHeader(*fuelLog);
      std::cout << "Fuel ledger: " << fuelLedgerPath << std::endl;
    } else {
      std::cerr << "Failed to open fuel ledger: " << fuelLedgerPath << std::endl;
      fuelLog.reset();
    }
  }

  int totalEatHubDropTicks = 0;

  for (int tick = 0; tick < tickCount; ++tick) {
    world.tick();
    const float sun = dayCycle.sunIntensity(world.tickCount());
    if (sun > 0.0f) {
      ++result.daylightTicks;
    }
    const int rainPop = cells.liveCampNomCount();
    energon.tick(world, sun, evolab::kWorldCellSize, evolab::kTerrainHeightScale, rainPop);
    cells.tick(world, energon, evolab::kWorldCellSize, evolab::kTerrainHeightScale, sun);

    if (fuelLog) {
      for (const evolab::Organism& organism : cells.organisms()) {
        logCamperFuelTick(*fuelLog, tick + 1, sun, organism, fuelLedgers[organism.id]);
      }
      fuelLog->flush();
    }

    const evolab::EnergonStats stats = evolab::computeEnergonStats(energon);
    const evolab::EnergonSunfallTickStats& sunStats = energon.lastSunfallTickStats();
    const int wetEdibleBytes = stats.wetEdibleBytes;
    const int alive = countAliveCampNoms(cells);
    const float avgFamine = averageLiveCamperFamine(cells);
    const float avgDuty = averageLiveCamperCoordinatorDuty(cells);

    result.aliveMin = std::min(result.aliveMin, alive);
    result.maxDry = std::max(result.maxDry, stats.groundedDry);
    result.maxWet = std::max(result.maxWet, stats.groundedWet);
    result.minWetEdibleBytes = std::min(result.minWetEdibleBytes, wetEdibleBytes);
    result.maxWetEdibleBytes = std::max(result.maxWetEdibleBytes, wetEdibleBytes);
    if (sun > 0.0f) {
      spawnProbSum += sunStats.spawnProbability;
    }
    famineSum += avgFamine;
    dutySum += avgDuty;
    if (wetEdibleBytes <= kLowFoodBytes) {
      ++result.lowFoodSamples;
      result.lowFoodAvgFamine += avgFamine;
      result.lowFoodAvgDuty += avgDuty;
    } else if (wetEdibleBytes >= kHighFoodBytes) {
      ++result.highFoodSamples;
      result.highFoodAvgFamine += avgFamine;
      result.highFoodAvgDuty += avgDuty;
    }

    if (tick == 0 || tick + 1 == tickCount || (tick + 1) % sampleInterval == 0) {
      const PopFuelSnapshot fuelSnap = snapshotPopulationFuel(cells, fuelLedgers);
      totalEatHubDropTicks = fuelSnap.eatHubDropTicks;

      TankSample sample;
      sample.tick = tick + 1;
      sample.sun = sun;
      sample.alive = alive;
      sample.blobs = stats.blobCount;
      sample.wet = stats.groundedWet;
      sample.dry = stats.groundedDry;
      sample.falling = stats.falling;
      sample.sunfallBlobs = countSunfallBlobs(energon);
      sample.wetEdibleBytes = wetEdibleBytes;
      sample.fullness = sunStats.fieldFullness;
      sample.spawnProbability = sunStats.spawnProbability;
      sample.nominalExpected = sunStats.nominalExpected;
      sample.adjustedExpected = sunStats.adjustedExpected;
      sample.spawnedThisTick = sunStats.spawnedBlobs;
      sample.cumulativeSpawns = energon.cumulativeSunfallSpawns();
      sample.avgFamineUnit = avgFamine;
      sample.avgCoordinatorDuty = avgDuty;
      sample.popCumBites = fuelSnap.popCumBites;
      sample.avgHubBytes =
          fuelSnap.alive > 0 ? fuelSnap.hubSum / static_cast<std::size_t>(fuelSnap.alive) : 0;
      sample.minHubBytes = fuelSnap.hubMin;
      sample.maxHubBytes = fuelSnap.hubMax;
      sample.eatHubDropTicks = fuelSnap.eatHubDropTicks;
      result.samples.push_back(sample);
    }
  }

  if (fuelLog) {
    *fuelLog << "# --- end-of-run camper summary ---\n"
             << "# id\tcohort\tcumBites\thubPeak\thubMin\teatHubDropTicks\thubVentTicks\n";
    for (const evolab::Organism& organism : cells.organisms()) {
      if (!organism.isCampNom()) {
        continue;
      }
      const auto it = fuelLedgers.find(organism.id);
      if (it == fuelLedgers.end()) {
        continue;
      }
      const CamperFuelLedger& ledger = it->second;
      const std::size_t hubMin =
          ledger.hubMin == std::numeric_limits<std::size_t>::max() ? 0 : ledger.hubMin;
      *fuelLog << organism.id << '\t' << camperCohortLabel(organism) << '\t' << ledger.cumBites
               << '\t' << ledger.hubPeak << '\t' << hubMin << '\t' << ledger.eatHubDropTicks
               << '\t' << ledger.cumHubVentTicks << '\t' << (organism.alive ? "alive" : "dead")
               << '\n';
    }
    fuelLog->flush();
    std::cout << "Fuel ledger eatHubDropTicks(cumulative)=" << totalEatHubDropTicks << std::endl;
  }

  result.totalEatHubDropTicks = totalEatHubDropTicks;
  result.aliveEnd = countAliveCampNoms(cells);
  result.cumulativeSpawns = energon.cumulativeSunfallSpawns();
  result.avgSpawnProb = result.daylightTicks > 0
                            ? spawnProbSum / static_cast<float>(result.daylightTicks)
                            : 0.0f;
  result.avgFamineUnit =
      tickCount > 0 ? famineSum / static_cast<float>(tickCount) : 0.0f;
  result.avgCoordinatorDuty =
      tickCount > 0 ? dutySum / static_cast<float>(tickCount) : 1.0f;
  result.lowFoodAvgFamine = result.lowFoodSamples > 0
                                ? result.lowFoodAvgFamine /
                                      static_cast<float>(result.lowFoodSamples)
                                : 0.0f;
  result.lowFoodAvgDuty = result.lowFoodSamples > 0
                              ? result.lowFoodAvgDuty / static_cast<float>(result.lowFoodSamples)
                              : 1.0f;
  result.highFoodAvgFamine = result.highFoodSamples > 0
                                 ? result.highFoodAvgFamine /
                                       static_cast<float>(result.highFoodSamples)
                                 : 0.0f;
  result.highFoodAvgDuty = result.highFoodSamples > 0
                               ? result.highFoodAvgDuty /
                                     static_cast<float>(result.highFoodSamples)
                               : 1.0f;
  result.spawnsPerDaylightTick = result.daylightTicks > 0
                                     ? static_cast<float>(result.cumulativeSpawns) /
                                           static_cast<float>(result.daylightTicks)
                                     : 0.0f;
  result.spawnLuck = analyzeSeedLuck(cells, seedCohort);
  int feedbagAlive = 0;
  int offspringAlive = 0;
  for (const evolab::Organism& organism : cells.organisms()) {
    if (!organism.alive || !organism.isCampNom()) {
      continue;
    }
    if (organism.feedbagOracle) {
      ++feedbagAlive;
    } else if (organism.createdAtTick != 0) {
      ++offspringAlive;
    }
  }
  result.spawnLuck.offspringAlive = offspringAlive;
  result.spawnLuck.feedbagAlive = feedbagAlive;
  return result;
}

void logTankRun(const char* label, const TankRunResult& result) {
  for (const TankSample& sample : result.samples) {
    INFO(label << " tick=" << sample.tick << " sun=" << sample.sun << " alive=" << sample.alive
               << " blobs=" << sample.blobs << " wetEdibleBytes=" << sample.wetEdibleBytes
               << " fullness=" << sample.fullness << " spawnProb=" << sample.spawnProbability
               << " avgFamine=" << sample.avgFamineUnit << " avgDuty=" << sample.avgCoordinatorDuty
               << " popCumBites=" << sample.popCumBites << " hubAvg=" << sample.avgHubBytes
               << " hubRange=" << sample.minHubBytes << ".." << sample.maxHubBytes
               << " eatHubDropTicks=" << sample.eatHubDropTicks);
  }

  const SpawnLuckSummary& luck = result.spawnLuck;
  INFO(label << " ticks=" << result.tickCount << " aliveStart=" << result.aliveStart
             << " aliveEnd=" << result.aliveEnd << " aliveMin=" << result.aliveMin
             << " cumulativeSpawns=" << result.cumulativeSpawns
             << " spawnsPerDaylightTick=" << result.spawnsPerDaylightTick
             << " wetEdibleBytesRange=" << result.minWetEdibleBytes << ".."
             << result.maxWetEdibleBytes << " avgFamine=" << result.avgFamineUnit
             << " seedSurvivors=" << luck.seedSurvivors << "/" << luck.seedCohort
             << " offspringAlive=" << luck.offspringAlive
             << " feedbagAlive=" << luck.feedbagAlive << " totalAlive=" << luck.totalAlive
             << " survivorSpawnMean=(" << luck.survivorMeanX << "," << luck.survivorMeanZ << ")"
             << " deadSpawnMean=(" << luck.deadMeanX << "," << luck.deadMeanZ << ")"
             << " survivorRadius=" << luck.survivorMeanRadius
             << " deadRadius=" << luck.deadMeanRadius);

  std::cout << label << " alive " << result.aliveStart << "->" << result.aliveEnd << " (min "
            << result.aliveMin << ") spawns=" << result.cumulativeSpawns
            << " spawns/daylightTick=" << result.spawnsPerDaylightTick << " wetEdibleBytes="
            << result.minWetEdibleBytes << ".." << result.maxWetEdibleBytes
            << " avgFamine=" << result.avgFamineUnit << " avgDuty=" << result.avgCoordinatorDuty
            << " seedSurvivors=" << luck.seedSurvivors << "/" << luck.seedCohort
            << " offspringAlive=" << luck.offspringAlive
            << " feedbagAlive=" << luck.feedbagAlive << " survivorSpawnMean=("
            << luck.survivorMeanX << "," << luck.survivorMeanZ << ") deadSpawnMean=("
            << luck.deadMeanX << "," << luck.deadMeanZ << ") survivorRadius="
            << luck.survivorMeanRadius << " deadRadius=" << luck.deadMeanRadius << std::endl;
}

void requireTankGateInvariants(const TankRunResult& result) {
  REQUIRE(result.aliveStart > 0);
  REQUIRE(result.cumulativeSpawns > 0);
  REQUIRE(result.samples.size() >= 6);
  REQUIRE(result.lowFoodSamples > 0);
  REQUIRE(result.highFoodSamples > 0);
  REQUIRE(result.lowFoodAvgFamine > result.highFoodAvgFamine);
  REQUIRE(result.lowFoodAvgDuty < result.highFoodAvgDuty);
}

struct CycleDiagnostic {
  int tick = 0;
  float sun = 0.0f;
  int blobs = 0;
  int wetEdibleBytes = 0;
  int falling = 0;
  float fullness = 0.0f;
  float spawnProbability = 0.0f;
  float nominalExpected = 0.0f;
  int spawned = 0;
};

struct CycleDiagnosticResult {
  int throttleDaylight = 0;
  int daylight = 0;
  int nightMinBlobs = 0;
  int dayMaxBlobs = 0;
  int nightFamineTicks = 0;
  float maxFullnessDay = 0.0f;
};

CycleDiagnosticResult runCycleDiagnostic(int startTick, int cycleTicks) {
  evolab::SimConfig config;
  evolab::BarrenWorld world(config.seed, config.resolution, evolab::makeTideFromConfig(config));
  evolab::DayCycle dayCycle(evolab::kVisualDayCyclePeriodTicks);
  evolab::EnergonConfig ec = makeVisualTankEnergonConfig(config.nomCount);
  evolab::EnergonField energon(config.seed, ec);
  evolab::CellPopulation cells;
  cells.seedNoms(world, evolab::kWorldCellSize, evolab::kTerrainHeightScale, config.nomCount, config.seed);
  cells.installFeedbagReproductionOracle(world, evolab::kWorldCellSize, evolab::kTerrainHeightScale,
                                       world.tickCount());

  float prevSun = 0.0f;
  std::vector<CycleDiagnostic> dawnSamples;
  CycleDiagnosticResult result;

  result.nightMinBlobs = std::numeric_limits<int>::max();

  for (int tick = 0; tick < startTick + cycleTicks; ++tick) {
    world.tick();
    const float sun = dayCycle.sunIntensity(world.tickCount());
    energon.tick(world, sun, evolab::kWorldCellSize, evolab::kTerrainHeightScale,
                 cells.liveCampNomCount());
    cells.tick(world, energon, evolab::kWorldCellSize, evolab::kTerrainHeightScale, sun);
    if (tick < startTick) {
      prevSun = sun;
      continue;
    }

    const evolab::EnergonStats stats = evolab::computeEnergonStats(energon);
    const evolab::EnergonSunfallTickStats& ss = energon.lastSunfallTickStats();
    if (sun > 0.0f) {
      ++result.daylight;
      result.dayMaxBlobs = std::max(result.dayMaxBlobs, stats.blobCount);
      result.maxFullnessDay = std::max(result.maxFullnessDay, ss.fieldFullness);
      if (ss.spawnProbability < 0.99f) {
        ++result.throttleDaylight;
      }
    } else {
      result.nightMinBlobs = std::min(result.nightMinBlobs, stats.blobCount);
      if (ss.nightFamineRain) {
        ++result.nightFamineTicks;
      }
    }
    if (prevSun <= 0.01f && sun > 0.01f) {
      CycleDiagnostic dawn;
      dawn.tick = tick + 1;
      dawn.sun = sun;
      dawn.blobs = stats.blobCount;
      dawn.wetEdibleBytes = stats.wetEdibleBytes;
      dawn.falling = stats.falling;
      dawn.fullness = ss.fieldFullness;
      dawn.spawnProbability = ss.spawnProbability;
      dawn.nominalExpected = ss.nominalExpected;
      dawn.spawned = ss.spawnedBlobs;
      dawnSamples.push_back(dawn);
    }
    prevSun = sun;
  }

  if (result.nightMinBlobs == std::numeric_limits<int>::max()) {
    result.nightMinBlobs = 0;
  }

  std::cout << "CYCLE_DIAG startTick=" << startTick << " cycleTicks=" << cycleTicks
            << " throttleDaylight=" << result.throttleDaylight << "/" << result.daylight
            << " maxFullnessDay=" << result.maxFullnessDay << " dayMaxBlobs=" << result.dayMaxBlobs
            << " nightMinBlobs=" << result.nightMinBlobs
            << " nightFamineTicks=" << result.nightFamineTicks << std::endl;
  for (const CycleDiagnostic& dawn : dawnSamples) {
    std::cout << "  dawn tick=" << dawn.tick << " blobs=" << dawn.blobs
              << " bytes=" << dawn.wetEdibleBytes << " falling=" << dawn.falling
              << " fullness=" << dawn.fullness << " spawnProb=" << dawn.spawnProbability
              << " nominal=" << dawn.nominalExpected << " spawned=" << dawn.spawned
              << " sun=" << dawn.sun << std::endl;
  }
  return result;
}

}  // namespace

TEST_CASE("energon cycle diagnostic dawn trough vs spawn gate", "[tank][diagnostic][energon]") {
  runCycleDiagnostic(4200, static_cast<int>(evolab::kVisualDayCyclePeriodTicks * 2));
  REQUIRE(true);
}

TEST_CASE("night famine rain limits dawn blob trough", "[tank][energon]") {
  const CycleDiagnosticResult cycle =
      runCycleDiagnostic(4200, static_cast<int>(evolab::kVisualDayCyclePeriodTicks * 2));
  REQUIRE(cycle.throttleDaylight == 0);
  REQUIRE(cycle.nightFamineTicks > 0);
  REQUIRE(cycle.nightMinBlobs > 250);
}

TEST_CASE("sunfall spawn probability scales with field fullness", "[energon][tank]") {
  REQUIRE(evolab::energonSunfallSpawnProbability(1.0f) ==
          Catch::Approx(evolab::kEnergonSpawnProbAtFull));
  REQUIRE(evolab::energonSunfallSpawnProbability(0.5f) == Catch::Approx(0.51f));
  REQUIRE(evolab::energonSunfallSpawnProbability(0.2f) == Catch::Approx(1.0f));
  REQUIRE(evolab::energonSunfallSpawnProbability(0.0f) == Catch::Approx(1.0f));
  REQUIRE(evolab::energonSunfallSpawnProbability(0.75f) == Catch::Approx(0.26f));
}

TEST_CASE("rain gate fullness follows wet food not dry blob cap", "[energon][tank]") {
  constexpr int kMaxBlobs = 6000;
  constexpr int kLive = 60;
  const float quota = evolab::rainCycleFieldBytesForPopulation(kLive);
  REQUIRE(quota > 0.0f);

  const float noFoodHighBlob =
      evolab::energonRainGateFullness(0, static_cast<int>(kMaxBlobs * 0.85f), kMaxBlobs, kLive);
  const float fullFoodLowBlob = evolab::energonRainGateFullness(
      static_cast<int>(quota), static_cast<int>(kMaxBlobs * 0.1f), kMaxBlobs, kLive);

  REQUIRE(evolab::energonSunfallSpawnProbability(noFoodHighBlob) >
          evolab::energonSunfallSpawnProbability(0.85f));
  REQUIRE(evolab::energonSunfallSpawnProbability(fullFoodLowBlob) <
          evolab::energonSunfallSpawnProbability(noFoodHighBlob));
}

TEST_CASE("rain gate ignores blob shell count when bytes are low", "[energon][tank]") {
  constexpr int kLive = 60;
  const float lowBytes = 800.0f;
  const float fewBlobs =
      evolab::energonRainGateFullness(static_cast<int>(lowBytes), 120, 6000, kLive);
  const float manyBlobs =
      evolab::energonRainGateFullness(static_cast<int>(lowBytes), 3200, 6000, kLive);
  REQUIRE(fewBlobs == Catch::Approx(manyBlobs));
  REQUIRE(evolab::energonSunfallSpawnProbability(fewBlobs) == Catch::Approx(1.0f));
}

TEST_CASE("visual session retains camp nom morphology at tick 600", "[tier3][marathon][viability]") {
  const evolab::CellPopulationStats stats = runVisualSessionStats(600);
  INFO("camp=" << stats.campNomOrganisms << " degraded=" << stats.degradedNomOrganisms
               << " stem=" << stats.stemCells);
  REQUIRE(stats.campNomOrganisms >= 40);
  REQUIRE(stats.stemCells <= 15);
  REQUIRE(stats.degradedNomOrganisms <= 10);
}

TEST_CASE("visual session retains P M A neurons on seeded campers at tick 600",
          "[tier3][marathon][viability]") {
  evolab::SimConfig config;
  evolab::BarrenWorld world(config.seed, config.resolution, evolab::makeTideFromConfig(config));
  evolab::DayCycle dayCycle(evolab::kVisualDayCyclePeriodTicks);
  const evolab::EnergonConfig energonConfig = makeVisualTankEnergonConfig(config.nomCount);
  evolab::EnergonField energon(config.seed, energonConfig);
  evolab::CellPopulation cells;
  cells.seedNoms(world, evolab::kWorldCellSize, evolab::kTerrainHeightScale, config.nomCount,
                 config.seed);
  cells.installFeedbagReproductionOracle(world, evolab::kWorldCellSize, evolab::kTerrainHeightScale,
                                       world.tickCount());

  for (int tick = 0; tick < 600; ++tick) {
    world.tick();
    const float sun = dayCycle.sunIntensity(world.tickCount());
    energon.tick(world, sun, evolab::kWorldCellSize, evolab::kTerrainHeightScale,
                 cells.liveCampNomCount());
    cells.tick(world, energon, evolab::kWorldCellSize, evolab::kTerrainHeightScale, sun);
  }

  int seedFloorOk = 0;
  int seedCohort = 0;
  for (const evolab::Organism& organism : cells.organisms()) {
    if (!organism.alive || organism.feedbagOracle || organism.createdAtTick != 0) {
      continue;
    }
    ++seedCohort;
    if (evolab::organismHasCampNeuronFloor(organism)) {
      ++seedFloorOk;
    }
  }
  INFO("seedCohort=" << seedCohort << " neuronFloorOk=" << seedFloorOk);
  REQUIRE(seedCohort >= 40);
  REQUIRE(seedFloorOk >= 40);
}

TEST_CASE("visual tank marathon measures sunfall availability over 6000 ticks",
          "[tier3][tank][marathon][energon]") {
  const TankRunResult result = runTankSimulation(kTankMarathonTicks, kTankMarathonSampleInterval,
                                                 "tank_marathon_fuel.log");
  logTankRun("TANK_MARATHON", result);
  requireTankGateInvariants(result);
  // Pre-conservation baseline ~3639 same-tick eat+hub-drop ticks over 6000 ticks.
  REQUIRE(result.totalEatHubDropTicks < 800);
  REQUIRE(result.aliveEnd >= 8);
}

TEST_CASE("visual tank ultra-marathon tracks seed spawn luck over 18000 ticks",
          "[tier3][optional][tank][ultra-marathon][energon]") {
  const TankRunResult result =
      runTankSimulation(kTankUltraMarathonTicks, kTankUltraMarathonSampleInterval,
                        "tank_ultra_marathon_fuel.log");
  logTankRun("TANK_ULTRA", result);
  requireTankGateInvariants(result);
  REQUIRE(result.spawnLuck.seedCohort > 0);
  REQUIRE(result.spawnLuck.feedbagAlive == 1);
  INFO("Ultra-marathon endgame: feedbag oracle + offspring = "
       << result.spawnLuck.feedbagAlive << " + " << result.spawnLuck.offspringAlive
       << " of " << result.aliveEnd << " alive");
}
