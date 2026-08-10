#include "sim/BarrenWorld.hpp"
#include "sim/CellConstants.hpp"
#include "sim/CellPopulation.hpp"
#include "sim/Energon.hpp"
#include "sim/Organism.hpp"
#include "sim/WorldConstants.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <algorithm>

namespace {

float worldHalfExtent(const evolab::BarrenWorld& world, float cellSize) {
  const int res = world.heightmap().resolution;
  if (res <= 1 || cellSize <= 0.0f) {
    return 0.0f;
  }
  return static_cast<float>(res - 1) * cellSize * 0.5f;
}

bool findDryWorldSite(const evolab::BarrenWorld& world, float cellSize, float& wx, float& wz) {
  const float half = worldHalfExtent(world, cellSize);
  for (float x = -half; x <= half; x += evolab::kWorldCellSize * 0.5f) {
    for (float z = -half; z <= half; z += evolab::kWorldCellSize * 0.5f) {
      if (!world.isWetWorld(x, z, cellSize)) {
        wx = x;
        wz = z;
        return true;
      }
    }
  }
  return false;
}

bool findWetWorldSite(const evolab::BarrenWorld& world, float cellSize, float& wx, float& wz) {
  const float half = worldHalfExtent(world, cellSize);
  for (float x = -half; x <= half; x += evolab::kWorldCellSize * 0.5f) {
    for (float z = -half; z <= half; z += evolab::kWorldCellSize * 0.5f) {
      if (world.isWetWorld(x, z, cellSize)) {
        wx = x;
        wz = z;
        return true;
      }
    }
  }
  return false;
}

}  // namespace

TEST_CASE("actuator factory is single A node with no mouth", "[actuator]") {
  evolab::Organism organism = evolab::makeActuatorOrganism(1, 0.0f, 0.0f, 1.0f, 100, 0);
  REQUIRE(organism.actuatorCount() == 1);
  REQUIRE(organism.mouthCount() == 0);
  REQUIRE(organism.hasActuatorNeurons());
  REQUIRE(!organism.hasMouthNeurons());
  REQUIRE(organism.nodes.size() == 1);
  REQUIRE(organism.nodes.front().neuron == evolab::NeuronType::Actuator);
}

TEST_CASE("fueled actuator stroke records displacement proprioception", "[actuator]") {
  evolab::BarrenWorld world(7, 32);
  float wetX = 0.0f;
  float wetZ = 0.0f;
  REQUIRE(findWetWorldSite(world, evolab::kWorldCellSize, wetX, wetZ));

  evolab::EnergonField energon(1, {});
  evolab::Organism organism = evolab::makeActuatorOrganism(1, wetX, wetZ, 1.0f, 50, 0);
  organism.heading = 0.0f;
  organism.alive = true;

  organism.advectRoot(world, energon, evolab::kWorldCellSize, evolab::kTerrainHeightScale, 32.0f);

  REQUIRE(organism.lastStrokePaid);
  REQUIRE(organism.lastIntendedThrust > 0.0f);
  REQUIRE(organism.lastDisplacement > 0.0f);
}

TEST_CASE("actuator without fuel skips stroke but still senses tide delta", "[actuator]") {
  evolab::BarrenWorld world(11, 32);
  evolab::EnergonField energon(1, {});
  evolab::Organism organism = evolab::makeActuatorOrganism(1, 0.0f, 0.0f, 1.0f, 0, 0);
  organism.alive = true;

  organism.advectRoot(world, energon, evolab::kWorldCellSize, evolab::kTerrainHeightScale, 32.0f);

  REQUIRE(!organism.lastStrokePaid);
  REQUIRE(organism.lastIntendedThrust == 0.0f);
}

TEST_CASE("actuator nom dies on full tick loop with no mouth to refuel", "[actuator]") {
  evolab::BarrenWorld world(3, 32);
  evolab::EnergonField energon(1, {});
  evolab::CellPopulation population;
  population.seedActuatorOrganisms(world, evolab::kWorldCellSize, evolab::kTerrainHeightScale, 3,
                                   42);
  REQUIRE(population.stats().actuatorOrganisms == 3);
  REQUIRE(population.stats().stemCells == 0);

  for (int i = 0; i < 400000; ++i) {
    world.tick();
    population.tick(world, energon, evolab::kWorldCellSize, evolab::kTerrainHeightScale);
    if (population.organisms().empty()) {
      break;
    }
  }

  REQUIRE(population.organisms().empty());
}

