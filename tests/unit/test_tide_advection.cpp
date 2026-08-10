#include "sim/BarrenWorld.hpp"
#include "sim/Tide.hpp"
#include "sim/TideAdvection.hpp"
#include "sim/WorldConstants.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>

namespace {

std::uint64_t tickAtStrongestDelta(const evolab::BarrenWorld& world, bool wantFlood) {
  const float period = world.tide().config().periodTicks;
  std::uint64_t bestTick = 0;
  float bestDelta = wantFlood ? -1.0e9f : 1.0e9f;
  for (std::uint64_t t = 0; t < static_cast<std::uint64_t>(period); ++t) {
    const float delta = evolab::Tide(world.tide().config()).waterLevelDelta(t);
    if (wantFlood) {
      if (delta > bestDelta) {
        bestDelta = delta;
        bestTick = t;
      }
    } else if (delta < bestDelta) {
      bestDelta = delta;
      bestTick = t;
    }
  }
  return bestTick;
}

float worldHalfExtent(const evolab::BarrenWorld& world, float cellSize) {
  const int res = world.heightmap().resolution;
  return static_cast<float>(res - 1) * cellSize * 0.5f;
}

}  // namespace

TEST_CASE("water level delta is zero at high and low slack", "[tide][advection]") {
  evolab::Tide tide;
  const float period = tide.config().periodTicks;
  const std::uint64_t highSlack = static_cast<std::uint64_t>(period * 0.25f);
  const std::uint64_t lowSlack = static_cast<std::uint64_t>(period * 0.75f);
  REQUIRE(tide.waterLevelDelta(highSlack) == Catch::Approx(0.0f).margin(1e-4f));
  REQUIRE(tide.waterLevelDelta(lowSlack) == Catch::Approx(0.0f).margin(1e-4f));
}

TEST_CASE("ebb advection moves wet cells toward deeper water", "[tide][advection]") {
  evolab::BarrenWorld world(123, 64);
  const float cellSize = evolab::kWorldCellSize;
  const float half = worldHalfExtent(world, cellSize);

  for (std::uint64_t i = 0; i < tickAtStrongestDelta(world, false); ++i) {
    world.tick();
  }
  REQUIRE(world.waterLevelDelta() < 0.0f);

  bool found = false;
  const int res = world.heightmap().resolution;
  for (int z = 1; z < res - 1; ++z) {
    for (int x = 1; x < res - 1; ++x) {
      const float wx = static_cast<float>(x) * cellSize - half;
      const float wz = static_cast<float>(z) * cellSize - half;
      if (!world.isWetWorld(wx, wz, cellSize)) {
        continue;
      }
      if (world.isImpoundedAt(wx, wz, cellSize)) {
        continue;
      }
      if (!world.isHydraulicallyConnectedAt(wx, wz, cellSize)) {
        continue;
      }
      const evolab::AdvectionVelocity velocity =
          evolab::shoreAdvection(world, wx, wz, cellSize, half);
      if (!velocity.active) {
        continue;
      }
      const float step = cellSize * 0.45f;
      const float depth0 = world.depthAtWorld(wx, wz, cellSize);
      const float depthAhead = world.depthAtWorld(wx + velocity.vx * step, wz + velocity.vz * step, cellSize);
      REQUIRE(depthAhead >= depth0 - 0.08f);
      found = true;
      break;
    }
    if (found) {
      break;
    }
  }
  REQUIRE(found);
}

TEST_CASE("flood advection moves deep wet cells toward shallower water", "[tide][advection]") {
  evolab::BarrenWorld world(456, 64);
  const float cellSize = evolab::kWorldCellSize;
  const float half = worldHalfExtent(world, cellSize);

  for (std::uint64_t i = 0; i < tickAtStrongestDelta(world, true); ++i) {
    world.tick();
  }
  REQUIRE(world.waterLevelDelta() > 0.0f);

  bool found = false;
  const int res = world.heightmap().resolution;
  for (int z = 2; z < res - 2; ++z) {
    for (int x = 2; x < res - 2; ++x) {
      const float wx = static_cast<float>(x) * cellSize - half;
      const float wz = static_cast<float>(z) * cellSize - half;
      if (!world.isHydraulicallyConnectedAt(wx, wz, cellSize)) {
        continue;
      }
      const float depth = world.depthAtWorld(wx, wz, cellSize);
      if (depth <= 1.5f) {
        continue;
      }
      const evolab::AdvectionVelocity velocity =
          evolab::shoreAdvection(world, wx, wz, cellSize, half);
      if (!velocity.active) {
        continue;
      }
      const float step = cellSize * 0.45f;
      const float depthAhead = world.depthAtWorld(wx + velocity.vx * step, wz + velocity.vz * step, cellSize);
      REQUIRE(depthAhead <= depth + 0.08f);
      found = true;
      break;
    }
    if (found) {
      break;
    }
  }
  REQUIRE(found);
}

