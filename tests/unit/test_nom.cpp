#include "game/OrganismDrawer.hpp"
#include "game/OrganismInspector.hpp"
#include "game/TerrainMesh.hpp"
#include "sim/BarrenWorld.hpp"
#include "sim/CellConstants.hpp"
#include "sim/CellPopulation.hpp"
#include "sim/Energon.hpp"
#include "sim/EnergonString.hpp"
#include "sim/Organism.hpp"
#include "sim/TideAdvection.hpp"
#include "sim/WorldConstants.hpp"

#include <chrono>

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

TEST_CASE("nom factory builds P-M-A chain with axons and split fuel", "[nom]") {
  evolab::Organism organism = evolab::makeNomOrganism(1, 0.0f, 0.0f, 1.0f, 100, 0, 1.2f);
  REQUIRE(organism.isPmaNom());
  REQUIRE(organism.bodyStorage.empty());
  REQUIRE(organism.nodes.size() == 3);
  REQUIRE(organism.links.size() == 2);
  REQUIRE(organism.neuralAxons.size() == 4);
  REQUIRE(organism.findNode(1)->neuron == evolab::NeuronType::Perceptor);
  REQUIRE(organism.findNode(2)->neuron == evolab::NeuronType::Mouth);
  REQUIRE(organism.findNode(3)->neuron == evolab::NeuronType::Actuator);
  REQUIRE(organism.findNode(1)->store.size() == 33);
  REQUIRE(organism.findNode(2)->store.size() == evolab::kMouthLocalStoreMaxBytes);
  REQUIRE(organism.findNode(3)->store.size() == 35);
}

TEST_CASE("I ATE signal inhibits actuator stroke same tick", "[nom]") {
  evolab::BarrenWorld world(7, 32);
  float wetX = 0.0f;
  float wetZ = 0.0f;
  REQUIRE(findWetWorldSite(world, evolab::kWorldCellSize, wetX, wetZ));

  evolab::EnergonField energon(1, {});
  evolab::Organism organism =
      evolab::makeNomOrganism(1, wetX, wetZ, 1.0f, 100, 0, evolab::kWorldCellSize);
  organism.alive = true;

  evolab::NeuralAxon* mouthToActuator = organism.findNeuralAxon(2, 3);
  REQUIRE(mouthToActuator != nullptr);
  mouthToActuator->lastReceived.valid = true;
  mouthToActuator->lastReceived.byte = evolab::kSignalTagIAte;
  mouthToActuator->lastReceived.tick = world.tickCount();

  organism.advectRoot(world, energon, evolab::kWorldCellSize, evolab::kTerrainHeightScale,
                      worldHalfExtent(world, evolab::kWorldCellSize));

  REQUIRE(organism.lastActuatorInhibited);
  REQUIRE(!organism.lastStrokePaid);
}

TEST_CASE("mouth emits I ATE on bite before advect", "[nom]") {
  evolab::BarrenWorld world(13, 32);
  float wetX = 0.0f;
  float wetZ = 0.0f;
  REQUIRE(findWetWorldSite(world, evolab::kWorldCellSize, wetX, wetZ));

  evolab::Organism organism =
      evolab::makeNomOrganism(1, wetX, wetZ, 1.0f, 100, 0, evolab::kWorldCellSize);
  organism.alive = true;

  evolab::SkeletonNode* mouth = organism.findNode(2);
  REQUIRE(mouth != nullptr);
  mouth->ateThisTick = true;
  organism.emitPreAdvectSignals(world.tickCount());

  const evolab::NeuralAxon* axon = organism.findNeuralAxon(2, 3);
  REQUIRE(axon != nullptr);
  REQUIRE(axon->lastReceived.valid);
  REQUIRE(axon->lastReceived.byte == evolab::kSignalTagIAte);
}

