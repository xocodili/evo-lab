#include "engine/Camera.hpp"
#include "game/OrganismDrawer.hpp"
#include "game/OrganismInspector.hpp"
#include "game/TerrainMesh.hpp"
#include "sim/BarrenWorld.hpp"
#include "sim/CampTopology.hpp"
#include "sim/CellConstants.hpp"
#include "sim/CampNeuronGating.hpp"
#include "sim/NeuronSignal.hpp"
#include "sim/NeuralAxon.hpp"
#include "sim/CellPopulation.hpp"
#include "sim/Energon.hpp"
#include "sim/EnergonString.hpp"
#include "sim/Organism.hpp"
#include "sim/NeuronFuel.hpp"
#include "sim/NeuronMusculature.hpp"
#include "sim/NeuronStem.hpp"
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
      if (!world.isWetWorld(x, z, cellSize)) {
        continue;
      }
      evolab::Organism probe =
          evolab::makeCampNomOrganism(999, x, z, 1.0f, 120, 0, cellSize);
      probe.heading = heading;
      probe.updateKinematics(world, cellSize, evolab::kTerrainHeightScale);
      const evolab::SkeletonNode* perceptor = probe.findNode(evolab::kCampPerceptorId);
      if (perceptor == nullptr) {
        continue;
      }
      if (blockProbesClear(world, cellSize, half, perceptor->worldX, perceptor->worldZ, heading,
                           senseRadius)) {
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
  REQUIRE(organism.computerHubFuelBytes() == 66);
  REQUIRE(organism.nodes.size() == 4);
  REQUIRE(organism.links.size() == 3);
  REQUIRE(organism.neuralAxons.size() == 12);
  REQUIRE(organism.findNode(1)->neuron == evolab::NeuronType::Perceptor);
  REQUIRE(organism.findNode(2)->neuron == evolab::NeuronType::Mouth);
  REQUIRE(organism.findNode(3)->neuron == evolab::NeuronType::Computer);
  REQUIRE(organism.findNode(4)->neuron == evolab::NeuronType::Actuator);
  REQUIRE(organism.findNode(1)->store.size() == 16);
  REQUIRE(organism.findNode(2)->store.size() == 0);
  REQUIRE(organism.findNode(4)->store.size() == 18);
}

TEST_CASE("camp skeleton forms torpedo chain M-P-C-A after kinematics", "[nom]") {
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
  REQUIRE(organism.rootNodeId == evolab::kCampMouthId);
  REQUIRE(evolab::kCampRootNodeId == evolab::kCampMouthId);
  REQUIRE(evolab::organismHasCampTorpedoChain(organism));

  const auto edgeLen = [](const evolab::SkeletonNode& a, const evolab::SkeletonNode& b) {
    return std::hypot(b.worldX - a.worldX, b.worldZ - a.worldZ);
  };
  const float segment = edgeLen(*actuator, *computer);
  REQUIRE(segment == Catch::Approx(1.0f).margin(0.05f));
  REQUIRE(edgeLen(*computer, *perceptor) == Catch::Approx(segment).margin(0.05f));
  REQUIRE(edgeLen(*perceptor, *mouth) == Catch::Approx(segment).margin(0.05f));

  REQUIRE(mouth->worldZ > perceptor->worldZ + 0.5f);
  REQUIRE(perceptor->worldZ > computer->worldZ + 0.5f);
  REQUIRE(computer->worldZ > actuator->worldZ + 0.5f);
  REQUIRE(evolab::campTorpedoMorphologyLabel(organism) == "MPCA");
  REQUIRE(evolab::campDisplayTypeLabel(organism) == "MPCA");
}

