#include "sim/BarrenWorld.hpp"
#include "sim/CellConstants.hpp"
#include "sim/CellPopulation.hpp"
#include "sim/Energon.hpp"
#include "sim/Organism.hpp"
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

TEST_CASE("mouth actuator factory is developmental [MA] chain", "[ma]") {
  evolab::Organism organism =
      evolab::makeMouthActuatorOrganism(1, 0.0f, 0.0f, 1.0f, 100, 0, 1.2f);
  REQUIRE(organism.isMouthActuatorNom());
  REQUIRE(organism.mouthCount() == 1);
  REQUIRE(organism.actuatorCount() == 1);
  REQUIRE(organism.nodes.size() == 2);
  REQUIRE(organism.nodes[0].neuron == evolab::NeuronType::Mouth);
  REQUIRE(organism.nodes[1].neuron == evolab::NeuronType::Actuator);
  REQUIRE(organism.neuralAxons.size() == 2);
}

TEST_CASE("I ATE signal inhibits actuator stroke same tick", "[ma]") {
  evolab::BarrenWorld world(7, 32);
  float wetX = 0.0f;
  float wetZ = 0.0f;
  REQUIRE(findWetWorldSite(world, evolab::kWorldCellSize, wetX, wetZ));

  evolab::EnergonField energon(1, {});
  evolab::Organism organism =
      evolab::makeMouthActuatorOrganism(1, wetX, wetZ, 1.0f, 100, 0, evolab::kWorldCellSize);
  organism.alive = true;

  evolab::NeuralAxon* mouthToActuator = organism.findNeuralAxon(1, 2);
  REQUIRE(mouthToActuator != nullptr);
  mouthToActuator->lastReceived.valid = true;
  mouthToActuator->lastReceived.byte = evolab::kSignalTagIAte;
  mouthToActuator->lastReceived.tick = world.tickCount();

  organism.advectRoot(world, energon, evolab::kWorldCellSize, evolab::kTerrainHeightScale,
                      worldHalfExtent(world, evolab::kWorldCellSize));

  REQUIRE(organism.lastActuatorInhibited);
  REQUIRE(!organism.lastStrokePaid);
  REQUIRE(organism.bodyStorage.empty());
  std::size_t totalFuel = 0;
  for (const evolab::SkeletonNode& node : organism.nodes) {
    totalFuel += node.store.size();
  }
  REQUIRE(totalFuel == 100);
}

TEST_CASE("mouth emits I ATE on bite before advect", "[ma]") {
  evolab::BarrenWorld world(13, 32);
  float wetX = 0.0f;
  float wetZ = 0.0f;
  REQUIRE(findWetWorldSite(world, evolab::kWorldCellSize, wetX, wetZ));

  evolab::EnergonField energon(1, {});
  evolab::Organism organism =
      evolab::makeMouthActuatorOrganism(1, wetX, wetZ, 1.0f, 100, 0, evolab::kWorldCellSize);
  organism.alive = true;

  evolab::SkeletonNode* mouth = organism.findNode(1);
  REQUIRE(mouth != nullptr);
  mouth->ateThisTick = true;
  organism.emitMouthActuatorPreAdvectSignals(world.tickCount());

  const evolab::NeuralAxon* axon = organism.findNeuralAxon(1, 2);
  REQUIRE(axon != nullptr);
  REQUIRE(axon->lastReceived.valid);
  REQUIRE(axon->lastReceived.byte == evolab::kSignalTagIAte);
  REQUIRE(axon->lastReceived.tick == world.tickCount());
}

