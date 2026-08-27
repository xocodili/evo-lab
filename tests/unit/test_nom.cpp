#include "engine/Camera.hpp"
#include "game/OrganismDrawer.hpp"
#include "game/OrganismInspector.hpp"
#include "game/TerrainMesh.hpp"
#include "sim/BarrenWorld.hpp"
#include "sim/CampTopology.hpp"
#include "sim/CellConstants.hpp"
#include "sim/NeuronSignal.hpp"
#include "sim/CellPopulation.hpp"
#include "sim/Energon.hpp"
#include "sim/EnergonString.hpp"
#include "sim/Organism.hpp"
#include "sim/PerceptorFocus.hpp"
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

TEST_CASE("camp factory builds P-M-C-A chain with axons and split fuel", "[nom]") {
  evolab::Organism organism = evolab::makeCampNomOrganism(1, 0.0f, 0.0f, 1.0f, 100, 0, 1.2f);
  REQUIRE(organism.isCampNom());
  REQUIRE(organism.bodyStorage.size() == 50);
  REQUIRE(organism.nodes.size() == 4);
  REQUIRE(organism.links.size() == 3);
  REQUIRE(organism.neuralAxons.size() == 12);
  REQUIRE(organism.findNode(1)->neuron == evolab::NeuronType::Perceptor);
  REQUIRE(organism.findNode(2)->neuron == evolab::NeuronType::Mouth);
  REQUIRE(organism.findNode(3)->neuron == evolab::NeuronType::Computer);
  REQUIRE(organism.findNode(4)->neuron == evolab::NeuronType::Actuator);
  REQUIRE(organism.findNode(1)->store.size() == 16);
  REQUIRE(organism.findNode(2)->store.size() == 16);
  REQUIRE(organism.findNode(4)->store.size() == 18);
}

TEST_CASE("camp skeleton forms Y-star from computer hub after kinematics", "[nom]") {
  evolab::BarrenWorld world(7, 32);
  evolab::Organism organism =
      evolab::makeCampNomOrganism(1, 0.0f, 0.0f, 1.0f, 100, 0, 1.0f);
  organism.heading = 0.0f;
  organism.updateKinematics(world, evolab::kWorldCellSize, evolab::kTerrainHeightScale);

  const evolab::SkeletonNode* perceptor = organism.findNode(1);
  const evolab::SkeletonNode* mouth = organism.findNode(2);
  const evolab::SkeletonNode* computer = organism.findNode(3);
  const evolab::SkeletonNode* actuator = organism.findNode(4);
  REQUIRE(perceptor != nullptr);
  REQUIRE(mouth != nullptr);
  REQUIRE(computer != nullptr);
  REQUIRE(actuator != nullptr);
  REQUIRE(organism.rootNodeId == evolab::kCampRootNodeId);

  const auto edgeLen = [](const evolab::SkeletonNode& a, const evolab::SkeletonNode& b) {
    return std::hypot(b.worldX - a.worldX, b.worldZ - a.worldZ);
  };
  const float arm = edgeLen(*computer, *perceptor);
  REQUIRE(arm == Catch::Approx(1.0f).margin(0.05f));
  REQUIRE(edgeLen(*computer, *mouth) == Catch::Approx(arm).margin(0.05f));
  REQUIRE(edgeLen(*computer, *actuator) == Catch::Approx(arm).margin(0.05f));

  REQUIRE(perceptor->worldZ > computer->worldZ + 0.5f);
  REQUIRE(actuator->worldX < computer->worldX - 0.3f);
  REQUIRE(mouth->worldX > computer->worldX + 0.3f);
}

