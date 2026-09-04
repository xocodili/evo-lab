#include "sim/BarrenWorld.hpp"
#include "sim/CampTopology.hpp"
#include "sim/CloacaSignal.hpp"
#include "sim/CellConstants.hpp"
#include "sim/Energon.hpp"
#include "sim/EnergonString.hpp"
#include "sim/NeuronFuel.hpp"
#include "sim/Organism.hpp"
#include "sim/PerceptorFocus.hpp"
#include "sim/TideAdvection.hpp"
#include "sim/WorldConstants.hpp"

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

bool findOpenWaterSite(const evolab::BarrenWorld& world, float cellSize, float heading,
                       float senseRadius, float& wx, float& wz) {
  const float half = worldHalfExtent(world, cellSize);
  const float fx = std::sin(heading);
  const float fz = std::cos(heading);
  const float samples[] = {0.35f, 0.6f, 0.85f, 1.0f};
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
      bool clear = true;
      for (float fraction : samples) {
        const float probeX = perceptor->worldX + fx * senseRadius * fraction;
        const float probeZ = perceptor->worldZ + fz * senseRadius * fraction;
        float clampedX = probeX;
        float clampedZ = probeZ;
        evolab::clampWorldPosition(clampedX, clampedZ, half, cellSize * 0.25f);
        const bool atBoundary =
            std::abs(clampedX - probeX) > 1.0e-3f || std::abs(clampedZ - probeZ) > 1.0e-3f;
        if (atBoundary || !world.isWetWorld(clampedX, clampedZ, cellSize)) {
          clear = false;
          break;
        }
      }
      if (clear) {
        wx = x;
        wz = z;
        return true;
      }
    }
  }
  return false;
}

evolab::EnergonBlob makeCloacaBlob(evolab::CloacaBand band, float x, float z) {
  const std::uint8_t tag = evolab::cloacaBandTag(band);
  const std::uint32_t cost = evolab::cloacaVentByteCost(band);
  evolab::EnergonBlob blob;
  for (std::uint32_t i = 0; i < cost; ++i) {
    blob.bytes[i] = tag;
  }
  blob.remaining = static_cast<std::uint16_t>(cost);
  blob.initialBytes = static_cast<std::uint8_t>(cost);
  blob.origin = evolab::EnergonOrigin::Cloaca;
  blob.x = x;
  blob.z = z;
  blob.y = 0.0f;
  blob.tailX = x;
  blob.tailZ = z;
  blob.headX = x;
  blob.headZ = z;
  blob.grounded = true;
  blob.onWet = true;
  blob.ttl = 50.0f;
  evolab::energonBlobInitPoint(blob);
  return blob;
}

evolab::Organism makeWetCampNom(float wx, float wz) {
  evolab::Organism organism =
      evolab::makeCampNomOrganism(1, wx, wz, 1.0f, 120, 0, evolab::kWorldCellSize);
  organism.alive = true;
  organism.heading = 0.0f;
  return organism;
}

void primeMateReady(evolab::Organism& organism, std::uint64_t simTick) {
  evolab::assignComputerHubFuel(organism, evolab::kComputerHubStoreMaxBytes, 1);
  organism.createdAtTick = 0;
  evolab::SkeletonNode* mouth = organism.findNode(2);
  evolab::SkeletonNode* perceptor = organism.findNode(1);
  evolab::SkeletonNode* actuator = organism.findNode(4);
  REQUIRE(mouth != nullptr);
  REQUIRE(perceptor != nullptr);
  REQUIRE(actuator != nullptr);
  mouth->store.assign(evolab::kNeuronStoreMaxBytes, 1);
  perceptor->store.assign(evolab::kPerceptorScanCostPerTick + 4, 1);
  actuator->store.assign(evolab::kActuatorStrokeCostPerTick + 4, 1);
  REQUIRE(evolab::campMateReadyPredicate(organism, simTick));
}

}  // namespace

TEST_CASE("perceptor locks mate on red cloaca when mate-ready", "[cloaca][perceptor]") {
  evolab::BarrenWorld world(31, 32);
  float wetX = 0.0f;
  float wetZ = 0.0f;
  const float senseRadius = evolab::kWorldCellSize * evolab::kPerceptorSenseRadiusFactor;
  REQUIRE(findOpenWaterSite(world, evolab::kWorldCellSize, 0.0f, senseRadius, wetX, wetZ));

  evolab::EnergonField energon(1, {});
  evolab::Organism organism = makeWetCampNom(wetX, wetZ);
  organism.updateKinematics(world, evolab::kWorldCellSize, evolab::kTerrainHeightScale);
  const std::uint64_t simTick = evolab::kMateMinAgeTicks + 10;
  primeMateReady(organism, simTick);

  evolab::SkeletonNode* perceptor = organism.findNode(1);
  REQUIRE(perceptor != nullptr);

  const float aheadZ = perceptor->worldZ + evolab::kWorldCellSize * 2.0f;
  energon.injectBlob(makeCloacaBlob(evolab::CloacaBand::Mate, perceptor->worldX, aheadZ));

  const float half = worldHalfExtent(world, evolab::kWorldCellSize);
  organism.perceive(world, energon, evolab::kWorldCellSize, half, {organism}, simTick, 1.0f);
  REQUIRE(organism.lastPerceptFocusKind == evolab::PerceptFocusKind::Mate);
}