TEST_CASE("actuator advect debits stroke bytes only", "[actuator]") {
  evolab::BarrenWorld world(17, 32);
  float wetX = 0.0f;
  float wetZ = 0.0f;
  REQUIRE(findWetWorldSite(world, evolab::kWorldCellSize, wetX, wetZ));

  evolab::EnergonField energon(1, {});
  evolab::Organism organism = evolab::makeActuatorOrganism(1, wetX, wetZ, 1.0f, 10, 0);
  organism.alive = true;

  organism.advectRoot(world, energon, evolab::kWorldCellSize, evolab::kTerrainHeightScale, 32.0f);

  REQUIRE(organism.lastStrokeBytesPaid == evolab::kActuatorStrokeCostPerTick);
  REQUIRE(organism.bodyStorage.size() == 10u - evolab::kActuatorStrokeCostPerTick);
}

TEST_CASE("actuator full tick crawl debits stroke plus basal", "[actuator]") {
  evolab::BarrenWorld world(19, 32);
  float wetX = 0.0f;
  float wetZ = 0.0f;
  REQUIRE(findWetWorldSite(world, evolab::kWorldCellSize, wetX, wetZ));

  evolab::EnergonField energon(1, {});
  evolab::Organism organism = evolab::makeActuatorOrganism(1, wetX, wetZ, 1.0f, 10, 0);
  organism.alive = true;

  organism.advectRoot(world, energon, evolab::kWorldCellSize, evolab::kTerrainHeightScale, 32.0f);
  organism.metabolise(world, evolab::kWorldCellSize, evolab::kTerrainHeightScale);

  REQUIRE(organism.bodyStorage.size() == 10u - evolab::kActuatorCrawlCostPerTick);
  REQUIRE(organism.alive);
}

TEST_CASE("actuator with one byte cannot wag tail", "[actuator]") {
  evolab::BarrenWorld world(23, 32);
  evolab::EnergonField energon(1, {});
  evolab::Organism organism = evolab::makeActuatorOrganism(1, 0.0f, 0.0f, 1.0f, 1, 0);
  organism.alive = true;

  organism.advectRoot(world, energon, evolab::kWorldCellSize, evolab::kTerrainHeightScale, 32.0f);

  REQUIRE(!organism.lastStrokePaid);
  REQUIRE(organism.lastStrokeBytesPaid == 0);
  REQUIRE(organism.bodyStorage.size() == 1);
}

TEST_CASE("actuator final two bytes wag then basal death on same tick", "[actuator]") {
  evolab::BarrenWorld world(29, 32);
  float wetX = 0.0f;
  float wetZ = 0.0f;
  REQUIRE(findWetWorldSite(world, evolab::kWorldCellSize, wetX, wetZ));

  evolab::EnergonField energon(1, {});
  evolab::Organism organism = evolab::makeActuatorOrganism(1, wetX, wetZ, 1.0f, 2, 0);
  organism.alive = true;

  organism.advectRoot(world, energon, evolab::kWorldCellSize, evolab::kTerrainHeightScale, 32.0f);
  organism.metabolise(world, evolab::kWorldCellSize, evolab::kTerrainHeightScale);

  REQUIRE(organism.lastStrokePaid);
  REQUIRE(organism.lastStrokeBytesPaid == evolab::kActuatorStrokeCostPerTick);
  REQUIRE(organism.bodyStorage.empty());
  REQUIRE(!organism.alive);
}