TEST_CASE("axon bundle flex bends camp arms under actuator stroke", "[nom][musculature]") {
  evolab::BarrenWorld world(7, 32);
  evolab::Organism organism =
      evolab::makeCampNomOrganism(1, 0.0f, 0.0f, 1.0f, 100, 0, 1.0f);
  organism.heading = 0.0f;
  organism.lastStrokePaid = false;
  organism.lastActuatorNetDrive = 0.0f;
  organism.updateKinematics(world, evolab::kWorldCellSize, evolab::kTerrainHeightScale);

  const evolab::SkeletonNode* actuatorRest = organism.findNode(4);
  REQUIRE(actuatorRest != nullptr);
  const float restX = actuatorRest->worldX;

  organism.lastStrokePaid = true;
  organism.lastActuatorNetDrive = 0.9f;
  organism.lastStrokeBytesPaid = evolab::kActuatorStrokeCostPerTick;
  organism.lastActuatorStrokeFlexBoost =
      static_cast<float>(evolab::kActuatorStrokeCostPerTick) * evolab::kActuatorThrustPerStrokeByte *
      evolab::kActuatorTranslationEta;
  organism.updateKinematics(world, evolab::kWorldCellSize, evolab::kTerrainHeightScale);

  const evolab::SkeletonNode* actuatorFlex = organism.findNode(4);
  REQUIRE(actuatorFlex != nullptr);
  REQUIRE(std::abs(actuatorFlex->worldX - restX) > 0.02f);
}

TEST_CASE("high mouth satiation suppresses actuator stroke via interoception", "[nom]") {
  evolab::BarrenWorld world(7, 32);
  float wetX = 0.0f;
  float wetZ = 0.0f;
  REQUIRE(findWetWorldSite(world, evolab::kWorldCellSize, wetX, wetZ));

  evolab::EnergonField energon(1, {});
  evolab::Organism organism =
      evolab::makeCampNomOrganism(1, wetX, wetZ, 1.0f, 100, 0, evolab::kWorldCellSize);
  organism.alive = true;

  evolab::NeuralAxon* mouthToActuator = organism.findNeuralAxon(2, 4);
  REQUIRE(mouthToActuator != nullptr);
  mouthToActuator->lastReceived.valid = true;
  mouthToActuator->lastReceived.byte = evolab::kMouthInhibitActuatorConfidence;
  mouthToActuator->lastReceived.tick = world.tickCount();

  organism.advectRoot(world, energon, evolab::kWorldCellSize, evolab::kTerrainHeightScale,
                      worldHalfExtent(world, evolab::kWorldCellSize));

  REQUIRE(organism.lastActuatorInhibited);
  REQUIRE(!organism.lastStrokePaid);
  REQUIRE(organism.lastStrokeBytesPaid == 0);
}

TEST_CASE("strong P and M together suppress stroke while eating at food", "[nom]") {
  evolab::BarrenWorld world(7, 32);
  float wetX = 0.0f;
  float wetZ = 0.0f;
  REQUIRE(findWetWorldSite(world, evolab::kWorldCellSize, wetX, wetZ));

  evolab::EnergonField energon(1, {});
  evolab::Organism organism =
      evolab::makeCampNomOrganism(1, wetX, wetZ, 1.0f, 100, 0, evolab::kWorldCellSize);
  organism.alive = true;

  evolab::SkeletonNode* perceptor = organism.findNode(1);
  REQUIRE(perceptor != nullptr);
  perceptor->focusLocked = true;
  perceptor->focusSalience = 0.9f;
  perceptor->focusBearing = 0.0f;
  perceptor->gazeHeading = organism.heading;

  const std::uint64_t tick = world.tickCount();
  evolab::NeuralAxon* pToA = organism.findNeuralAxon(1, 4);
  evolab::NeuralAxon* mToA = organism.findNeuralAxon(2, 4);
  REQUIRE(pToA != nullptr);
  REQUIRE(mToA != nullptr);
  pToA->lastReceived.valid = true;
  pToA->lastReceived.byte = 6;
  pToA->lastReceived.tick = tick;
  mToA->lastReceived.valid = true;
  mToA->lastReceived.byte = 6;
  mToA->lastReceived.tick = tick;

  organism.advectRoot(world, energon, evolab::kWorldCellSize, evolab::kTerrainHeightScale,
                      worldHalfExtent(world, evolab::kWorldCellSize));

  REQUIRE(organism.lastActuatorInhibited);
  REQUIRE(!organism.lastStrokePaid);
}

