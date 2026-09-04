#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "sim/CampTopology.hpp"
#include "sim/BarrenWorld.hpp"
#include "sim/CellConstants.hpp"
#include "sim/Energon.hpp"
#include "sim/EnergonString.hpp"
#include "sim/EnergonTasteSensory.hpp"
#include "sim/OrganismMouth.hpp"
#include "sim/Organism.hpp"
#include "sim/PerceptorFocus.hpp"
#include "sim/WorldConstants.hpp"

#include <cmath>

namespace {

bool pointInFocusCone(float originX, float originZ, float heading, float halfAngle, float range,
                      float px, float pz) {
  const float dx = px - originX;
  const float dz = pz - originZ;
  const float distSq = dx * dx + dz * dz;
  if (distSq < 1.0e-8f || distSq > range * range) {
    return false;
  }
  const float bearing = std::atan2(dx, dz);
  float rel = bearing - heading;
  constexpr float kTwoPi = 6.2831853f;
  while (rel > 3.14159265f) {
    rel -= kTwoPi;
  }
  while (rel < -3.14159265f) {
    rel += kTwoPi;
  }
  return std::abs(rel) <= halfAngle;
}

bool findWetSite(const evolab::BarrenWorld& world, float cellSize, float& wx, float& wz) {
  const int res = world.heightmap().resolution;
  const float half = static_cast<float>(res - 1) * cellSize * 0.5f;
  for (float x = -half; x <= half; x += cellSize * 0.5f) {
    for (float z = -half; z <= half; z += cellSize * 0.5f) {
      if (world.isWetWorld(x, z, cellSize)) {
        wx = x;
        wz = z;
        return true;
      }
    }
  }
  return false;
}

bool placeWetFoodAheadOfMouth(const evolab::BarrenWorld& world, float cellSize,
                              const evolab::SkeletonNode& mouth, float bearing, float distance,
                              float& foodX, float& foodZ) {
  foodX = mouth.worldX + std::sin(bearing) * distance;
  foodZ = mouth.worldZ + std::cos(bearing) * distance;
  return world.isWetWorld(foodX, foodZ, cellSize);
}

bool findWetFoodNearMouth(const evolab::BarrenWorld& world, float cellSize,
                          const evolab::SkeletonNode& mouth, float preferredBearing,
                          float distance, float& foodX, float& foodZ) {
  if (placeWetFoodAheadOfMouth(world, cellSize, mouth, preferredBearing, distance, foodX, foodZ)) {
    return true;
  }
  constexpr int kBearingSteps = 16;
  for (int step = 1; step < kBearingSteps; ++step) {
    const float delta = static_cast<float>(step) * 6.2831853f / static_cast<float>(kBearingSteps);
    if (placeWetFoodAheadOfMouth(world, cellSize, mouth, preferredBearing + delta, distance,
                                 foodX, foodZ)) {
      return true;
    }
    if (placeWetFoodAheadOfMouth(world, cellSize, mouth, preferredBearing - delta, distance,
                                 foodX, foodZ)) {
      return true;
    }
  }
  return false;
}

}  // namespace

