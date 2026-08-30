#include <catch2/catch_test_macros.hpp>

#include "sim/CampTopology.hpp"
#include "sim/BarrenWorld.hpp"
#include "sim/CellConstants.hpp"
#include "sim/Energon.hpp"
#include "sim/EnergonString.hpp"
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

  const float foodBearing = 0.55f;
  const float foodDistance = evolab::kWorldCellSize * 2.5f;
  evolab::SkeletonNode* mouth = camper.findNode(evolab::kCampMouthId);
  evolab::SkeletonNode* perceptor = camper.findNode(evolab::kCampPerceptorId);
  REQUIRE(mouth != nullptr);
  REQUIRE(perceptor != nullptr);

  camper.updateKinematics(world, evolab::kWorldCellSize, evolab::kTerrainHeightScale);
  const float foodX = mouth->worldX + std::sin(foodBearing) * foodDistance;
  const float foodZ = mouth->worldZ + std::cos(foodBearing) * foodDistance;
  REQUIRE(world.isWetWorld(foodX, foodZ, evolab::kWorldCellSize));
  energon.injectBlob(evolab::makeCornucopiaBlob(foodX, foodZ, 0x42));

  const float blindHeading = foodBearing + 3.14159265f;
  camper.heading = blindHeading;
  camper.updateKinematics(world, evolab::kWorldCellSize, evolab::kTerrainHeightScale);
  mouth = camper.findNode(evolab::kCampMouthId);
  perceptor = camper.findNode(evolab::kCampPerceptorId);
  REQUIRE(mouth != nullptr);
  REQUIRE(perceptor != nullptr);

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

  const float foodBearing = 0.0f;
  const float startDistance = evolab::kWorldCellSize * 4.0f;
  camper.updateKinematics(world, evolab::kWorldCellSize, evolab::kTerrainHeightScale);
  mouth = camper.findNode(evolab::kCampMouthId);
  const float foodX = mouth->worldX + std::sin(foodBearing) * startDistance;
  const float foodZ = mouth->worldZ + std::cos(foodBearing) * startDistance;
  REQUIRE(world.isWetWorld(foodX, foodZ, evolab::kWorldCellSize));
  energon.injectBlob(evolab::makeCornucopiaBlob(foodX, foodZ, 0x42));
  camper.heading = foodBearing;
  const float halfExtent =
      static_cast<float>(world.heightmap().resolution - 1) * evolab::kWorldCellSize * 0.5f;
  energon.prepareSpatialQueries(evolab::kWorldCellSize, halfExtent, world);

  evolab::runMouthTastePhase(camper, energon, evolab::kWorldCellSize, 1);
  const float farSalience = mouth->mouthTasteSalience;
  REQUIRE(farSalience > 0.0f);

  mouth->worldX += std::sin(foodBearing) * evolab::kWorldCellSize * 2.0f;
  mouth->worldZ += std::cos(foodBearing) * evolab::kWorldCellSize * 2.0f;
  evolab::runMouthTastePhase(camper, energon, evolab::kWorldCellSize, 2);

  REQUIRE(mouth->mouthTasteSalience > farSalience);
  REQUIRE(mouth->mouthTasteGradient > 0.0f);
}