TEST_CASE("strong P with low M allows full actuator stroke", "[nom]") {
  evolab::BarrenWorld world(7, 32);
  float wetX = 0.0f;
  float wetZ = 0.0f;
  REQUIRE(findWetWorldSite(world, evolab::kWorldCellSize, wetX, wetZ));

  evolab::EnergonField energon(1, {});
  evolab::Organism organism =
      evolab::makeCampNomOrganism(1, wetX, wetZ, 1.0f, 100, 0, evolab::kWorldCellSize);
  organism.alive = true;

  evolab::SkeletonNode* perceptor = organism.findNode(1);
  REQUIRE(perceptor != nullptr);
  perceptor->focusLocked = true;
  perceptor->focusSalience = 0.85f;

  const std::uint64_t tick = world.tickCount();
  evolab::NeuralAxon* pToA = organism.findNeuralAxon(1, 4);
  evolab::NeuralAxon* mToA = organism.findNeuralAxon(2, 4);
  REQUIRE(pToA != nullptr);
  REQUIRE(mToA != nullptr);
  pToA->lastReceived.valid = true;
  pToA->lastReceived.byte = 6;
  pToA->lastReceived.tick = tick;
  mToA->lastReceived.valid = true;
  mToA->lastReceived.byte = 1;
  mToA->lastReceived.tick = tick;

  organism.advectRoot(world, energon, evolab::kWorldCellSize, evolab::kTerrainHeightScale,
                      worldHalfExtent(world, evolab::kWorldCellSize));

  REQUIRE(organism.lastStrokePaid);
  REQUIRE(organism.lastStrokeBytesPaid >= 1);
  REQUIRE(organism.lastActuatorNetDrive > 0.35f);
}

TEST_CASE("chemotaxis slews heading toward off-axis food via P interoception", "[nom]") {
  evolab::BarrenWorld world(7, 32);
  float wetX = 0.0f;
  float wetZ = 0.0f;
  const float senseRadius = evolab::kWorldCellSize * evolab::kPerceptorSenseRadiusFactor;
  REQUIRE(findOpenWaterSite(world, evolab::kWorldCellSize, 0.0f, senseRadius, wetX, wetZ));

  evolab::EnergonField energon(1, {});
  evolab::Organism organism =
      evolab::makeCampNomOrganism(1, wetX, wetZ, 1.0f, 240, 0, evolab::kWorldCellSize);
  organism.alive = true;
  organism.heading = 0.0f;
  organism.updateKinematics(world, evolab::kWorldCellSize, evolab::kTerrainHeightScale);

  evolab::NeuralAxon* mouthToPerceptor = organism.findNeuralAxon(2, 1);
  REQUIRE(mouthToPerceptor != nullptr);

  const float foodBearing = 0.65f;
  const float foodDistance = evolab::kWorldCellSize * 2.5f;
  const float foodX =
      organism.findNode(1)->worldX + std::sin(foodBearing) * foodDistance;
  const float foodZ =
      organism.findNode(1)->worldZ + std::cos(foodBearing) * foodDistance;
  energon.injectBlob(makeWetFoodBlob(foodX, foodZ, 0x42));

  const float half = worldHalfExtent(world, evolab::kWorldCellSize);
  const float headingStart = organism.heading;
  bool sawFoodFocus = false;

  for (int i = 0; i < 25; ++i) {
    const std::uint64_t tick = world.tickCount();
    mouthToPerceptor->lastReceived.valid = true;
    mouthToPerceptor->lastReceived.byte = 1;
    mouthToPerceptor->lastReceived.tick = tick;
    organism.perceive(world, energon, evolab::kWorldCellSize, half, {organism}, tick, 1.0f);
    organism.feed(energon, evolab::kWorldCellSize, tick);
    if (organism.lastPerceptFocusKind == evolab::PerceptFocusKind::Food) {
      sawFoodFocus = true;
    }
    organism.emitPreAdvectSignals(tick);
    organism.advectRoot(world, energon, evolab::kWorldCellSize, evolab::kTerrainHeightScale, half);
    world.tick();
  }

  REQUIRE(sawFoodFocus);
  REQUIRE(organism.heading > headingStart + 0.12f);
}