TEST_CASE("mouth taste ignores dry land energon", "[camp][mouth][taste]") {
  evolab::BarrenWorld world(17, 17);
  evolab::EnergonField energon(9, {});
  evolab::Organism camper =
      evolab::makeCampNomOrganism(1, 0.0f, 0.0f, 1.0f, evolab::kTicksPerStemCellDay, 0,
                                  evolab::kWorldCellSize);

  float dryX = 0.0f;
  float dryZ = 0.0f;
  const int res = world.heightmap().resolution;
  const float half = static_cast<float>(res - 1) * evolab::kWorldCellSize * 0.5f;
  bool foundDry = false;
  for (float x = -half; x <= half && !foundDry; x += evolab::kWorldCellSize * 0.5f) {
    for (float z = -half; z <= half; ++z) {
      if (!world.isWetWorld(x, z, evolab::kWorldCellSize)) {
        dryX = x;
        dryZ = z;
        foundDry = true;
        break;
      }
    }
  }
  REQUIRE(foundDry);

  evolab::SkeletonNode* mouth = camper.findNode(evolab::kCampMouthId);
  REQUIRE(mouth != nullptr);
  mouth->worldX = dryX;
  mouth->worldZ = dryZ;
  camper.updateKinematics(world, evolab::kWorldCellSize, evolab::kTerrainHeightScale);
  mouth = camper.findNode(evolab::kCampMouthId);

  evolab::EnergonBlob dryBlob;
  dryBlob.remaining = 8;
  dryBlob.initialBytes = 8;
  dryBlob.origin = evolab::EnergonOrigin::Sunfall;
  dryBlob.x = dryX;
  dryBlob.z = dryZ;
  dryBlob.grounded = true;
  dryBlob.onWet = false;
  evolab::energonBlobInitPoint(dryBlob);
  energon.injectBlob(dryBlob);

  const float halfExtent = half;
  energon.prepareSpatialQueries(evolab::kWorldCellSize, halfExtent, world);
  evolab::runMouthTastePhase(camper, energon, evolab::kWorldCellSize, 1);

  REQUIRE_FALSE(mouth->mouthTasteSampleValid);
  REQUIRE(mouth->mouthTasteSalience <= 1.0e-4f);
}

TEST_CASE("mouth taste coarse sensory layer breaks symmetric food ring paralysis",
          "[camp][mouth][taste]") {
  evolab::BarrenWorld world(42, 64);
  evolab::EnergonField energon(9, {});
  float wx = 0.0f;
  float wz = 0.0f;
  REQUIRE(findWetSite(world, evolab::kWorldCellSize, wx, wz));
  evolab::Organism camper =
      evolab::makeCampNomOrganism(1, wx, wz, 1.0f, evolab::kTicksPerStemCellDay, 0,
                                  evolab::kWorldCellSize);

  evolab::SkeletonNode* mouth = camper.findNode(evolab::kCampMouthId);
  REQUIRE(mouth != nullptr);
  camper.updateKinematics(world, evolab::kWorldCellSize, evolab::kTerrainHeightScale);
  mouth = camper.findNode(evolab::kCampMouthId);

  const float ringRadius = evolab::kWorldCellSize * 3.0f;
  constexpr int kBlobCount = 8;
  int wetBlobs = 0;
  for (int i = 0; i < kBlobCount; ++i) {
    const float bearing = static_cast<float>(i) * 6.2831853f / static_cast<float>(kBlobCount);
    const float foodX = mouth->worldX + std::sin(bearing) * ringRadius;
    const float foodZ = mouth->worldZ + std::cos(bearing) * ringRadius;
    if (!world.isWetWorld(foodX, foodZ, evolab::kWorldCellSize)) {
      continue;
    }
    energon.injectBlob(evolab::makeCornucopiaBlob(foodX, foodZ, 0x42));
    ++wetBlobs;
  }
  REQUIRE(wetBlobs >= 3);

  const float halfExtent =
      static_cast<float>(world.heightmap().resolution - 1) * evolab::kWorldCellSize * 0.5f;
  energon.prepareSpatialQueries(evolab::kWorldCellSize, halfExtent, world);
  evolab::runMouthTastePhase(camper, energon, evolab::kWorldCellSize, 1);

  REQUIRE(mouth->mouthTasteSampleValid);
  REQUIRE(mouth->mouthTasteSalience > evolab::kOrganismCampReflexMinValence);
  REQUIRE_FALSE(mouth->mouthTasteSymmetricAmbiguity);
  REQUIRE(std::abs(mouth->mouthTasteBearing) > 0.05f);
}

