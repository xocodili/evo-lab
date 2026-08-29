#include "sim/BarrenWorld.hpp"
#include "sim/CellConstants.hpp"
#include "sim/CloacaSignal.hpp"
#include "sim/DayCycle.hpp"
#include "sim/Energon.hpp"
#include "sim/EnergonRain.hpp"
#include "sim/EnergonString.hpp"
#include "sim/NeuronStem.hpp"
#include "sim/Organism.hpp"
#include "sim/WorldConstants.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

TEST_CASE("day cycle sun intensity is zero at night", "[energon]") {
  evolab::DayCycle day(360.0f);
  const float noon = day.sunIntensity(90);
  const float night = day.sunIntensity(270);
  REQUIRE(noon > 0.5f);
  REQUIRE(night == Catch::Approx(0.0f).margin(1e-3f));
}

TEST_CASE("rain cycle budget is f(population) times entropy", "[energon]") {
  const float perNom = evolab::rainCycleFieldBytesPerNom();
  REQUIRE(perNom ==
          Catch::Approx(static_cast<float>(evolab::kVisualDayCyclePeriodTicks) *
                        static_cast<float>(evolab::kCampNomRainCycleBurnPerTick) /
                        static_cast<float>(evolab::kBiteNetYieldBytes)));

  REQUIRE(evolab::rainCycleFieldBytesForPopulation(0) == Catch::Approx(0.0f));
  REQUIRE(evolab::rainCycleFieldBytesForPopulation(60) ==
          Catch::Approx(60.0f * perNom * evolab::kEnergonRainEntropy));

  REQUIRE(evolab::expectedSunfallBlobsPerTick(60, 1.0f) >
          evolab::expectedSunfallBlobsPerTick(20, 1.0f));
  REQUIRE(evolab::expectedSunfallBlobsPerTick(10, 0.0f) == Catch::Approx(0.0f));
}

TEST_CASE("energon spawns during daylight", "[energon]") {
  evolab::BarrenWorld world(42, 64);
  evolab::EnergonConfig config;
  config.populationScaledRain = false;
  evolab::EnergonField field(42, config);

  for (int i = 0; i < 200; ++i) {
    world.tick();
    field.tick(world, 0.0f, evolab::kWorldCellSize, evolab::kTerrainHeightScale);
  }
  REQUIRE(field.activeCount() == 0);

  for (int i = 0; i < 400; ++i) {
    world.tick();
    field.tick(world, 1.0f, evolab::kWorldCellSize, evolab::kTerrainHeightScale);
  }
  REQUIRE(field.activeCount() > 0);
}

TEST_CASE("population scaled sunfall increases with live organism count", "[energon]") {
  evolab::BarrenWorld world(42, 64);
  evolab::EnergonConfig config;
  config.populationScaledRain = true;
  config.spawnRateMax = 0.0f;
  config.maxBlobs = 8000;
  evolab::EnergonField field(42, config);

  for (int i = 0; i < 300; ++i) {
    world.tick();
    field.tick(world, 1.0f, evolab::kWorldCellSize, evolab::kTerrainHeightScale, 0);
  }
  const int emptyPopCount = field.activeCount();

  for (int i = 0; i < 300; ++i) {
    world.tick();
    field.tick(world, 1.0f, evolab::kWorldCellSize, evolab::kTerrainHeightScale, 60);
  }
  const int fullPopCount = field.activeCount();

  REQUIRE(fullPopCount > emptyPopCount + 50);
}

TEST_CASE("dry land energon decays faster than wet", "[energon]") {
  evolab::BarrenWorld world(7, 32);
  evolab::EnergonConfig config;
  config.spawnRateMax = 0.0f;
  config.ttlWetSeconds = 40.0f;
  config.ttlDrySeconds = 4.0f;
  evolab::EnergonField field(7, config);

  evolab::EnergonBlob dryBlob;
  dryBlob.id = 1;
  dryBlob.data = 0xFFFFFF;
  dryBlob.remaining = 3;
  dryBlob.initialBytes = 3;
  dryBlob.x = 0.0f;
  dryBlob.z = 0.0f;
  dryBlob.y = 1.0f;
  dryBlob.grounded = true;
  dryBlob.onWet = false;
  dryBlob.ttl = config.ttlDrySeconds;

  evolab::EnergonBlob wetBlob = dryBlob;
  wetBlob.id = 2;
  wetBlob.onWet = true;
  wetBlob.ttl = config.ttlWetSeconds;

  field.injectBlob(dryBlob);
  field.injectBlob(wetBlob);

  for (int i = 0; i < 120; ++i) {
    world.tick();
    field.tick(world, 0.0f, evolab::kWorldCellSize, evolab::kTerrainHeightScale);
  }

  float dryTtl = 0.0f;
  float wetTtl = 0.0f;
  for (const evolab::EnergonBlob& blob : field.blobs()) {
    if (blob.id == 1) {
      dryTtl = blob.ttl;
    } else if (blob.id == 2) {
      wetTtl = blob.ttl;
    }
  }

  REQUIRE(dryTtl < wetTtl);
}