TEST_CASE("axial stroke impulse propagates along camp chain", "[nom][musculature]") {
  evolab::BarrenWorld world(7, 32);
  evolab::Organism organism =
      evolab::makeCampNomOrganism(1, 0.0f, 0.0f, 1.0f, 100, 0, 1.0f);
  organism.heading = 0.0f;
  organism.updateKinematics(world, evolab::kWorldCellSize, evolab::kTerrainHeightScale);

  const evolab::SkeletonNode* computerRest = organism.findNode(3);
  const evolab::SkeletonNode* mouthRest = organism.findNode(2);
  REQUIRE(computerRest != nullptr);
  REQUIRE(mouthRest != nullptr);
  const float hubZ = computerRest->worldZ;
  const float mouthZ = mouthRest->worldZ;

  const float mechanicalThrust =
      static_cast<float>(evolab::kActuatorStrokeCostPerTick) * evolab::kActuatorThrustPerStrokeByte *
      evolab::kActuatorTranslationEta;
  evolab::queueCampStrokeImpulse(organism, evolab::kCampActuatorId, mechanicalThrust,
                                 organism.heading);
  organism.updateKinematics(world, evolab::kWorldCellSize, evolab::kTerrainHeightScale);

  const evolab::SkeletonNode* computerAfter = organism.findNode(3);
  const evolab::SkeletonNode* mouthAfter = organism.findNode(2);
  REQUIRE(computerAfter != nullptr);
  REQUIRE(mouthAfter != nullptr);
  REQUIRE(computerAfter->worldZ > hubZ + mechanicalThrust * 0.5f);
  REQUIRE(mouthAfter->worldZ > computerAfter->worldZ);
  REQUIRE(mouthAfter->worldZ >= mouthZ - 0.02f);
}

TEST_CASE("camp actuator stroke records displacement after kinematics", "[nom][musculature]") {
  evolab::BarrenWorld world(7, 32);
  float wetX = 0.0f;
  float wetZ = 0.0f;
  REQUIRE(findWetWorldSite(world, evolab::kWorldCellSize, wetX, wetZ));

  evolab::EnergonField energon(1, {});
  evolab::Organism organism =
      evolab::makeCampNomOrganism(1, wetX, wetZ, 1.0f, 100, 0, evolab::kWorldCellSize);
  organism.alive = true;
  organism.heading = 0.0f;

  organism.advectRoot(world, energon, evolab::kWorldCellSize, evolab::kTerrainHeightScale,
                      worldHalfExtent(world, evolab::kWorldCellSize));

  REQUIRE(organism.lastStrokePaid);
  REQUIRE(organism.lastMechanicalThrust > 0.0f);
  REQUIRE(organism.lastDisplacement > 0.0f);
}

TEST_CASE("camp stroke moves mouth root along heading not A-arm bearing", "[nom][musculature]") {
  evolab::BarrenWorld world(7, 32);
  float wetX = 0.0f;
  float wetZ = 0.0f;
  REQUIRE(findWetWorldSite(world, evolab::kWorldCellSize, wetX, wetZ));

  evolab::EnergonField energon(1, {});
  evolab::Organism organism =
      evolab::makeCampNomOrganism(1, wetX, wetZ, 1.0f, 100, 0, evolab::kWorldCellSize);
  organism.alive = true;
  organism.heading = 0.7853982f;
  const evolab::SkeletonNode* rootBefore = organism.findNode(organism.rootNodeId);
  REQUIRE(rootBefore != nullptr);
  const float startX = rootBefore->worldX;
  const float startZ = rootBefore->worldZ;

  organism.advectRoot(world, energon, evolab::kWorldCellSize, evolab::kTerrainHeightScale,
                      worldHalfExtent(world, evolab::kWorldCellSize));

  REQUIRE(organism.lastStrokePaid);
  const evolab::SkeletonNode* rootAfter = organism.findNode(organism.rootNodeId);
  REQUIRE(rootAfter != nullptr);
  const float dx = rootAfter->worldX - startX;
  const float dz = rootAfter->worldZ - startZ;
  REQUIRE(std::hypot(dx, dz) > 0.0f);
  const float moveHeading = std::atan2(dx, dz);
  REQUIRE(moveHeading == Catch::Approx(organism.heading).margin(0.2f));
}

