#include "sim/BarrenWorld.hpp"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("barren world regenerates deterministically", "[world]") {
  evolab::BarrenWorld a(123, 64);
  evolab::BarrenWorld b(123, 64);
  REQUIRE(a.heightChecksum() == b.heightChecksum());
}

TEST_CASE("barren world tick advances", "[world]") {
  evolab::BarrenWorld world(1, 16);
  REQUIRE(world.tickCount() == 0);
  world.tick();
  REQUIRE(world.tickCount() == 1);
}

TEST_CASE("regenerate resets tick and changes seed output", "[world]") {
  evolab::BarrenWorld world(1, 32);
  world.tick();
  world.tick();
  const float before = world.heightChecksum();
  world.regenerate(999);
  REQUIRE(world.tickCount() == 0);
  REQUIRE(world.heightChecksum() != before);
}