TEST_CASE("perceptor ignores red cloaca when not mate-ready", "[cloaca][perceptor]") {
  evolab::BarrenWorld world(31, 32);
  float wetX = 0.0f;
  float wetZ = 0.0f;
  const float senseRadius = evolab::kWorldCellSize * evolab::kPerceptorSenseRadiusFactor;
  REQUIRE(findOpenWaterSite(world, evolab::kWorldCellSize, 0.0f, senseRadius, wetX, wetZ));

  evolab::EnergonField energon(1, {});
  evolab::Organism organism = makeWetCampNom(wetX, wetZ);
  organism.updateKinematics(world, evolab::kWorldCellSize, evolab::kTerrainHeightScale);
  evolab::SkeletonNode* perceptor = organism.findNode(1);
  REQUIRE(perceptor != nullptr);
  perceptor->store.assign(evolab::kPerceptorScanCostPerTick + 2, 1);

  const float aheadZ = perceptor->worldZ + evolab::kWorldCellSize * 2.0f;
  energon.injectBlob(makeCloacaBlob(evolab::CloacaBand::Mate, perceptor->worldX, aheadZ));

  const float half = worldHalfExtent(world, evolab::kWorldCellSize);
  organism.perceive(world, energon, evolab::kWorldCellSize, half, {organism}, 1, 1.0f);
  REQUIRE(organism.lastPerceptFocusKind != evolab::PerceptFocusKind::Mate);
}

TEST_CASE("perceptor reads blue cloaca as food when hungry", "[cloaca][perceptor]") {
  evolab::BarrenWorld world(31, 32);
  float wetX = 0.0f;
  float wetZ = 0.0f;
  const float senseRadius = evolab::kWorldCellSize * evolab::kPerceptorSenseRadiusFactor;
  REQUIRE(findOpenWaterSite(world, evolab::kWorldCellSize, 0.0f, senseRadius, wetX, wetZ));

  evolab::EnergonField energon(1, {});
  evolab::Organism organism = makeWetCampNom(wetX, wetZ);
  organism.updateKinematics(world, evolab::kWorldCellSize, evolab::kTerrainHeightScale);
  evolab::SkeletonNode* mouth = organism.findNode(2);
  evolab::SkeletonNode* perceptor = organism.findNode(1);
  REQUIRE(mouth != nullptr);
  REQUIRE(perceptor != nullptr);
  mouth->store.clear();
  perceptor->store.assign(evolab::kPerceptorScanCostPerTick + 2, 1);

  evolab::NeuralAxon* mouthToPerceptor = organism.findNeuralAxon(2, 1);
  REQUIRE(mouthToPerceptor != nullptr);
  mouthToPerceptor->lastReceived.valid = true;
  mouthToPerceptor->lastReceived.byte = 0;
  mouthToPerceptor->lastReceived.tick = 0;

  const float aheadZ = perceptor->worldZ + evolab::kWorldCellSize * 2.0f;
  energon.injectBlob(makeCloacaBlob(evolab::CloacaBand::Distress, perceptor->worldX, aheadZ));

  const float half = worldHalfExtent(world, evolab::kWorldCellSize);
  organism.perceive(world, energon, evolab::kWorldCellSize, half, {organism}, 1, 1.0f);
  REQUIRE(organism.lastPerceptFocusKind == evolab::PerceptFocusKind::Food);
}

TEST_CASE("perceptor ignores green baseline cloaca trails", "[cloaca][perceptor]") {
  evolab::BarrenWorld world(31, 32);
  float wetX = 0.0f;
  float wetZ = 0.0f;
  const float senseRadius = evolab::kWorldCellSize * evolab::kPerceptorSenseRadiusFactor;
  REQUIRE(findOpenWaterSite(world, evolab::kWorldCellSize, 0.0f, senseRadius, wetX, wetZ));

  evolab::EnergonField energon(1, {});
  evolab::Organism organism = makeWetCampNom(wetX, wetZ);
  organism.updateKinematics(world, evolab::kWorldCellSize, evolab::kTerrainHeightScale);
  evolab::SkeletonNode* perceptor = organism.findNode(1);
  REQUIRE(perceptor != nullptr);
  perceptor->store.assign(evolab::kPerceptorScanCostPerTick + 2, 1);

  const float aheadZ = perceptor->worldZ + evolab::kWorldCellSize * 2.0f;
  energon.injectBlob(makeCloacaBlob(evolab::CloacaBand::Baseline, perceptor->worldX, aheadZ));

  const float half = worldHalfExtent(world, evolab::kWorldCellSize);
  organism.perceive(world, energon, evolab::kWorldCellSize, half, {organism}, 1, 1.0f);
  REQUIRE(organism.lastPerceptFocusKind == evolab::PerceptFocusKind::None);
}