TEST_CASE("high mouth chew fill does not latch actuator inhibit", "[nom]") {
  evolab::BarrenWorld world(7, 32);
  float wetX = 0.0f;
  float wetZ = 0.0f;
  REQUIRE(findWetWorldSite(world, evolab::kWorldCellSize, wetX, wetZ));

  evolab::EnergonField energon(1, {});
  evolab::Organism organism =
      evolab::makeCampNomOrganism(1, wetX, wetZ, 1.0f, 100, 0, evolab::kWorldCellSize);
  organism.alive = true;

  evolab::SkeletonNode* mouth = organism.findNode(2);
  evolab::SkeletonNode* actuator = organism.findNode(4);
  REQUIRE(mouth != nullptr);
  REQUIRE(actuator != nullptr);
  mouth->mouthChewFill = evolab::kMouthLocalStoreMaxBytes;
  actuator->store.assign(evolab::kActuatorStrokeCostPerTick * 4, 1);
  organism.actuatorMouthInboundPriorUnit =
      evolab::confidenceToUnit(evolab::kNeuronConfidenceMax);
  organism.actuatorMouthInboundPriorValid = true;

  organism.advectRoot(world, energon, evolab::kWorldCellSize, evolab::kTerrainHeightScale,
                      worldHalfExtent(world, evolab::kWorldCellSize));

  REQUIRE(organism.lastStrokePaid);
  REQUIRE(!organism.lastActuatorInhibited);
}

TEST_CASE("rising mouth satiation gradient trims actuator drive", "[nom]") {
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

  evolab::SkeletonNode* actuator = organism.findNode(4);
  REQUIRE(actuator != nullptr);
  actuator->store.assign(evolab::kActuatorStrokeCostPerTick * 4, 1);
  organism.actuatorMouthInboundPriorUnit = evolab::confidenceToUnit(2);
  organism.actuatorMouthInboundPriorValid = true;

  organism.advectRoot(world, energon, evolab::kWorldCellSize, evolab::kTerrainHeightScale,
                      worldHalfExtent(world, evolab::kWorldCellSize));

  REQUIRE(organism.lastActuatorInteroception.mouthSignalDelta > 0.0f);
  REQUIRE(organism.lastActuatorNetDrive < 1.0f);
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
  evolab::BarrenWorld world(31, 32);
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

  evolab::SkeletonNode* perceptor = organism.findNode(1);
  REQUIRE(perceptor != nullptr);
  perceptor->store.assign(evolab::kPerceptorScanCostPerTick +
                              evolab::kPerceptorTransductionCostPerTick + 16,
                          0x01);

  evolab::NeuralAxon* mouthToPerceptor = organism.findNeuralAxon(2, 1);
  REQUIRE(mouthToPerceptor != nullptr);

  const float foodBearing = 0.65f;
  const float foodDistance = senseRadius * 0.85f;
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
    perceptor->store.assign(std::max<std::size_t>(perceptor->store.size(),
                                                   static_cast<std::size_t>(
                                                       evolab::kPerceptorScanCostPerTick +
                                                       evolab::kPerceptorTransductionCostPerTick + 8)),
                            0x01);
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
  mouth->mouthChewFill = evolab::kMouthLocalStoreMaxBytes;

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
  evolab::BarrenWorld world(31, 32);
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

  evolab::SkeletonNode* perceptor = organism.findNode(1);
  REQUIRE(perceptor != nullptr);
  perceptor->store.assign(evolab::kPerceptorScanCostPerTick +
                              evolab::kPerceptorTransductionCostPerTick + 16,
                          0x01);

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
  REQUIRE(organism.lastPerceptConfidence >= 4);
  REQUIRE(evolab::isPerceptorConfidenceByte(organism.findNeuralAxon(1, 2)->lastReceived.byte));
}