TEST_CASE("nom stroke translates skeleton rigidly", "[nom]") {
  evolab::BarrenWorld world(7, 32);
  float wetX = 0.0f;
  float wetZ = 0.0f;
  REQUIRE(findWetWorldSite(world, evolab::kWorldCellSize, wetX, wetZ));

  evolab::EnergonField energon(1, {});
  evolab::Organism organism =
      evolab::makeNomOrganism(1, wetX, wetZ, 1.0f, 100, 0, evolab::kWorldCellSize);
  organism.heading = 0.0f;
  organism.alive = true;

  evolab::SkeletonNode* mouth = organism.findNode(2);
  evolab::SkeletonNode* actuator = organism.findNode(3);
  REQUIRE(mouth != nullptr);
  REQUIRE(actuator != nullptr);

  const float mouthStartX = mouth->worldX;
  const float mouthStartZ = mouth->worldZ;
  const float actuatorStartX = actuator->worldX;
  const float actuatorStartZ = actuator->worldZ;
  const float boneBefore = std::hypot(actuatorStartX - mouthStartX, actuatorStartZ - mouthStartZ);

  organism.advectRoot(world, energon, evolab::kWorldCellSize, evolab::kTerrainHeightScale,
                      worldHalfExtent(world, evolab::kWorldCellSize));

  REQUIRE(organism.lastStrokePaid);
  const float boneAfter =
      std::hypot(actuator->worldX - mouth->worldX, actuator->worldZ - mouth->worldZ);
  REQUIRE(boneBefore == Catch::Approx(boneAfter).margin(1e-3f));
}

TEST_CASE("perceptor scan detects food ahead and emits P axon tags", "[nom]") {
  evolab::BarrenWorld world(7, 32);
  float wetX = 0.0f;
  float wetZ = 0.0f;
  const float senseRadius = evolab::kWorldCellSize * evolab::kPerceptorSenseRadiusFactor;
  REQUIRE(findOpenWaterSite(world, evolab::kWorldCellSize, 0.0f, senseRadius, wetX, wetZ));

  evolab::EnergonField energon(1, {});
  evolab::Organism organism =
      evolab::makeNomOrganism(1, wetX, wetZ, 1.0f, 120, 0, evolab::kWorldCellSize);
  organism.alive = true;
  organism.heading = 0.0f;
  organism.updateKinematics(world, evolab::kWorldCellSize, evolab::kTerrainHeightScale);

  const float foodZ = organism.findNode(1)->worldZ + evolab::kWorldCellSize * 2.0f;
  energon.injectBlob(makeWetFoodBlob(organism.findNode(1)->worldX, foodZ, 0x42));

  const float half = worldHalfExtent(world, evolab::kWorldCellSize);
  organism.perceive(world, energon, evolab::kWorldCellSize, half, {organism}, 10);

  REQUIRE(organism.lastPerceptTag == evolab::kSignalTagISenseFood);
  REQUIRE(organism.findNeuralAxon(1, 2)->lastReceived.byte == evolab::kSignalTagISenseFood);
}

TEST_CASE("perceptor prioritizes block over food", "[nom]") {
  evolab::BarrenWorld world(7, 32);
  float wetX = 0.0f;
  float wetZ = 0.0f;
  float probeX = 0.0f;
  float probeZ = 0.0f;
  const float probeDistance = evolab::kWorldCellSize * 2.5f;
  REQUIRE(findWetSiteWithDryAhead(world, evolab::kWorldCellSize, 0.0f, probeDistance, wetX, wetZ,
                                  probeX, probeZ));

  evolab::EnergonField energon(1, {});
  evolab::Organism organism =
      evolab::makeNomOrganism(1, wetX, wetZ, 1.0f, 120, 0, evolab::kWorldCellSize);
  organism.alive = true;
  organism.heading = 0.0f;
  organism.updateKinematics(world, evolab::kWorldCellSize, evolab::kTerrainHeightScale);
  energon.injectBlob(makeWetFoodBlob(probeX, probeZ, 0x11));

  const float half = worldHalfExtent(world, evolab::kWorldCellSize);
  organism.perceive(world, energon, evolab::kWorldCellSize, half, {organism}, 3);

  REQUIRE(organism.lastPerceptTag == evolab::kSignalTagISenseBlock);
}