TEST_CASE("mouth actuator stroke translates skeleton rigidly", "[ma]") {
  evolab::BarrenWorld world(7, 32);
  float wetX = 0.0f;
  float wetZ = 0.0f;
  REQUIRE(findWetWorldSite(world, evolab::kWorldCellSize, wetX, wetZ));

  evolab::EnergonField energon(1, {});
  evolab::Organism organism =
      evolab::makeMouthActuatorOrganism(1, wetX, wetZ, 1.0f, 100, 0, evolab::kWorldCellSize);
  organism.heading = 0.0f;
  organism.alive = true;

  evolab::SkeletonNode* mouth = organism.findNode(1);
  evolab::SkeletonNode* actuator = organism.findNode(2);
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
  REQUIRE(organism.lastDisplacement > 0.0f);
  const float mouthDelta =
      std::hypot(mouth->worldX - mouthStartX, mouth->worldZ - mouthStartZ);
  const float actuatorDelta =
      std::hypot(actuator->worldX - actuatorStartX, actuator->worldZ - actuatorStartZ);
  const float boneAfter =
      std::hypot(actuator->worldX - mouth->worldX, actuator->worldZ - mouth->worldZ);
  REQUIRE(mouthDelta > 0.0f);
  REQUIRE(actuatorDelta > 0.0f);
  REQUIRE(boneBefore == Catch::Approx(boneAfter).margin(1e-3f));
}

TEST_CASE("population tick feed precedes advect for mouth actuator", "[ma]") {
  evolab::BarrenWorld world(5, 32);
  evolab::EnergonField energon(1, {});
  evolab::CellPopulation population;
  population.seedMouthActuatorOrganisms(world, evolab::kWorldCellSize, evolab::kTerrainHeightScale,
                                        5, 42);
  REQUIRE(!population.organisms().empty());

  world.tick();
  population.tick(world, energon, evolab::kWorldCellSize, evolab::kTerrainHeightScale);
  REQUIRE(!population.organisms().empty());
}

TEST_CASE("MA spawn fuel splits between mouth and actuator stores", "[ma]") {
  evolab::Organism organism =
      evolab::makeMouthActuatorOrganism(1, 0.0f, 0.0f, 1.0f, 101, 0, 1.2f);
  REQUIRE(organism.bodyStorage.empty());
  REQUIRE(organism.findNode(1)->store.size() == 51);
  REQUIRE(organism.findNode(2)->store.size() == 50);
}

TEST_CASE("starved actuator neuron dies and releases energon", "[ma]") {
  evolab::BarrenWorld world(7, 32);
  float wetX = 0.0f;
  float wetZ = 0.0f;
  REQUIRE(findWetWorldSite(world, evolab::kWorldCellSize, wetX, wetZ));

  evolab::EnergonField energon(1, {});
  evolab::Organism organism =
      evolab::makeMouthActuatorOrganism(1, wetX, wetZ, 1.0f, 200, 0, evolab::kWorldCellSize);
  evolab::SkeletonNode* actuator = organism.findNode(2);
  REQUIRE(actuator != nullptr);
  actuator->store.clear();
  organism.alive = true;

  organism.tickNeuronViability(energon);

  REQUIRE(!actuator->alive);
  REQUIRE(actuator->neuron == evolab::NeuronType::None);
  REQUIRE(organism.findNeuralAxon(1, 2) == nullptr);
  REQUIRE(organism.findNeuralAxon(2, 1) == nullptr);
  REQUIRE(organism.alive);
  REQUIRE(organism.findNode(1)->alive);
  REQUIRE(actuator->store.empty());
}

TEST_CASE("last functional neuron death removes organism and releases fuel", "[ma]") {
  evolab::BarrenWorld world(7, 32);
  float wetX = 0.0f;
  float wetZ = 0.0f;
  REQUIRE(findWetWorldSite(world, evolab::kWorldCellSize, wetX, wetZ));

  evolab::EnergonField energon(1, {});
  evolab::Organism organism =
      evolab::makeMouthActuatorOrganism(1, wetX, wetZ, 1.0f, 4, 0, evolab::kWorldCellSize);
  organism.alive = true;

  organism.tickNeuronViability(energon);
  organism.tickNeuronViability(energon);
  organism.tickNeuronViability(energon);

  REQUIRE(!organism.alive);
  REQUIRE(organism.findNode(1)->store.empty());
  REQUIRE(organism.findNode(2)->store.empty());
}