TEST_CASE("perceptor temporal gradient boosts confidence when food salience rises", "[nom]") {
  evolab::BarrenWorld world(31, 32);
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

  evolab::SkeletonNode* perceptor = organism.findNode(1);
  REQUIRE(perceptor != nullptr);
  perceptor->store.assign(evolab::kPerceptorScanCostPerTick + evolab::kPerceptorTransductionCostPerTick + 16,
                          0x01);

  evolab::NeuralAxon* mouthToPerceptor = organism.findNeuralAxon(2, 1);
  REQUIRE(mouthToPerceptor != nullptr);

  const float half = worldHalfExtent(world, evolab::kWorldCellSize);
  const float px = perceptor->worldX;

  mouthToPerceptor->lastReceived.valid = true;
  mouthToPerceptor->lastReceived.byte = 1;
  mouthToPerceptor->lastReceived.tick = 9;

  const float farZ = perceptor->worldZ + senseRadius * 0.95f;
  energon.injectBlob(makeWetFoodBlob(px, farZ, 0x42));
  organism.perceive(world, energon, evolab::kWorldCellSize, half, {organism}, 10, 1.0f);
  REQUIRE(organism.lastPerceptScanPaid);
  REQUIRE(organism.lastPerceptFocusKind == evolab::PerceptFocusKind::Food);
  const std::uint8_t farConfidence = organism.lastPerceptConfidence;

  energon.purgeDepletedBlobs();
  const float nearZ = perceptor->worldZ + senseRadius * 0.35f;
  energon.injectBlob(makeWetFoodBlob(px, nearZ, 0x42));
  mouthToPerceptor->lastReceived.tick = 10;
  organism.perceive(world, energon, evolab::kWorldCellSize, half, {organism}, 11, 1.0f);
  REQUIRE(organism.lastPerceptScanPaid);
  REQUIRE(organism.lastPerceptFocusKind == evolab::PerceptFocusKind::Food);

  INFO("farConfidence=" << static_cast<int>(farConfidence)
                         << " nearConfidence=" << static_cast<int>(organism.lastPerceptConfidence));
  REQUIRE(organism.lastPerceptConfidence > farConfidence);
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

  evolab::SkeletonNode* perceptor = organism.findNode(1);
  REQUIRE(perceptor != nullptr);
  perceptor->store.assign(evolab::kPerceptorScanCostPerTick +
                              evolab::kPerceptorTransductionCostPerTick + 16,
                          0x01);
  probeX = perceptor->worldX;
  for (float ahead = probeDistance; ahead <= probeDistance * 4.0f;
       ahead += evolab::kWorldCellSize * 0.25f) {
    probeZ = perceptor->worldZ + ahead;
    if (!world.isWetWorld(probeX, probeZ, evolab::kWorldCellSize)) {
      break;
    }
  }
  REQUIRE_FALSE(world.isWetWorld(probeX, probeZ, evolab::kWorldCellSize));
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
  evolab::SkeletonNode* perceptor = organism.findNode(1);
  REQUIRE(mouth != nullptr);
  REQUIRE(perceptor != nullptr);
  perceptor->store.assign(evolab::kPerceptorScanCostPerTick +
                              evolab::kPerceptorTransductionCostPerTick + 16,
                          0x01);
  probeX = perceptor->worldX;
  for (float ahead = probeDistance; ahead <= probeDistance * 4.0f;
       ahead += evolab::kWorldCellSize * 0.25f) {
    probeZ = perceptor->worldZ + ahead;
    if (!world.isWetWorld(probeX, probeZ, evolab::kWorldCellSize)) {
      break;
    }
  }
  REQUIRE_FALSE(world.isWetWorld(probeX, probeZ, evolab::kWorldCellSize));
  energon.injectBlob(makeWetFoodBlob(mouth->worldX, mouth->worldZ, 0x55));
  energon.injectBlob(makeWetFoodBlob(probeX, probeZ, 0x11));

  const float half = worldHalfExtent(world, evolab::kWorldCellSize);
  const std::uint64_t tick = world.tickCount();
  organism.perceive(world, energon, evolab::kWorldCellSize, half, {organism}, tick, 1.0f);
  organism.perceive(world, energon, evolab::kWorldCellSize, half, {organism}, tick + 1, 1.0f);
  evolab::NeuralAxon* pToM = organism.findNeuralAxon(1, 2);
  REQUIRE(pToM != nullptr);
  pToM->lastReceived.valid = true;
  pToM->lastReceived.byte = 1;
  pToM->lastReceived.tick = tick + 1;
  organism.feed(energon, evolab::kWorldCellSize, tick + 1);

  REQUIRE(organism.lastPerceptFocusKind == evolab::PerceptFocusKind::Threat);
  REQUIRE(organism.lastMouthFeedSuppressed);
  REQUIRE(!mouth->ateThisTick);
}

