#include "sim/BarrenWorld.hpp"
#include "sim/CampTopology.hpp"
#include "sim/CellPopulation.hpp"
#include "sim/DayCycle.hpp"
#include "sim/Energon.hpp"
#include "sim/EnergonRain.hpp"
#include "sim/NeuronFuel.hpp"
#include "sim/NeuronStem.hpp"
#include "sim/SimConfig.hpp"
#include "sim/WorldConstants.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <iostream>

namespace {

evolab::EnergonConfig makeVisualTankEnergonConfig(int nomCount) {
  evolab::EnergonConfig config;
  config.populationScaledRain = true;
  config.rainPopulationBaseline = nomCount;
  config.spawnRateMax = 10.0f;
  config.maxBlobs = std::max(4000, nomCount * 100);
  return config;
}

struct LocomotionSample {
  int ticks = 0;
  int strokeTicks = 0;
  int inhibitedTicks = 0;
  int lockedFoodTicks = 0;
  int zeroActuatorFuelTicks = 0;
  int hubRepleteTicks = 0;
  float displacementSum = 0.0f;
  float netDriveSum = 0.0f;
  std::size_t actuatorFuelSum = 0;
  std::size_t hubFuelSum = 0;
};

}  // namespace

TEST_CASE("visual tank locomotion diagnostic over 600 ticks", "[diagnostic][locomotion]") {
  evolab::SimConfig config;
  evolab::BarrenWorld world(config.seed, config.resolution, evolab::makeTideFromConfig(config));
  evolab::DayCycle dayCycle(evolab::kVisualDayCyclePeriodTicks);
  const evolab::EnergonConfig energonConfig = makeVisualTankEnergonConfig(config.nomCount);
  evolab::EnergonField energon(config.seed, energonConfig);
  evolab::CellPopulation cells;
  cells.seedNoms(world, evolab::kWorldCellSize, evolab::kTerrainHeightScale, config.nomCount,
                 config.seed);

  LocomotionSample sample;
  float totalStartX = 0.0f;
  float totalStartZ = 0.0f;
  int startCount = 0;

  for (const evolab::Organism& organism : cells.organisms()) {
    if (!organism.alive || !organism.isCampNom() || organism.createdAtTick != 0) {
      continue;
    }
    totalStartX += organism.rootWorldX();
    totalStartZ += organism.rootWorldZ();
    ++startCount;
  }

  for (int tick = 0; tick < 600; ++tick) {
    world.tick();
    const float sun = dayCycle.sunIntensity(world.tickCount());
    energon.tick(world, sun, evolab::kWorldCellSize, evolab::kTerrainHeightScale,
                 cells.liveCampNomCount());
    cells.tick(world, energon, evolab::kWorldCellSize, evolab::kTerrainHeightScale, sun);

    for (const evolab::Organism& organism : cells.organisms()) {
      if (!organism.alive || !organism.isCampNom() || organism.feedbagOracle ||
          organism.createdAtTick != 0) {
        continue;
      }
      ++sample.ticks;
      if (organism.lastStrokePaid) {
        ++sample.strokeTicks;
      }
      if (organism.lastActuatorInhibited) {
        ++sample.inhibitedTicks;
      }
      const evolab::SkeletonNode* perceptor =
          evolab::findNeuronNode(organism, evolab::NeuronType::Perceptor, true);
      if (perceptor != nullptr && perceptor->focusLocked &&
          organism.lastPerceptFocusKind == evolab::PerceptFocusKind::Food) {
        ++sample.lockedFoodTicks;
      }
      const evolab::SkeletonNode* actuator =
          evolab::findNeuronNode(organism, evolab::NeuronType::Actuator, true);
      const std::size_t actuatorFuel = actuator != nullptr ? actuator->store.size() : 0;
      if (actuatorFuel < evolab::kActuatorStrokeCostPerTick) {
        ++sample.zeroActuatorFuelTicks;
      }
      sample.actuatorFuelSum += actuatorFuel;
      sample.hubFuelSum += organism.computerHubFuelBytes();
      if (organism.lastActuatorInteroception.hubSatiation >=
          evolab::confidenceToUnit(evolab::kComputerSatiationConfidence)) {
        ++sample.hubRepleteTicks;
      }
      sample.displacementSum += organism.lastDisplacement;
      sample.netDriveSum += organism.lastActuatorNetDrive;
    }
  }

  float totalEndX = 0.0f;
  float totalEndZ = 0.0f;
  int endCount = 0;
  for (const evolab::Organism& organism : cells.organisms()) {
    if (!organism.alive || !organism.isCampNom() || organism.createdAtTick != 0) {
      continue;
    }
    totalEndX += organism.rootWorldX();
    totalEndZ += organism.rootWorldZ();
    ++endCount;
  }

  const float cohortDrift =
      endCount > 0 && startCount > 0
          ? std::hypot(totalEndX / endCount - totalStartX / startCount,
                       totalEndZ / endCount - totalStartZ / startCount)
          : 0.0f;

  const float strokeRate =
      sample.ticks > 0 ? static_cast<float>(sample.strokeTicks) / static_cast<float>(sample.ticks)
                       : 0.0f;
  const float avgDisp =
      sample.ticks > 0 ? sample.displacementSum / static_cast<float>(sample.ticks) : 0.0f;
  const float avgNetDrive =
      sample.ticks > 0 ? sample.netDriveSum / static_cast<float>(sample.ticks) : 0.0f;
  const float avgActuatorFuel =
      sample.ticks > 0 ? static_cast<float>(sample.actuatorFuelSum) / static_cast<float>(sample.ticks)
                       : 0.0f;
  const float avgHubFuel =
      sample.ticks > 0 ? static_cast<float>(sample.hubFuelSum) / static_cast<float>(sample.ticks)
                       : 0.0f;

  INFO("seedCohort start=" << startCount << " end=" << endCount << " cohortMeanDrift=" << cohortDrift
                           << " strokeRate=" << strokeRate << " avgDispPerTick=" << avgDisp
                           << " avgNetDrive=" << avgNetDrive << " avgActuatorFuel=" << avgActuatorFuel
                           << " avgHubFuel=" << avgHubFuel
                           << " inhibitedRate="
                           << (sample.ticks > 0
                                   ? static_cast<float>(sample.inhibitedTicks) /
                                         static_cast<float>(sample.ticks)
                                   : 0.0f)
                           << " lockedFoodRate="
                           << (sample.ticks > 0
                                   ? static_cast<float>(sample.lockedFoodTicks) /
                                         static_cast<float>(sample.ticks)
                                   : 0.0f)
                           << " zeroActuatorFuelRate="
                           << (sample.ticks > 0
                                   ? static_cast<float>(sample.zeroActuatorFuelTicks) /
                                         static_cast<float>(sample.ticks)
                                   : 0.0f)
                           << " hubRepleteRate="
                           << (sample.ticks > 0
                                   ? static_cast<float>(sample.hubRepleteTicks) /
                                         static_cast<float>(sample.ticks)
                                   : 0.0f));

  std::cout << "LOCOMOTION_DIAG strokeRate=" << strokeRate << " avgDisp=" << avgDisp
            << " cohortDrift=" << cohortDrift << " avgActuatorFuel=" << avgActuatorFuel
            << " avgHubFuel=" << avgHubFuel << " zeroActuatorFuelRate="
            << (sample.ticks > 0
                    ? static_cast<float>(sample.zeroActuatorFuelTicks) /
                          static_cast<float>(sample.ticks)
                    : 0.0f)
            << std::endl;

  REQUIRE(sample.ticks > 0);
  REQUIRE(strokeRate > 0.05f);
  REQUIRE(avgDisp > 0.001f);
}