TEST_CASE("mouth emits fuel confidence before advect", "[nom]") {
  evolab::BarrenWorld world(13, 32);
  float wetX = 0.0f;
  float wetZ = 0.0f;
  REQUIRE(findWetWorldSite(world, evolab::kWorldCellSize, wetX, wetZ));

  evolab::Organism organism =
      evolab::makeCampNomOrganism(1, wetX, wetZ, 1.0f, 100, 0, evolab::kWorldCellSize);
  organism.alive = true;

  evolab::SkeletonNode* mouth = organism.findNode(2);
  REQUIRE(mouth != nullptr);
  mouth->ateThisTick = true;
  organism.emitPreAdvectSignals(world.tickCount());

  const evolab::NeuralAxon* axon = organism.findNeuralAxon(2, 4);
  REQUIRE(axon != nullptr);
  REQUIRE(axon->lastReceived.valid);
  REQUIRE(evolab::isNeuronConfidenceByte(axon->lastReceived.byte));
  REQUIRE(axon->lastReceived.byte == evolab::mouthFuelConfidence(*mouth));
  REQUIRE(organism.findNeuralAxon(2, 1)->lastReceived.byte == axon->lastReceived.byte);
}

TEST_CASE("camp signal phase does not duplicate mouth confidence emit", "[nom]") {
  evolab::BarrenWorld world(13, 32);
  float wetX = 0.0f;
  float wetZ = 0.0f;
  REQUIRE(findWetWorldSite(world, evolab::kWorldCellSize, wetX, wetZ));

  evolab::Organism organism =
      evolab::makeCampNomOrganism(1, wetX, wetZ, 1.0f, 100, 0, evolab::kWorldCellSize);
  organism.alive = true;

  evolab::SkeletonNode* mouth = organism.findNode(2);
  REQUIRE(mouth != nullptr);
  while (mouth->store.size() < evolab::kMouthLocalStoreMaxBytes) {
    mouth->store.push_back(0x01);
  }

  const std::uint64_t tick = world.tickCount();
  organism.emitPreAdvectSignals(tick);

  const evolab::NeuralAxon* toActuator = organism.findNeuralAxon(2, 4);
  const evolab::NeuralAxon* toComputer = organism.findNeuralAxon(2, 3);
  const evolab::NeuralAxon* toPerceptor = organism.findNeuralAxon(2, 1);
  REQUIRE(toActuator != nullptr);
  REQUIRE(toPerceptor != nullptr);
  REQUIRE(toComputer != nullptr);
  REQUIRE(toActuator->lastReceived.valid);
  REQUIRE(toPerceptor->lastReceived.valid);
  REQUIRE(toComputer->lastReceived.valid);
  REQUIRE(evolab::isNeuronConfidenceByte(toActuator->lastReceived.byte));
  REQUIRE(evolab::isNeuronConfidenceByte(toPerceptor->lastReceived.byte));
  const std::uint8_t expected = evolab::mouthFuelConfidence(*mouth);
  REQUIRE(toActuator->lastReceived.byte == expected);
  REQUIRE(toPerceptor->lastReceived.byte == expected);
  REQUIRE(toComputer->lastReceived.byte == expected);

  evolab::EnergonField energon(1, {});
  organism.signal(energon, tick);
  REQUIRE(toActuator->lastReceived.byte == expected);
  REQUIRE(toPerceptor->lastReceived.byte == expected);
  REQUIRE(toComputer->lastReceived.byte == expected);
}