TEST_CASE("population tick runs feed perceive then advect", "[nom]") {
  evolab::BarrenWorld world(5, 32);
  evolab::EnergonField energon(1, {});
  evolab::CellPopulation population;
  population.seedNoms(world, evolab::kWorldCellSize, evolab::kTerrainHeightScale, 3, 42);
  REQUIRE(!population.organisms().empty());

  world.tick();
  population.tick(world, energon, evolab::kWorldCellSize, evolab::kTerrainHeightScale);

  bool anyScan = false;
  for (const evolab::Organism& organism : population.organisms()) {
    anyScan = anyScan || organism.lastPerceptScanPaid;
  }
  REQUIRE(anyScan);
}

TEST_CASE("starved actuator neuron dies and releases energon", "[nom]") {
  evolab::BarrenWorld world(7, 32);
  float wetX = 0.0f;
  float wetZ = 0.0f;
  REQUIRE(findWetWorldSite(world, evolab::kWorldCellSize, wetX, wetZ));

  evolab::EnergonField energon(1, {});
  evolab::Organism organism =
      evolab::makeNomOrganism(1, wetX, wetZ, 1.0f, 200, 0, evolab::kWorldCellSize);
  evolab::SkeletonNode* actuator = organism.findNode(3);
  REQUIRE(actuator != nullptr);
  actuator->store.clear();
  organism.alive = true;

  organism.tickNeuronViability(energon);

  REQUIRE(!actuator->alive);
  REQUIRE(organism.findNeuralAxon(2, 3) == nullptr);
  REQUIRE(organism.alive);
}

TEST_CASE("startup generation benchmark", "[nom][.benchmark]") {
  using clock = std::chrono::steady_clock;
  const auto t0 = clock::now();
  evolab::BarrenWorld world(42, 128);
  const auto t1 = clock::now();
  evolab::game::TerrainMesh mesh =
      evolab::game::buildTerrainMesh(world.heightmap(), evolab::kWorldCellSize);
  const auto t2 = clock::now();
  evolab::CellPopulation population;
  population.seedNoms(world, evolab::kWorldCellSize, evolab::kTerrainHeightScale, 60, 42);
  const auto t3 = clock::now();

  const auto worldMs = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();
  const auto meshMs = std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count();
  const auto seedMs = std::chrono::duration_cast<std::chrono::milliseconds>(t3 - t2).count();

  INFO("worldMs=" << worldMs << " meshMs=" << meshMs << " seedMs=" << seedMs);
  REQUIRE(population.organisms().size() == 60);
  REQUIRE(worldMs < 5000);
  REQUIRE(meshMs < 1000);
  REQUIRE(seedMs < 5000);
}

TEST_CASE("nom seeds and renders for visual startup", "[nom]") {
  evolab::BarrenWorld world(7, 32);
  evolab::CellPopulation population;
  population.seedNoms(world, evolab::kWorldCellSize, evolab::kTerrainHeightScale, 60, 42);
  REQUIRE(population.organisms().size() == 60);

  for (const evolab::Organism& organism : population.organisms()) {
    REQUIRE(organism.isPmaNom());
    const std::string label = evolab::game::formatOrganismArchitectureLabel(organism, 0);
    REQUIRE(label.find("P-M-A Nom") != std::string::npos);
  }

  const evolab::game::OrganismDrawBatch batch =
      evolab::game::buildOrganismDrawBatch(population.organisms(), 0.0f, 80.0f, 120.0f);
  REQUIRE(!batch.cellVerts.empty());
  REQUIRE(!batch.neuralLineVerts.empty());
}