TEST_CASE("mouth taste senses food outside perceptor gaze cone", "[camp][mouth][taste]") {
  evolab::BarrenWorld world(42, 64);
  evolab::EnergonField energon(9, {});
  float wx = 0.0f;
  float wz = 0.0f;
  REQUIRE(findWetSite(world, evolab::kWorldCellSize, wx, wz));
  evolab::Organism camper =
      evolab::makeCampNomOrganism(1, wx, wz, 1.0f, evolab::kTicksPerStemCellDay, 0,
                                  evolab::kWorldCellSize);
  camper.heading = 0.0f;

  // Lateral food — outside P forward cone (±45°) but inside omnidirectional M taste.
  const float foodBearing = 1.15f;
  const float foodDistance = evolab::kWorldCellSize * 2.5f;

  camper.updateKinematics(world, evolab::kWorldCellSize, evolab::kTerrainHeightScale);
  evolab::SkeletonNode* mouth = camper.findNode(evolab::kCampMouthId);
  evolab::SkeletonNode* perceptor = camper.findNode(evolab::kCampPerceptorId);
  REQUIRE(mouth != nullptr);
  REQUIRE(perceptor != nullptr);

  float foodX = 0.0f;
  float foodZ = 0.0f;
  REQUIRE(findWetFoodNearMouth(world, evolab::kWorldCellSize, *mouth, foodBearing, foodDistance,
                               foodX, foodZ));
  energon.injectBlob(evolab::makeCornucopiaBlob(foodX, foodZ, 0x42));

  const float senseRadius = evolab::kWorldCellSize * evolab::kPerceptorSenseRadiusFactor;
  REQUIRE_FALSE(pointInFocusCone(perceptor->worldX, perceptor->worldZ, camper.heading,
                                 evolab::kPerceptorFocusHalfAngle, senseRadius, foodX, foodZ));

  const float halfExtent =
      static_cast<float>(world.heightmap().resolution - 1) * evolab::kWorldCellSize * 0.5f;
  energon.prepareSpatialQueries(evolab::kWorldCellSize, halfExtent, world);
  evolab::runMouthTastePhase(camper, energon, evolab::kWorldCellSize, 1);

  REQUIRE(mouth->mouthTasteSampleValid);
  REQUIRE(mouth->mouthTasteSalience > evolab::kOrganismCampReflexMinValence);
  REQUIRE(std::abs(mouth->mouthTasteBearing) > 0.35f);
}

TEST_CASE("mouth taste temporal gradient turns positive when approaching food",
          "[camp][mouth][taste]") {
  evolab::BarrenWorld world(42, 64);
  evolab::EnergonField energon(9, {});
  float wx = 0.0f;
  float wz = 0.0f;
  REQUIRE(findWetSite(world, evolab::kWorldCellSize, wx, wz));
  evolab::Organism camper =
      evolab::makeCampNomOrganism(1, wx, wz, 1.0f, evolab::kTicksPerStemCellDay, 0,
                                  evolab::kWorldCellSize);
  evolab::SkeletonNode* mouth = camper.findNode(evolab::kCampMouthId);
  REQUIRE(mouth != nullptr);

  const float foodBearing = 0.35f;
  const float startDistance = evolab::kWorldCellSize * 2.5f;
  camper.updateKinematics(world, evolab::kWorldCellSize, evolab::kTerrainHeightScale);
  mouth = camper.findNode(evolab::kCampMouthId);
  float foodX = 0.0f;
  float foodZ = 0.0f;
  REQUIRE(findWetFoodNearMouth(world, evolab::kWorldCellSize, *mouth, foodBearing, startDistance,
                               foodX, foodZ));
  energon.injectBlob(evolab::makeCornucopiaBlob(foodX, foodZ, 0x42));
  camper.heading = foodBearing;
  const float halfExtent =
      static_cast<float>(world.heightmap().resolution - 1) * evolab::kWorldCellSize * 0.5f;
  energon.prepareSpatialQueries(evolab::kWorldCellSize, halfExtent, world);

  evolab::runMouthTastePhase(camper, energon, evolab::kWorldCellSize, 1);
  const float farSalience = mouth->mouthTasteSalience;
  REQUIRE(farSalience > 0.0f);

  mouth->worldX += std::sin(foodBearing) * evolab::kWorldCellSize * 1.5f;
  mouth->worldZ += std::cos(foodBearing) * evolab::kWorldCellSize * 1.5f;
  evolab::runMouthTastePhase(camper, energon, evolab::kWorldCellSize, 2);

  REQUIRE(mouth->mouthTasteSalience > farSalience);
  REQUIRE(mouth->mouthTasteGradient > 0.0f);
}