TEST_CASE("nom stroke translates hub and flexes actuator arm", "[nom]") {
  evolab::BarrenWorld world(7, 32);
  float wetX = 0.0f;
  float wetZ = 0.0f;
  REQUIRE(findWetWorldSite(world, evolab::kWorldCellSize, wetX, wetZ));

  evolab::EnergonField energon(1, {});
  evolab::Organism organism =
      evolab::makeCampNomOrganism(1, wetX, wetZ, 1.0f, 100, 0, evolab::kWorldCellSize);
  organism.heading = 0.0f;
  organism.alive = true;
  organism.updateKinematics(world, evolab::kWorldCellSize, evolab::kTerrainHeightScale);

  evolab::SkeletonNode* computer = organism.findNode(3);
  evolab::SkeletonNode* actuator = organism.findNode(4);
  REQUIRE(computer != nullptr);
  REQUIRE(actuator != nullptr);

  const float hubStartZ = computer->worldZ;
  const float actuatorStartX = actuator->worldX;

  organism.advectRoot(world, energon, evolab::kWorldCellSize, evolab::kTerrainHeightScale,
                      worldHalfExtent(world, evolab::kWorldCellSize));

  REQUIRE(organism.lastStrokePaid);
  REQUIRE(computer->worldZ > hubStartZ);
  REQUIRE(std::hypot(actuator->worldX - computer->worldX, actuator->worldZ - computer->worldZ) ==
          Catch::Approx(evolab::kWorldCellSize).margin(0.08f));
  REQUIRE(std::abs(actuator->worldX - actuatorStartX) > 0.001f);
}

TEST_CASE("perceptor scan detects food ahead and emits confidence", "[nom]") {
  evolab::BarrenWorld world(7, 32);
  float wetX = 0.0f;
  float wetZ = 0.0f;
  const float senseRadius = evolab::kWorldCellSize * evolab::kPerceptorSenseRadiusFactor;
  REQUIRE(findOpenWaterSite(world, evolab::kWorldCellSize, 0.0f, senseRadius, wetX, wetZ));

  evolab::EnergonField energon(1, {});
  evolab::Organism organism =
      evolab::makeCampNomOrganism(1, wetX, wetZ, 1.0f, 120, 0, evolab::kWorldCellSize);
  organism.alive = true;
  organism.heading = 0.0f;
  organism.updateKinematics(world, evolab::kWorldCellSize, evolab::kTerrainHeightScale);

  evolab::NeuralAxon* mouthToPerceptor = organism.findNeuralAxon(2, 1);
  REQUIRE(mouthToPerceptor != nullptr);
  mouthToPerceptor->lastReceived.valid = true;
  mouthToPerceptor->lastReceived.byte = 1;
  mouthToPerceptor->lastReceived.tick = 9;

  const float foodZ = organism.findNode(1)->worldZ + evolab::kWorldCellSize * 2.0f;
  energon.injectBlob(makeWetFoodBlob(organism.findNode(1)->worldX, foodZ, 0x42));

  const float half = worldHalfExtent(world, evolab::kWorldCellSize);
  organism.perceive(world, energon, evolab::kWorldCellSize, half, {organism}, 10, 1.0f);

  REQUIRE(organism.lastPerceptScanPaid);
  REQUIRE(organism.lastPerceptFocusKind == evolab::PerceptFocusKind::Food);
  REQUIRE(organism.lastPerceptConfidence >= 5);
  REQUIRE(evolab::isPerceptorConfidenceByte(organism.findNeuralAxon(1, 2)->lastReceived.byte));
  REQUIRE(organism.findNeuralAxon(1, 2)->lastReceived.byte ==
          organism.findNeuralAxon(1, 4)->lastReceived.byte);
}

TEST_CASE("perceptor focuses threat with low avoid confidence", "[nom]") {
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
      evolab::makeCampNomOrganism(1, wetX, wetZ, 1.0f, 120, 0, evolab::kWorldCellSize);
  organism.alive = true;
  organism.heading = 0.0f;
  organism.updateKinematics(world, evolab::kWorldCellSize, evolab::kTerrainHeightScale);
  energon.injectBlob(makeWetFoodBlob(probeX, probeZ, 0x11));

  const float half = worldHalfExtent(world, evolab::kWorldCellSize);
  organism.perceive(world, energon, evolab::kWorldCellSize, half, {organism}, 3, 1.0f);

  REQUIRE(organism.lastPerceptScanPaid);
  REQUIRE(organism.lastPerceptFocusKind == evolab::PerceptFocusKind::Threat);
  REQUIRE(organism.lastPerceptConfidence <= 2);
  REQUIRE(organism.lastPerceptRange > 0.0f);
}

