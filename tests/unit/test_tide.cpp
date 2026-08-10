#include "sim/Tide.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using Catch::Approx;

TEST_CASE("tide is periodic", "[tide]") {
  evolab::Tide tide({0.0f, 8.0f, 3600.0f});
  REQUIRE(tide.waterLevel(0) == Approx(tide.waterLevel(3600)).margin(1e-3f));
}

TEST_CASE("tide stays within amplitude bounds", "[tide]") {
  evolab::Tide tide({0.0f, 8.0f, 3600.0f});
  for (int i = 0; i < 100; ++i) {
    const float w = tide.waterLevel(static_cast<std::uint64_t>(i * 37));
    REQUIRE(w >= tide.minLevel());
    REQUIRE(w <= tide.maxLevel());
  }
}

TEST_CASE("tide changes over time", "[tide]") {
  evolab::Tide tide({0.0f, 8.0f, 360.0f});
  REQUIRE(tide.waterLevel(0) != Approx(tide.waterLevel(90)));
}