TEST_CASE("ebb advection aligns with precomputed drainage direction", "[tide][advection]") {
  evolab::BarrenWorld world(77, 64);
  const float cellSize = evolab::kWorldCellSize;
  const float half = worldHalfExtent(world, cellSize);

  bool found = false;
  const int res = world.heightmap().resolution;
  for (std::uint64_t tick = 0; tick < 1800 && !found; ++tick) {
    if (tick > 0) {
      world.tick();
    }
    if (world.waterLevelDelta() >= 0.0f) {
      continue;
    }
    for (int z = 1; z < res - 1; ++z) {
      for (int x = 1; x < res - 1; ++x) {
        const float wx = static_cast<float>(x) * cellSize - half;
        const float wz = static_cast<float>(z) * cellSize - half;
        if (!world.isWetWorld(wx, wz, cellSize)) {
          continue;
        }
        if (!world.isHydraulicallyConnectedAt(wx, wz, cellSize)) {
          continue;
        }
        const evolab::FlowVector drainage = world.flowDirectionAtWorld(wx, wz, cellSize);
        if (!drainage.valid) {
          continue;
        }
        const evolab::AdvectionVelocity velocity =
            evolab::shoreAdvection(world, wx, wz, cellSize, half);
        if (!velocity.active) {
          continue;
        }
        const float alignment = velocity.vx * drainage.dx + velocity.vz * drainage.dz;
        if (alignment > 0.05f) {
          found = true;
          break;
        }
      }
    }
  }
  REQUIRE(found);
}

TEST_CASE("flood advection opposes drainage direction when connected", "[tide][advection]") {
  evolab::BarrenWorld world(88, 64);
  const float cellSize = evolab::kWorldCellSize;
  const float half = worldHalfExtent(world, cellSize);

  bool found = false;
  const int res = world.heightmap().resolution;
  for (std::uint64_t tick = 0; tick < 1800 && !found; ++tick) {
    if (tick > 0) {
      world.tick();
    }
    if (world.waterLevelDelta() <= 0.0f) {
      continue;
    }
    for (int z = 1; z < res - 1; ++z) {
      for (int x = 1; x < res - 1; ++x) {
        const float wx = static_cast<float>(x) * cellSize - half;
        const float wz = static_cast<float>(z) * cellSize - half;
        if (!world.isWetWorld(wx, wz, cellSize)) {
          continue;
        }
        if (!world.isHydraulicallyConnectedAt(wx, wz, cellSize)) {
          continue;
        }
        const evolab::FlowVector drainage = world.flowDirectionAtWorld(wx, wz, cellSize);
        if (!drainage.valid) {
          continue;
        }
        const evolab::AdvectionVelocity velocity =
            evolab::shoreAdvection(world, wx, wz, cellSize, half);
        if (!velocity.active) {
          continue;
        }
        const float alignment = velocity.vx * drainage.dx + velocity.vz * drainage.dz;
        if (alignment < -0.05f) {
          found = true;
          break;
        }
      }
    }
  }
  REQUIRE(found);
}

TEST_CASE("applyShoreAdvection blocks velocity into map boundary", "[tide][advection]") {
  const float half = 10.0f;
  const float margin = 0.3f;
  float x = -half + margin;
  float z = 0.0f;
  evolab::AdvectionVelocity velocity;
  velocity.vx = -0.5f;
  velocity.vz = 0.2f;
  velocity.active = true;
  evolab::applyShoreAdvection(x, z, velocity, half, margin);
  REQUIRE(x > -half + margin - 1.0e-3f);
}
