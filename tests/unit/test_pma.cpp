#include "game/OrganismDrawer.hpp"
#include "game/OrganismInspector.hpp"
#include "sim/BarrenWorld.hpp"
#include "sim/CellConstants.hpp"
#include "sim/CellPopulation.hpp"
#include "sim/Energon.hpp"
#include "sim/EnergonString.hpp"
#include "sim/Organism.hpp"
#include "sim/TideAdvection.hpp"
#include "sim/WorldConstants.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>

namespace {

float worldHalfExtent(const evolab::BarrenWorld& world, float cellSize) {
  const int res = world.heightmap().resolution;
  if (res <= 1 || cellSize <= 0.0f) {
    return 0.0f;
  }
  return static_cast<float>(res - 1) * cellSize * 0.5f;
}

evolab::EnergonBlob makeWetFoodBlob(float x, float z, std::uint8_t bytes) {
  evolab::EnergonBlob blob;
  blob.data = bytes;
  blob.remaining = 1;
  blob.initialBytes = 1;
  blob.origin = evolab::EnergonOrigin::Sunfall;
  blob.x = x;
  blob.z = z;
  blob.y = 0.0f;
  blob.tailX = x;
  blob.tailZ = z;
  blob.headX = x;
  blob.headZ = z;
  blob.grounded = true;
  blob.onWet = true;
  blob.ttl = 60.0f;
  evolab::energonBlobInitPoint(blob);
  return blob;
}

bool blockProbesClear(const evolab::BarrenWorld& world, float cellSize, float halfExtent,
                      float wx, float wz, float heading, float senseRadius) {
  const float fx = std::sin(heading);
  const float fz = std::cos(heading);
  const float samples[] = {0.35f, 0.6f, 0.85f, 1.0f};
  for (float fraction : samples) {
    const float probeX = wx + fx * senseRadius * fraction;
    const float probeZ = wz + fz * senseRadius * fraction;
    float clampedX = probeX;
    float clampedZ = probeZ;
    evolab::clampWorldPosition(clampedX, clampedZ, halfExtent, cellSize * 0.25f);
    const bool atBoundary =
        std::abs(clampedX - probeX) > 1.0e-3f || std::abs(clampedZ - probeZ) > 1.0e-3f;
    if (atBoundary || !world.isWetWorld(clampedX, clampedZ, cellSize)) {
      return false;
    }
  }
  return true;
}

bool findOpenWaterSite(const evolab::BarrenWorld& world, float cellSize, float heading,
                       float senseRadius, float& wx, float& wz) {
  const float half = worldHalfExtent(world, cellSize);
  for (float x = -half; x <= half; x += evolab::kWorldCellSize * 0.5f) {
    for (float z = -half; z <= half; z += evolab::kWorldCellSize * 0.5f) {
      if (world.isWetWorld(x, z, cellSize) &&
          blockProbesClear(world, cellSize, half, x, z, heading, senseRadius)) {
        wx = x;
        wz = z;
        return true;
      }
    }
  }
  return false;
}

bool findWetSiteWithDryAhead(const evolab::BarrenWorld& world, float cellSize, float heading,
                             float probeDistance, float& wx, float& wz, float& probeX,
                             float& probeZ) {
  const float fx = std::sin(heading);
  const float fz = std::cos(heading);
  const float half = worldHalfExtent(world, cellSize);
  for (float x = -half; x <= half; x += evolab::kWorldCellSize * 0.5f) {
    for (float z = -half; z <= half; z += evolab::kWorldCellSize * 0.5f) {
      if (!world.isWetWorld(x, z, cellSize)) {
        continue;
      }
      const float px = x + fx * probeDistance;
      const float pz = z + fz * probeDistance;
      if (world.isWetWorld(px, pz, cellSize)) {
        continue;
      }
      wx = x;
      wz = z;
      probeX = px;
      probeZ = pz;
      return true;
    }
  }
  return false;
}

}  // namespace