TEST_CASE("injectBlob enforces maxBlobs for cloaca vents", "[energon]") {
  evolab::EnergonConfig config;
  config.maxBlobs = 32;
  config.spawnRateMax = 0.0f;
  evolab::EnergonField field(99, config);

  for (int i = 0; i < 40; ++i) {
    evolab::EnergonBlob blob;
    blob.id = static_cast<std::uint32_t>(i + 1);
    blob.data = 0x01010101;
    blob.remaining = 1;
    blob.initialBytes = 1;
    blob.origin = evolab::EnergonOrigin::Cloaca;
    blob.x = static_cast<float>(i);
    blob.z = 0.0f;
    blob.y = 1.0f;
    blob.grounded = true;
    blob.onWet = true;
    blob.ttl = 10.0f;
    field.injectBlob(blob);
  }

  REQUIRE(field.activeCount() == config.maxBlobs);
}

TEST_CASE("corpse release packs up to eight bytes per blob", "[energon]") {
  evolab::EnergonField field(1, {});
  evolab::SkeletonNode node;
  node.worldX = 1.0f;
  node.worldZ = 2.0f;
  node.worldY = 0.5f;
  std::vector<std::uint8_t> storage(10, 0xAB);

  evolab::releaseFuelAtNode(node, field, storage, evolab::EnergonOrigin::Fragment, 1.0f);

  REQUIRE(storage.empty());
  REQUIRE(field.activeCount() == 2);
  REQUIRE(field.blobs()[0].remaining == 8);
  REQUIRE(field.blobs()[1].remaining == 2);
  REQUIRE(field.blobs()[0].origin == evolab::EnergonOrigin::Fragment);
}

TEST_CASE("cloaca band wet TTL blue faster than green faster than red", "[energon]") {
  evolab::EnergonConfig config;
  config.ttlWetSeconds = 50.0f;

  evolab::EnergonBlob distress;
  distress.origin = evolab::EnergonOrigin::Cloaca;
  distress.data = evolab::kCloacaTagDistress;
  distress.remaining = 1;

  evolab::EnergonBlob baseline = distress;
  baseline.data = evolab::kCloacaTagBaseline;

  evolab::EnergonBlob mate = distress;
  mate.data = evolab::kCloacaTagMate;

  const float distressTtl = evolab::energonWetTtlSeconds(distress, config);
  const float baselineTtl = evolab::energonWetTtlSeconds(baseline, config);
  const float mateTtl = evolab::energonWetTtlSeconds(mate, config);

  REQUIRE(distressTtl < baselineTtl);
  REQUIRE(baselineTtl < mateTtl);
  REQUIRE(distressTtl == Catch::Approx(50.0f * evolab::kEnergonTtlDistressScale));
  REQUIRE(mateTtl == Catch::Approx(50.0f * evolab::kEnergonTtlMateScale));
}

TEST_CASE("airborne sunfall retains TTL until landing", "[energon]") {
  evolab::BarrenWorld world(42, 64);
  evolab::EnergonConfig config;
  config.populationScaledRain = false;
  config.spawnRateMax = 4.0f;
  config.maxBlobs = 64;
  evolab::EnergonField field(42, config);

  for (int i = 0; i < 40; ++i) {
    world.tick();
    field.tick(world, 1.0f, evolab::kWorldCellSize, evolab::kTerrainHeightScale);
  }
  REQUIRE(field.activeCount() > 0);

  bool sawAirborne = false;
  float airborneTtl = 0.0f;
  for (const evolab::EnergonBlob& blob : field.blobs()) {
    if (blob.origin == evolab::EnergonOrigin::Sunfall && !blob.grounded) {
      sawAirborne = true;
      airborneTtl = blob.ttl;
      break;
    }
  }
  REQUIRE(sawAirborne);
  REQUIRE(airborneTtl == Catch::Approx(evolab::kEnergonAirborneTtlSeconds));

  for (int i = 0; i < 5; ++i) {
    world.tick();
    field.tick(world, 1.0f, evolab::kWorldCellSize, evolab::kTerrainHeightScale);
  }

  for (const evolab::EnergonBlob& blob : field.blobs()) {
    if (blob.origin == evolab::EnergonOrigin::Sunfall && !blob.grounded) {
      REQUIRE(blob.ttl == Catch::Approx(airborneTtl));
    }
  }
}

TEST_CASE("fragment TTL is shorter than sunfall", "[energon]") {
  evolab::EnergonConfig config;
  config.ttlWetSeconds = 40.0f;

  evolab::EnergonBlob fragment;
  fragment.origin = evolab::EnergonOrigin::Fragment;
  fragment.remaining = 4;

  evolab::EnergonBlob sunfall;
  sunfall.origin = evolab::EnergonOrigin::Sunfall;
  sunfall.remaining = 4;

  REQUIRE(evolab::energonWetTtlSeconds(fragment, config) <
          evolab::energonWetTtlSeconds(sunfall, config));
}

TEST_CASE("barren world height at world coordinates clamps to grid", "[energon]") {
  evolab::BarrenWorld world(1, 16);
  const float h0 = world.heightAtWorld(0.0f, 0.0f, evolab::kWorldCellSize);
  const float hFar = world.heightAtWorld(9999.0f, -9999.0f, evolab::kWorldCellSize);
  REQUIRE(h0 == Catch::Approx(world.heightAt(7, 7)).margin(1e-3f));
  REQUIRE(hFar == Catch::Approx(world.heightAt(15, 0)).margin(1e-3f));
}