TEST_CASE("mouth bites when perceptor signals food and contact exists", "[nom]") {
  evolab::BarrenWorld world(31, 32);
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
  evolab::SkeletonNode* mouth = organism.findNode(2);
  REQUIRE(actuator != nullptr);
  REQUIRE(mouth != nullptr);
  mouth->store.assign(evolab::kStemCellBasalCostPerTick * (evolab::kNeuronBasalGraceTicks + 2),
                      0);
  actuator->store.clear();
  organism.alive = true;

  for (std::uint32_t i = 0; i < evolab::kNeuronBasalGraceTicks; ++i) {
    organism.tickNeuronViability(energon);
    REQUIRE(actuator->alive);
  }
  organism.tickNeuronViability(energon);

  REQUIRE(!actuator->alive);
  const evolab::NeuralAxon* mToA = organism.findNeuralAxon(2, 4);
  REQUIRE(mToA != nullptr);
  REQUIRE(evolab::axonIsDangling(*mToA));
  REQUIRE(mToA->uncappedNodeId == 4);
  REQUIRE(organism.alive);
  REQUIRE(!organism.isCampNom());
  REQUIRE(evolab::organismHasCampTorpedoSkeleton(organism));

  evolab::engine::OrbitCamera camera;
  camera.distance = 140.0f;
  const evolab::engine::Mat4 proj =
      evolab::engine::mat4Perspective(60.0f * 3.1415926535f / 180.0f, 1280.0f / 720.0f, 0.1f, 800.0f);
  const evolab::engine::Mat4 mvp =
      evolab::engine::mat4Multiply(proj, camera.viewMatrix());
  const evolab::game::OrganismDrawBatch degradedBatch = evolab::game::buildOrganismDrawBatch(
      {organism}, 0.0f, 80.0f, 120.0f, mvp, 1280, 720);
  REQUIRE(degradedBatch.cellVerts.size() >= 18);
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
  const std::size_t peripheral = total / 2;
  const std::size_t mouthReserve = peripheral / 3;
  const std::size_t peripheralShare = peripheral / 3;
  const std::size_t peripheralCap = evolab::peripheralStoreCapBytes(organism);

  // Hub-first camp storage: mouth endowment credits hub; P/A wallets cap at peripheralStoreCapBytes.
  // Endowment above wallet caps is not credited (see initPeripheralNodeStore / initComputerHubStore).
  const std::size_t hubCap = evolab::hubStoreCapBytes(organism);
  const std::size_t expectedHub = std::min(total / 2 + mouthReserve, hubCap);
  const std::size_t expectedPeripheral = std::min(peripheralShare, peripheralCap);

  REQUIRE(organism.computerHubFuelBytes() == expectedHub);
  REQUIRE(perceptor->store.size() == expectedPeripheral);
  REQUIRE(mouth->store.size() == 0);
  REQUIRE(actuator->store.size() == expectedPeripheral);
  REQUIRE(organism.totalFuelBytes() == expectedHub + 2 * expectedPeripheral);
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
    REQUIRE(evolab::organismHasCampTorpedoChain(organism));
    REQUIRE(evolab::campDisplayTypeLabel(organism) == "MPCA");
    const std::string label = evolab::game::formatOrganismArchitectureLabel(organism, 0);
    REQUIRE(label.find("Type: CAMP MPCA") != std::string::npos);
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

TEST_CASE("camp peripheral basal uses hub when local wallet empty", "[nom][stemcell][viability]") {
  evolab::EnergonField energon(1, {});
  evolab::Organism camper =
      evolab::makeCampNomOrganism(18, 0.0f, 0.0f, 1.0f, evolab::kTicksPerStemCellDay, 0,
                                  evolab::kWorldCellSize);
  evolab::assignComputerHubFuel(camper, evolab::kComputerHubStoreMaxBytes, 1);

  evolab::SkeletonNode* perceptor = camper.findNode(evolab::kCampPerceptorId);
  evolab::SkeletonNode* mouth = camper.findNode(evolab::kCampMouthId);
  evolab::SkeletonNode* actuator = camper.findNode(evolab::kCampActuatorId);
  REQUIRE(perceptor != nullptr);
  REQUIRE(mouth != nullptr);
  REQUIRE(actuator != nullptr);
  perceptor->store.clear();
  mouth->store.clear();
  actuator->store.clear();

  for (int i = 0; i < static_cast<int>(evolab::kNeuronBasalGraceTicks) + 4; ++i) {
    camper.tickNeuronViability(energon);
  }

  REQUIRE(evolab::organismHasCampNeuronFloor(camper));
  REQUIRE(camper.isCampNom());
}