TEST_CASE("PMA factory builds P-M-A chain with axons and split fuel", "[pma]") {
  evolab::Organism organism =
      evolab::makePerceptorMouthActuatorOrganism(1, 0.0f, 0.0f, 1.0f, 100, 0, 1.2f);
  REQUIRE(organism.isPmaNom());
  REQUIRE(!organism.isMouthActuatorNom());
  REQUIRE(organism.bodyStorage.empty());
  REQUIRE(organism.nodes.size() == 3);
  REQUIRE(organism.links.size() == 2);
  REQUIRE(organism.neuralAxons.size() == 4);
  REQUIRE(organism.findNode(1)->neuron == evolab::NeuronType::Perceptor);
  REQUIRE(organism.findNode(2)->neuron == evolab::NeuronType::Mouth);
  REQUIRE(organism.findNode(3)->neuron == evolab::NeuronType::Actuator);
  REQUIRE(organism.findNode(1)->store.size() == 33);
  REQUIRE(organism.findNode(2)->store.size() == 33);
  REQUIRE(organism.findNode(3)->store.size() == 34);
  REQUIRE(organism.findNeuralAxon(1, 2) != nullptr);
  REQUIRE(organism.findNeuralAxon(1, 3) != nullptr);
  REQUIRE(organism.findNeuralAxon(2, 3) != nullptr);
  REQUIRE(organism.findNeuralAxon(3, 2) != nullptr);
}

TEST_CASE("perceptor scan detects food ahead and emits P axon tags", "[pma]") {
  evolab::BarrenWorld world(7, 32);
  float wetX = 0.0f;
  float wetZ = 0.0f;
  const float senseRadius = evolab::kWorldCellSize * evolab::kPerceptorSenseRadiusFactor;
  REQUIRE(findOpenWaterSite(world, evolab::kWorldCellSize, 0.0f, senseRadius, wetX, wetZ));

  evolab::EnergonField energon(1, {});
  evolab::Organism organism = evolab::makePerceptorMouthActuatorOrganism(
      1, wetX, wetZ, 1.0f, 120, 0, evolab::kWorldCellSize);
  organism.alive = true;
  organism.heading = 0.0f;
  organism.updateKinematics(world, evolab::kWorldCellSize, evolab::kTerrainHeightScale);

  const float foodZ =
      organism.findNode(1)->worldZ + evolab::kWorldCellSize * 2.0f;
  evolab::EnergonBlob blob = makeWetFoodBlob(organism.findNode(1)->worldX, foodZ, 0x42);
  energon.injectBlob(blob);

  const std::size_t storeBefore = organism.findNode(1)->store.size();
  const float half = worldHalfExtent(world, evolab::kWorldCellSize);
  organism.perceive(world, energon, evolab::kWorldCellSize, half, {organism}, 10);

  REQUIRE(organism.lastPerceptScanPaid);
  REQUIRE(organism.lastPerceptBytesPaid ==
          evolab::kPerceptorScanCostPerTick + evolab::kPerceptorTransductionCostPerTick);
  REQUIRE(organism.findNode(1)->store.size() ==
          storeBefore - organism.lastPerceptBytesPaid);
  REQUIRE(organism.lastPerceptTag == evolab::kSignalTagISenseFood);
  REQUIRE(organism.lastPerceptBearing == Catch::Approx(0.0f).margin(0.05f));

  const evolab::NeuralAxon* toMouth = organism.findNeuralAxon(1, 2);
  const evolab::NeuralAxon* toActuator = organism.findNeuralAxon(1, 3);
  REQUIRE(toMouth != nullptr);
  REQUIRE(toActuator != nullptr);
  REQUIRE(toMouth->lastReceived.valid);
  REQUIRE(toActuator->lastReceived.valid);
  REQUIRE(toMouth->lastReceived.byte == evolab::kSignalTagISenseFood);
  REQUIRE(toActuator->lastReceived.byte == evolab::kSignalTagISenseFood);
  REQUIRE(toMouth->lastReceived.tick == 10);
}