evolab::EnergonBlob makeDistressCloacaBlob(float x, float z, std::uint16_t bytes = 1) {
  evolab::EnergonBlob blob;
  blob.origin = evolab::EnergonOrigin::Cloaca;
  blob.bytes[0] = evolab::kCloacaTagDistress;
  blob.remaining = bytes;
  blob.initialBytes = static_cast<std::uint8_t>(std::min<std::uint16_t>(bytes, 8));
  blob.x = x;
  blob.z = z;
  blob.grounded = true;
  blob.onWet = true;
  evolab::energonBlobInitPoint(blob);
  return blob;
}

TEST_CASE("mouth taste grid includes low-weight distress blue when alone", "[camp][mouth][taste]") {
  evolab::BarrenWorld world(42, 64);
  evolab::EnergonField energon(9, {});
  float wx = 0.0f;
  float wz = 0.0f;
  REQUIRE(findWetSite(world, evolab::kWorldCellSize, wx, wz));
  evolab::Organism camper =
      evolab::makeCampNomOrganism(1, wx, wz, 1.0f, evolab::kTicksPerStemCellDay, 0,
                                  evolab::kWorldCellSize);
  evolab::SkeletonNode* mouth = camper.findNode(evolab::kCampMouthId);
  REQUIRE(mouth != nullptr);
  camper.updateKinematics(world, evolab::kWorldCellSize, evolab::kTerrainHeightScale);
  mouth = camper.findNode(evolab::kCampMouthId);

  const float bearing = 0.35f;
  const float dist = evolab::kWorldCellSize * 2.0f;
  const float blueX = mouth->worldX + std::sin(bearing) * dist;
  const float blueZ = mouth->worldZ + std::cos(bearing) * dist;
  REQUIRE(world.isWetWorld(blueX, blueZ, evolab::kWorldCellSize));
  energon.injectBlob(makeDistressCloacaBlob(blueX, blueZ));

  const float halfExtent =
      static_cast<float>(world.heightmap().resolution - 1) * evolab::kWorldCellSize * 0.5f;
  energon.prepareSpatialQueries(evolab::kWorldCellSize, halfExtent, world);
  evolab::runMouthTastePhase(camper, energon, evolab::kWorldCellSize, 1);

  REQUIRE(mouth->mouthTasteSampleValid);
  REQUIRE(mouth->mouthTasteSalience > evolab::kOrganismCampReflexMinValence);
  const float peakDx = blueX - mouth->worldX;
  const float peakDz = blueZ - mouth->worldZ;
  REQUIRE(peakDx * std::sin(bearing) + peakDz * std::cos(bearing) > 0.0f);
}

