#include "sim/BarrenWorld.hpp"
#include "sim/DayCycle.hpp"
#include "sim/Energon.hpp"
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

TEST_CASE("energon spawns during daylight", "[energon]") {
  evolab::BarrenWorld world(42, 64);
  evolab::EnergonField field(42, {});

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

TEST_CASE("barren world height at world coordinates clamps to grid", "[energon]") {
  evolab::BarrenWorld world(1, 16);
  const float h0 = world.heightAtWorld(0.0f, 0.0f, evolab::kWorldCellSize);
  const float hFar = world.heightAtWorld(9999.0f, -9999.0f, evolab::kWorldCellSize);
  REQUIRE(h0 == Catch::Approx(world.heightAt(7, 7)).margin(1e-3f));
  REQUIRE(hFar == Catch::Approx(world.heightAt(15, 0)).margin(1e-3f));
}