TEST_CASE("perceptor scan without fuel skips perception", "[pma]") {
  evolab::BarrenWorld world(3, 32);
  evolab::EnergonField energon(1, {});
  evolab::Organism organism =
      evolab::makePerceptorMouthActuatorOrganism(1, 0.0f, 0.0f, 1.0f, 6, 0, 1.0f);
  organism.alive = true;
  organism.findNode(1)->store.clear();

  organism.perceive(world, energon, evolab::kWorldCellSize, 16.0f, {organism}, 1);

  REQUIRE(!organism.lastPerceptScanPaid);
  REQUIRE(organism.lastPerceptTag == 0);
}

TEST_CASE("perceptor prioritizes block over food", "[pma]") {
  evolab::BarrenWorld world(7, 32);
  float wetX = 0.0f;
  float wetZ = 0.0f;
  float probeX = 0.0f;
  float probeZ = 0.0f;
  const float probeDistance = evolab::kWorldCellSize * 2.5f;
  REQUIRE(findWetSiteWithDryAhead(world, evolab::kWorldCellSize, 0.0f, probeDistance, wetX, wetZ,
                                  probeX, probeZ));

  evolab::EnergonField energon(1, {});
  evolab::Organism organism = evolab::makePerceptorMouthActuatorOrganism(
      1, wetX, wetZ, 1.0f, 120, 0, evolab::kWorldCellSize);
  organism.alive = true;
  organism.heading = 0.0f;
  organism.updateKinematics(world, evolab::kWorldCellSize, evolab::kTerrainHeightScale);

  evolab::EnergonBlob blob = makeWetFoodBlob(probeX, probeZ, 0x11);
  energon.injectBlob(blob);

  const float half = worldHalfExtent(world, evolab::kWorldCellSize);
  organism.perceive(world, energon, evolab::kWorldCellSize, half, {organism}, 3);

  REQUIRE(organism.lastPerceptTag == evolab::kSignalTagISenseBlock);
}

TEST_CASE("population tick runs perceive between feed and advect", "[pma]") {
  evolab::BarrenWorld world(5, 32);
  evolab::EnergonField energon(1, {});
  evolab::CellPopulation population;
  population.seedPmaOrganisms(world, evolab::kWorldCellSize, evolab::kTerrainHeightScale, 3, 42);
  REQUIRE(!population.organisms().empty());

  world.tick();
  population.tick(world, energon, evolab::kWorldCellSize, evolab::kTerrainHeightScale);

  bool anyScan = false;
  for (const evolab::Organism& organism : population.organisms()) {
    anyScan = anyScan || organism.lastPerceptScanPaid;
  }
  REQUIRE(anyScan);
}

TEST_CASE("PMA visual startup path seeds 60 noms with inspectable 3-node chains", "[pma]") {
  evolab::BarrenWorld world(7, 32);
  evolab::EnergonField energon(1, {});
  evolab::CellPopulation population;
  population.seedPmaOrganisms(world, evolab::kWorldCellSize, evolab::kTerrainHeightScale, 60, 42);
  REQUIRE(population.organisms().size() == 60);

  for (const evolab::Organism& organism : population.organisms()) {
    REQUIRE(organism.isPmaNom());
    REQUIRE(organism.nodes.size() == 3);
  }

  for (int i = 0; i < 30; ++i) {
    world.tick();
    population.tick(world, energon, evolab::kWorldCellSize, evolab::kTerrainHeightScale);
  }

  for (const evolab::Organism& organism : population.organisms()) {
    if (!organism.alive) {
      continue;
    }
    const std::string label =
        evolab::game::formatOrganismArchitectureLabel(organism, world.tickCount());
    if (organism.isPmaNom()) {
      REQUIRE(label.find("[PMA]") != std::string::npos);
    }
    REQUIRE(label.size() < 2048);
    const std::string hover = evolab::game::formatOrganismHoverSummary(organism);
    REQUIRE(hover.size() < 160);
  }

  const evolab::game::OrganismDrawBatch batch =
      evolab::game::buildOrganismDrawBatch(population.organisms(), 0.0f, 80.0f, 120.0f);
  REQUIRE(!batch.cellVerts.empty());
  REQUIRE(!batch.neuralLineVerts.empty());
}