TEST_CASE("mouth taste grid prefers sunfall over distress blue in same neighborhood",
          "[camp][mouth][taste]") {
  evolab::BarrenWorld world(42, 64);
  evolab::EnergonField energon(9, {});
  float wx = 0.0f;
  float wz = 0.0f;
  REQUIRE(findWetSite(world, evolab::kWorldCellSize, wx, wz));
  evolab::Organism camper =
      evolab::makeCampNomOrganism(1, wx, wz, 1.0f, evolab::kTicksPerStemCellDay, 0,
                                  evolab::kWorldCellSize);
  evolab::SkeletonNode* mouth = camper.findNode(evolab::kCampMouthId);
  REQUIRE(mouth != nullptr);
  camper.updateKinematics(world, evolab::kWorldCellSize, evolab::kTerrainHeightScale);
  mouth = camper.findNode(evolab::kCampMouthId);

  const float sunBearing = 0.55f;
  const float dist = evolab::kWorldCellSize * 2.5f;
  const float sunX = mouth->worldX + std::sin(sunBearing) * dist;
  const float sunZ = mouth->worldZ + std::cos(sunBearing) * dist;
  REQUIRE(world.isWetWorld(sunX, sunZ, evolab::kWorldCellSize));

  float blueBearing = sunBearing;
  float blueX = sunX;
  float blueZ = sunZ;
  bool foundBlueSite = false;
  for (int i = 1; i <= 16; ++i) {
    blueBearing = sunBearing + static_cast<float>(i) * 0.45f;
    blueX = mouth->worldX + std::sin(blueBearing) * dist;
    blueZ = mouth->worldZ + std::cos(blueBearing) * dist;
    if (!world.isWetWorld(blueX, blueZ, evolab::kWorldCellSize)) {
      continue;
    }
    const float sepSq = (blueX - sunX) * (blueX - sunX) + (blueZ - sunZ) * (blueZ - sunZ);
    if (sepSq > evolab::kWorldCellSize * evolab::kWorldCellSize) {
      foundBlueSite = true;
      break;
    }
  }
  REQUIRE(foundBlueSite);

  energon.injectBlob(evolab::makeCornucopiaBlob(sunX, sunZ, 0x42));
  energon.injectBlob(makeDistressCloacaBlob(blueX, blueZ, 8));

  const float halfExtent =
      static_cast<float>(world.heightmap().resolution - 1) * evolab::kWorldCellSize * 0.5f;
  energon.prepareSpatialQueries(evolab::kWorldCellSize, halfExtent, world);
  const float tasteRadius = evolab::kWorldCellSize * evolab::kMouthTasteRadiusFactor;
  const evolab::EnergonTasteSensoryPeak peak =
      energon.queryTasteSensoryPeak(mouth->worldX, mouth->worldZ, tasteRadius);
  REQUIRE(peak.valid);
  const float distSunSq = (peak.worldX - sunX) * (peak.worldX - sunX) +
                          (peak.worldZ - sunZ) * (peak.worldZ - sunZ);
  const float distBlueSq = (peak.worldX - blueX) * (peak.worldX - blueX) +
                           (peak.worldZ - blueZ) * (peak.worldZ - blueZ);
  REQUIRE(distSunSq < distBlueSq);
}

TEST_CASE("mouth taste latch holds bearing across ticks", "[camp][mouth][taste][latch]") {
  evolab::BarrenWorld world(42, 64);
  evolab::EnergonField energon(11, {});
  float wx = 0.0f;
  float wz = 0.0f;
  REQUIRE(findWetSite(world, evolab::kWorldCellSize, wx, wz));
  evolab::Organism camper =
      evolab::makeCampNomOrganism(1, wx, wz, 1.0f, evolab::kTicksPerStemCellDay, 0,
                                  evolab::kWorldCellSize);
  camper.heading = 0.0f;
  camper.updateKinematics(world, evolab::kWorldCellSize, evolab::kTerrainHeightScale);
  evolab::SkeletonNode* mouth = camper.findNode(evolab::kCampMouthId);
  REQUIRE(mouth != nullptr);

  const float foodBearing = 0.35f;
  const float foodDist = evolab::kWorldCellSize * 2.5f;
  float foodX = 0.0f;
  float foodZ = 0.0f;
  REQUIRE(findWetFoodNearMouth(world, evolab::kWorldCellSize, *mouth, foodBearing, foodDist, foodX,
                               foodZ));
  energon.injectBlob(evolab::makeCornucopiaBlob(foodX, foodZ, 0x42));

  const float halfExtent =
      static_cast<float>(world.heightmap().resolution - 1) * evolab::kWorldCellSize * 0.5f;
  energon.prepareSpatialQueries(evolab::kWorldCellSize, halfExtent, world);
  evolab::runMouthTastePhase(camper, energon, evolab::kWorldCellSize, 1);
  REQUIRE(mouth->mouthTasteLatchValid);
  const float latchX = mouth->mouthTasteLatchWorldX;
  const float latchZ = mouth->mouthTasteLatchWorldZ;
  const float bearingTick1 = mouth->mouthTasteBearing;

  energon.prepareSpatialQueries(evolab::kWorldCellSize, halfExtent, world);
  evolab::runMouthTastePhase(camper, energon, evolab::kWorldCellSize, 2);
  REQUIRE(mouth->mouthTasteLatchValid);
  REQUIRE(mouth->mouthTasteLatchWorldX == Catch::Approx(latchX).margin(1e-3f));
  REQUIRE(mouth->mouthTasteLatchWorldZ == Catch::Approx(latchZ).margin(1e-3f));
  REQUIRE(mouth->mouthTasteBearing == Catch::Approx(bearingTick1).margin(1e-3f));
}

