#include "sim/BarrenWorld.hpp"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("depth is zero on dry cells", "[wetness]") {
  evolab::BarrenWorld world(42, 32);
  bool foundDry = false;
  for (int z = 0; z < 32; ++z) {
    for (int x = 0; x < 32; ++x) {
      if (!world.isWet(x, z)) {
        REQUIRE(world.depthAt(x, z) == 0.0f);
        foundDry = true;
      }
    }
  }
  REQUIRE(foundDry);
}

TEST_CASE("wet cells have positive depth", "[wetness]") {
  evolab::BarrenWorld world(42, 32);
  bool foundWet = false;
  for (int z = 0; z < 32; ++z) {
    for (int x = 0; x < 32; ++x) {
      if (world.isWet(x, z)) {
        REQUIRE(world.depthAt(x, z) > 0.0f);
        foundWet = true;
      }
    }
  }
  REQUIRE(foundWet);
}

TEST_CASE("tide changes wet cell counts", "[wetness]") {
  evolab::Tide tide({0.0f, 50.0f, 360.0f});
  evolab::BarrenWorld world(7, 64, tide);
  const auto atMeanTide = world.wetnessStats();
  for (int i = 0; i < 90; ++i) {
    world.tick();  // quarter period → high tide (water ≈ +50)
  }
  const auto atHighTide = world.wetnessStats();
  REQUIRE(atMeanTide.wetCells != atHighTide.wetCells);
}
