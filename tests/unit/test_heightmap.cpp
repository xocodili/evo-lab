#include "sim/BarrenWorld.hpp"
#include "sim/Heightmap.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using Catch::Approx;

TEST_CASE("heightmap is deterministic for seed", "[heightmap]") {
  const auto a = evolab::generateHeightmap(42, 64);
  const auto b = evolab::generateHeightmap(42, 64);
  REQUIRE(a.samples == b.samples);
}

TEST_CASE("heightmap varies by seed", "[heightmap]") {
  const auto a = evolab::generateHeightmap(42, 64);
  const auto b = evolab::generateHeightmap(43, 64);
  REQUIRE(a.samples != b.samples);
}

TEST_CASE("heightmap is not flat", "[heightmap]") {
  const auto map = evolab::generateHeightmap(99, 64);
  REQUIRE(map.variance() > 1.0f);
}

TEST_CASE("heightmap has land and bathymetry", "[heightmap]") {
  const auto map = evolab::generateHeightmap(7, 128);
  REQUIRE(map.countAbove(map.seaLevel) > 0);
  REQUIRE(map.countBelow(map.seaLevel) > 0);
}

TEST_CASE("height samples stay within configured bounds", "[heightmap]") {
  const auto map = evolab::generateHeightmap(1, 32);
  for (float h : map.samples) {
    REQUIRE(h >= map.minHeight);
    REQUIRE(h <= map.maxHeight);
  }
}