TEST_CASE("mouth taste latch switch costs bytes", "[camp][mouth][taste][latch]") {
  evolab::BarrenWorld world(42, 64);
  evolab::EnergonField energon(12, {});
  float wx = 0.0f;
  float wz = 0.0f;
  REQUIRE(findWetSite(world, evolab::kWorldCellSize, wx, wz));
  evolab::Organism camper =
      evolab::makeCampNomOrganism(1, wx, wz, 1.0f, evolab::kTicksPerStemCellDay, 0,
                                  evolab::kWorldCellSize);
  camper.heading = 0.0f;
  camper.updateKinematics(world, evolab::kWorldCellSize, evolab::kTerrainHeightScale);
  evolab::SkeletonNode* mouth = camper.findNode(evolab::kCampMouthId);
  REQUIRE(mouth != nullptr);

  const float foodBearing = 0.35f;
  const float farDist = evolab::kWorldCellSize * 2.8f;
  const float nearDist = evolab::kWorldCellSize * 1.8f;
  float farX = 0.0f;
  float farZ = 0.0f;
  float nearX = 0.0f;
  float nearZ = 0.0f;
  REQUIRE(findWetFoodNearMouth(world, evolab::kWorldCellSize, *mouth, foodBearing, farDist, farX,
                               farZ));
  REQUIRE(findWetFoodNearMouth(world, evolab::kWorldCellSize, *mouth, foodBearing, nearDist, nearX,
                               nearZ));
  energon.injectBlob(evolab::makeCornucopiaBlob(farX, farZ, 0x22));

  const float halfExtent =
      static_cast<float>(world.heightmap().resolution - 1) * evolab::kWorldCellSize * 0.5f;
  energon.prepareSpatialQueries(evolab::kWorldCellSize, halfExtent, world);
  evolab::runMouthTastePhase(camper, energon, evolab::kWorldCellSize, 1);
  REQUIRE(mouth->mouthTasteLatchValid);
  const float firstLatchX = mouth->mouthTasteLatchWorldX;
  const float firstLatchPeakBytes = mouth->mouthTasteLatchPeakBytes;
  mouth->store.assign(4, 0x01);
  const std::size_t fuelBeforeSwitch = mouth->store.size();

  energon.injectBlob(evolab::makeCornucopiaBlob(nearX, nearZ, 0x64));
  energon.prepareSpatialQueries(evolab::kWorldCellSize, halfExtent, world);
  evolab::runMouthTastePhase(camper, energon, evolab::kWorldCellSize, 2);
  REQUIRE(mouth->mouthTasteLatchValid);
  const bool latchRetargeted =
      mouth->mouthTasteLatchWorldX != Catch::Approx(firstLatchX).margin(1e-3f) ||
      mouth->mouthTasteLatchPeakBytes > firstLatchPeakBytes + 1.0e-3f;
  REQUIRE(latchRetargeted);
  REQUIRE(mouth->store.size() + evolab::kMouthTasteLatchSwitchCostBytes == fuelBeforeSwitch);
}
