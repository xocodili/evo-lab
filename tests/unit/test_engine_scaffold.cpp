#include "engine/FixedTimestepClock.hpp"
#include "engine/Viewport.hpp"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("letterbox preserves design aspect on wide drawable", "[engine]") {
  const evolab::engine::ViewportLayout layout =
      evolab::engine::computeLetterbox(1920, 1080, 1280, 720);
  REQUIRE(layout.contentW == 1920);
  REQUIRE(layout.contentH == 1080);
  REQUIRE(layout.offsetX == 0);
  REQUIRE(layout.offsetY == 0);
}

TEST_CASE("screen maps into design coordinates at letterbox origin", "[engine]") {
  const evolab::engine::ViewportLayout layout =
      evolab::engine::computeLetterbox(1600, 900, 1280, 720);
  int designX = -1;
  int designY = -1;
  evolab::engine::mapScreenToDesign(layout.offsetX, layout.offsetY, layout, designX, designY);
  REQUIRE(designX == 0);
  REQUIRE(designY == 0);
}

TEST_CASE("fixed timestep clock accumulates sim steps", "[engine]") {
  evolab::engine::FixedTimestepClock clock(60.0f);
  REQUIRE(clock.advance(0.0f) == 0);
  REQUIRE(clock.advance(1.0f / 60.0f) == 1);
  REQUIRE(clock.advance(1.0f / 60.0f) == 1);
}