TEST_CASE("actuator stroke applies translation entropy to thrust", "[actuator]") {
  evolab::BarrenWorld world(13, 32);
  float wetX = 0.0f;
  float wetZ = 0.0f;
  REQUIRE(findWetWorldSite(world, evolab::kWorldCellSize, wetX, wetZ));

  evolab::EnergonField energon(1, {});
  evolab::Organism organism = evolab::makeActuatorOrganism(1, wetX, wetZ, 1.0f, 50, 0);
  organism.heading = 0.0f;
  organism.alive = true;

  organism.advectRoot(world, energon, evolab::kWorldCellSize, evolab::kTerrainHeightScale, 32.0f);

  const float gross =
      static_cast<float>(evolab::kActuatorStrokeCostPerTick) * evolab::kActuatorThrustPerStrokeByte;
  REQUIRE(organism.lastIntendedThrust == Catch::Approx(gross));
  REQUIRE(organism.lastMechanicalThrust ==
          Catch::Approx(gross * evolab::kActuatorTranslationEta));
  REQUIRE(organism.lastTranslationEntropyLoss ==
          Catch::Approx(static_cast<float>(evolab::kActuatorStrokeCostPerTick) *
                        (1.0f - evolab::kActuatorTranslationEta)));
}

TEST_CASE("population tick integrates actuator crawl before starvation", "[actuator]") {
  evolab::BarrenWorld world(5, 32);
  evolab::EnergonField energon(1, {});
  evolab::Organism organism = evolab::makeActuatorOrganism(1, 0.0f, 0.0f, 1.0f, 500, 0);
  organism.heading = 1.2f;
  organism.alive = true;

  const float startX = organism.rootWorldX();
  const float startZ = organism.rootWorldZ();

  for (int i = 0; i < 120; ++i) {
    world.tick();
    organism.advectRoot(world, energon, evolab::kWorldCellSize, evolab::kTerrainHeightScale, 32.0f);
    organism.metabolise(world, evolab::kWorldCellSize, evolab::kTerrainHeightScale);
  }

  const float dx = organism.rootWorldX() - startX;
  const float dz = organism.rootWorldZ() - startZ;
  const float moved = std::sqrt(dx * dx + dz * dz);
  REQUIRE(moved > 0.4f);
  REQUIRE(organism.alive);
}

TEST_CASE("actuator on dry land cannot stroke with fuel", "[actuator]") {
  evolab::BarrenWorld world(42, 64);
  float dryX = 0.0f;
  float dryZ = 0.0f;
  REQUIRE(findDryWorldSite(world, evolab::kWorldCellSize, dryX, dryZ));

  evolab::EnergonField energon(1, {});
  evolab::Organism organism =
      evolab::makeActuatorOrganism(1, dryX, dryZ, 1.0f, 50, 0);
  organism.alive = true;

  organism.advectRoot(world, energon, evolab::kWorldCellSize, evolab::kTerrainHeightScale,
                      worldHalfExtent(world, evolab::kWorldCellSize));

  REQUIRE(!organism.lastInWater);
  REQUIRE(!organism.lastStrokePaid);
  REQUIRE(organism.lastStrokeBytesPaid == 0);
  REQUIRE(organism.bodyStorage.size() == 50);
}

TEST_CASE("actuator stroke clamps to map edge", "[actuator]") {
  evolab::BarrenWorld world(7, 32);
  evolab::EnergonField energon(1, {});
  const float half = worldHalfExtent(world, evolab::kWorldCellSize);
  const float limit = half - evolab::kWorldCellSize * 0.25f;

  float wetX = 0.0f;
  float wetZ = 0.0f;
  REQUIRE(findWetWorldSite(world, evolab::kWorldCellSize, wetX, wetZ));
  wetX = std::min(wetX, limit - 0.001f);

  evolab::Organism organism =
      evolab::makeActuatorOrganism(1, wetX, wetZ, 1.0f, 500, 0);
  organism.heading = 1.5707963f;
  organism.alive = true;

  for (int i = 0; i < 200; ++i) {
    world.tick();
    organism.advectRoot(world, energon, evolab::kWorldCellSize, evolab::kTerrainHeightScale, half);
  }

  REQUIRE(std::abs(organism.rootWorldX()) <= limit + 1.0e-3f);
  REQUIRE(std::abs(organism.rootWorldZ()) <= limit + 1.0e-3f);
}