TEST_CASE("mouth refuses food when perceptor signals threat", "[nom]") {
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
      evolab::makeCampNomOrganism(1, wetX, wetZ, 1.0f, 120, 0, evolab::kWorldCellSize);
  organism.alive = true;
  organism.heading = 0.0f;
  organism.updateKinematics(world, evolab::kWorldCellSize, evolab::kTerrainHeightScale);

  evolab::SkeletonNode* mouth = organism.findNode(2);
  REQUIRE(mouth != nullptr);
  energon.injectBlob(makeWetFoodBlob(mouth->worldX, mouth->worldZ, 0x55));
  energon.injectBlob(makeWetFoodBlob(probeX, probeZ, 0x11));

  const float half = worldHalfExtent(world, evolab::kWorldCellSize);
  const std::uint64_t tick = world.tickCount();
  organism.perceive(world, energon, evolab::kWorldCellSize, half, {organism}, tick, 1.0f);
  organism.feed(energon, evolab::kWorldCellSize, tick);

  REQUIRE(organism.lastPerceptFocusKind == evolab::PerceptFocusKind::Threat);
  REQUIRE(organism.lastMouthFeedSuppressed);
  REQUIRE(!mouth->ateThisTick);
}

TEST_CASE("mouth bites when perceptor signals food and contact exists", "[nom]") {
  evolab::BarrenWorld world(7, 32);
  float wetX = 0.0f;
  float wetZ = 0.0f;
  const float senseRadius = evolab::kWorldCellSize * evolab::kPerceptorSenseRadiusFactor;
  REQUIRE(findOpenWaterSite(world, evolab::kWorldCellSize, 0.0f, senseRadius, wetX, wetZ));

  evolab::EnergonField energon(1, {});
  evolab::Organism organism =
      evolab::makeCampNomOrganism(1, wetX, wetZ, 1.0f, 120, 0, evolab::kWorldCellSize);
  organism.alive = true;
  organism.heading = 0.0f;
  organism.updateKinematics(world, evolab::kWorldCellSize, evolab::kTerrainHeightScale);

  evolab::SkeletonNode* mouth = organism.findNode(2);
  REQUIRE(mouth != nullptr);

  evolab::NeuralAxon* mouthToPerceptor = organism.findNeuralAxon(2, 1);
  REQUIRE(mouthToPerceptor != nullptr);
  mouthToPerceptor->lastReceived.valid = true;
  mouthToPerceptor->lastReceived.byte = 1;
  mouthToPerceptor->lastReceived.tick = 9;

  const float foodZ = organism.findNode(1)->worldZ + evolab::kWorldCellSize * 2.0f;
  energon.injectBlob(makeWetFoodBlob(organism.findNode(1)->worldX, foodZ, 0x42));
  energon.injectBlob(makeWetFoodBlob(mouth->worldX, mouth->worldZ, 0x33));

  const float half = worldHalfExtent(world, evolab::kWorldCellSize);
  const std::uint64_t tick = 10;
  organism.perceive(world, energon, evolab::kWorldCellSize, half, {organism}, tick, 1.0f);
  organism.feed(energon, evolab::kWorldCellSize, tick);

  REQUIRE(organism.lastPerceptFocusKind == evolab::PerceptFocusKind::Food);
  REQUIRE(organism.lastMouthBiteDrive > 0.2f);
  REQUIRE(mouth->ateThisTick);
}

TEST_CASE("population tick runs perceive feed then advect", "[nom]") {
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
      evolab::makeCampNomOrganism(1, wetX, wetZ, 1.0f, 200, 0, evolab::kWorldCellSize);
  evolab::SkeletonNode* actuator = organism.findNode(4);
  REQUIRE(actuator != nullptr);
  actuator->store.clear();
  organism.alive = true;

  for (std::uint32_t i = 0; i < evolab::kNeuronBasalGraceTicks; ++i) {
    organism.tickNeuronViability(energon);
    REQUIRE(actuator->alive);
  }
  organism.tickNeuronViability(energon);

  REQUIRE(!actuator->alive);
  REQUIRE(organism.findNeuralAxon(2, 4) == nullptr);
  REQUIRE(organism.alive);
}

TEST_CASE("camp nom signal phase does not flood red feed fragments", "[nom]") {
  evolab::BarrenWorld world(42, 64);
  evolab::EnergonField energon(42, {});
  evolab::CellPopulation population;
  population.seedNoms(world, evolab::kWorldCellSize, evolab::kTerrainHeightScale, 20, 42);
  REQUIRE(population.organisms().size() == 20);

  auto countFragments = [&energon]() {
    int count = 0;
    for (const evolab::EnergonBlob& blob : energon.blobs()) {
      if (blob.origin == evolab::EnergonOrigin::Fragment) {
        ++count;
      }
    }
    return count;
  };

  for (int tick = 0; tick < 120; ++tick) {
    world.tick();
    energon.tick(world, 1.0f, evolab::kWorldCellSize, evolab::kTerrainHeightScale);
    population.tick(world, energon, evolab::kWorldCellSize, evolab::kTerrainHeightScale, 1.0f);
  }

  // Incidental cloaca spit (e.g. one eating overflow) is OK; axonal feed must not flood red trails.
  REQUIRE(countFragments() < 20);
}

TEST_CASE("camp spawn splits fuel between hub and peripheral wallets", "[nom]") {
  evolab::Organism organism =
      evolab::makeCampNomOrganism(1, 0.0f, 0.0f, 1.0f, evolab::kTicksPerStemCellDay * 2, 0,
                              evolab::kWorldCellSize);
  const evolab::SkeletonNode* perceptor = organism.findNode(1);
  const evolab::SkeletonNode* mouth = organism.findNode(2);
  const evolab::SkeletonNode* actuator = organism.findNode(4);
  REQUIRE(perceptor != nullptr);
  REQUIRE(mouth != nullptr);
  REQUIRE(actuator != nullptr);

  const std::size_t total = evolab::kTicksPerStemCellDay * 2;
  REQUIRE(organism.bodyStorage.size() == total / 2);
  const std::size_t peripheral = total - organism.bodyStorage.size();
  REQUIRE(perceptor->store.size() == peripheral / 3);
  REQUIRE(mouth->store.size() == peripheral / 3);
  REQUIRE(actuator->store.size() == peripheral - (peripheral / 3) * 2);
}

TEST_CASE("nom survives 400 ticks intact without feeding", "[nom]") {
  evolab::BarrenWorld world(42, 128);
  evolab::EnergonField energon(42, {});
  evolab::CellPopulation population;
  population.seedNoms(world, evolab::kWorldCellSize, evolab::kTerrainHeightScale, 12, 42);
  REQUIRE(!population.organisms().empty());

  for (int tick = 0; tick < 400; ++tick) {
    world.tick();
    const float sun = 1.0f;
    energon.tick(world, sun, evolab::kWorldCellSize, evolab::kTerrainHeightScale);
    population.tick(world, energon, evolab::kWorldCellSize, evolab::kTerrainHeightScale, sun);
  }

  int intact = 0;
  int degraded = 0;
  for (const evolab::Organism& organism : population.organisms()) {
    if (organism.isCampNom()) {
      ++intact;
    } else if (organism.hasPerceptorNeurons()) {
      ++degraded;
    }
  }
  INFO("intact=" << intact << " degraded=" << degraded
                 << " live=" << population.organisms().size());
  REQUIRE(intact == static_cast<int>(population.organisms().size()));
  REQUIRE(degraded == 0);
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
    REQUIRE(organism.isCampNom());
    const std::string label = evolab::game::formatOrganismArchitectureLabel(organism, 0);
    REQUIRE(label.find("CAMP Nom") != std::string::npos);
  }

  evolab::engine::OrbitCamera camera;
  camera.distance = 140.0f;
  const evolab::engine::Mat4 proj =
      evolab::engine::mat4Perspective(60.0f * 3.1415926535f / 180.0f, 1280.0f / 720.0f, 0.1f, 800.0f);
  const evolab::engine::Mat4 mvp =
      evolab::engine::mat4Multiply(proj, camera.viewMatrix());
  const evolab::game::OrganismDrawBatch batch = evolab::game::buildOrganismDrawBatch(
      population.organisms(), 0.0f, 80.0f, 120.0f, mvp, 1280, 720);
  REQUIRE(!batch.cellVerts.empty());
}
