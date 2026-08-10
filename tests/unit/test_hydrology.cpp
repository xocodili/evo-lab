#include "sim/BarrenWorld.hpp"
#include "sim/Hydrology.hpp"
#include "sim/Tide.hpp"
#include "sim/TideAdvection.hpp"
#include "sim/WorldConstants.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

namespace {

evolab::Heightmap landbridgeBasinMap() {
  evolab::Heightmap map;
  map.resolution = 7;
  map.samples = {
      3.0f, 3.0f, 3.0f, 3.0f, 3.0f, 3.0f, 3.0f,  //
      3.0f, 5.0f, 5.0f, 5.0f, 5.0f, 5.0f, 3.0f,  //
      3.0f, 5.0f, 5.0f, 5.0f, 5.0f, 5.0f, 3.0f,  //
      3.0f, 5.0f, 5.0f, 1.0f, 5.0f, 5.0f, 3.0f,  // floor
      3.0f, 5.0f, 5.0f, 4.0f, 5.0f, 5.0f, 3.0f,  // landbridge saddle
      3.0f, 3.0f, 3.0f, 3.0f, 3.0f, 3.0f, 3.0f,  //
      3.0f, 3.0f, 3.0f, 3.0f, 3.0f, 3.0f, 3.0f,  //
  };
  return map;
}

}  // namespace

TEST_CASE("spill height equals landbridge saddle", "[hydrology]") {
  const evolab::Heightmap map = landbridgeBasinMap();
  const std::vector<float> spill = evolab::computeSpillHeights(map);
  REQUIRE(spill[static_cast<std::size_t>(3 * 7 + 3)] == Catch::Approx(4.0f).margin(0.05f));
}

TEST_CASE("flow direction follows steepest descent on a slope", "[hydrology]") {
  evolab::Heightmap map;
  map.resolution = 5;
  map.samples = {
      0.0f, 0.0f, 0.0f, 0.0f, 0.0f,  //
      1.0f, 1.0f, 1.0f, 1.0f, 1.0f,  //
      2.0f, 2.0f, 2.0f, 2.0f, 2.0f,  //
      3.0f, 3.0f, 3.0f, 3.0f, 3.0f,  //
      4.0f, 4.0f, 4.0f, 4.0f, 4.0f,  //
  };
  const std::vector<float> spill = evolab::computeSpillHeights(map);
  const std::vector<evolab::FlowVector> flow = evolab::computeFlowDirections(map, spill);
  const evolab::FlowVector& mid = flow[static_cast<std::size_t>(2 * 5 + 2)];
  REQUIRE(mid.valid);
  REQUIRE(mid.dz < -0.5f);
}

TEST_CASE("map edge flow includes virtual ocean off terrain", "[hydrology]") {
  evolab::Heightmap map;
  map.resolution = 5;
  map.minHeight = 5.0f;
  map.samples.assign(25, 5.0f);
  const std::vector<float> spill = evolab::computeSpillHeights(map);
  const std::vector<evolab::FlowVector> flow = evolab::computeFlowDirections(map, spill);
  const evolab::FlowVector& leftEdge = flow[static_cast<std::size_t>(2 * 5 + 0)];
  REQUIRE(leftEdge.valid);
  REQUIRE(leftEdge.dx < -0.5f);
}

TEST_CASE("isolated basin stays dry until spill is overtopped", "[hydrology]") {
  const float spill = 4.0f;
  const float floor = 1.0f;
  bool impounded = false;

  REQUIRE(evolab::localWaterDepth(3.0f, spill, impounded, floor) == Catch::Approx(0.0f).margin(1e-4f));
  REQUIRE_FALSE(evolab::shouldImpoundBasin(3.0f, spill));

  REQUIRE(evolab::localWaterDepth(4.5f, spill, impounded, floor) == Catch::Approx(3.5f).margin(1e-4f));
  if (evolab::shouldImpoundBasin(4.5f, spill)) {
    impounded = true;
  }
  REQUIRE(impounded);
}

TEST_CASE("impounded basin stops draining below spill height", "[hydrology]") {
  const float spill = 4.0f;
  const float floor = 1.0f;
  bool impounded = true;

  REQUIRE(evolab::localWaterDepth(2.0f, spill, impounded, floor) == Catch::Approx(3.0f).margin(1e-4f));
  REQUIRE(evolab::localWaterSurface(2.0f, spill, impounded) == Catch::Approx(4.0f).margin(1e-4f));
}

TEST_CASE("barren world impounds basins after tide drops below spill", "[hydrology][barren_world]") {
  evolab::BarrenWorld world(42, 64);
  const float cellSize = evolab::kWorldCellSize;
  const int res = world.heightmap().resolution;

  int basinX = -1;
  int basinZ = -1;
  float basinSpill = 0.0f;
  for (int z = 1; z < res - 1; ++z) {
    for (int x = 1; x < res - 1; ++x) {
      const float spill = world.spillHeightAt(x, z);
      const float terrain = world.heightAt(x, z);
      if (spill > terrain + 1.0f && spill < world.tide().maxLevel() - 0.5f) {
        basinX = x;
        basinZ = z;
        basinSpill = spill;
        break;
      }
    }
    if (basinX >= 0) {
      break;
    }
  }
  REQUIRE(basinX >= 0);

  const float half = static_cast<float>(res - 1) * cellSize * 0.5f;
  const float wx = static_cast<float>(basinX) * cellSize - half;
  const float wz = static_cast<float>(basinZ) * cellSize - half;

  for (std::uint64_t t = 0; t < 5000; ++t) {
    world.tick();
    if (world.waterLevel() >= basinSpill + 0.25f) {
      break;
    }
  }
  REQUIRE(world.waterLevel() >= basinSpill);

  for (std::uint64_t t = 0; t < 5000; ++t) {
    world.tick();
    if (world.waterLevel() < basinSpill - 0.25f) {
      break;
    }
  }
  REQUIRE(world.waterLevel() < basinSpill);

  const float isolatedDepth = world.depthAtWorld(wx, wz, cellSize);
  const float terrain = world.heightAtWorld(wx, wz, cellSize);
  REQUIRE(isolatedDepth == Catch::Approx(basinSpill - terrain).margin(0.25f));
}

TEST_CASE("impounded basin has no tidal advection on ebb", "[hydrology][barren_world]") {
  evolab::BarrenWorld world(42, 64);
  const float cellSize = evolab::kWorldCellSize;
  const int res = world.heightmap().resolution;
  const float half = static_cast<float>(res - 1) * cellSize * 0.5f;

  int basinX = -1;
  int basinZ = -1;
  float basinSpill = 0.0f;
  for (int z = 1; z < res - 1; ++z) {
    for (int x = 1; x < res - 1; ++x) {
      const float spill = world.spillHeightAt(x, z);
      const float terrain = world.heightAt(x, z);
      if (spill > terrain + 1.0f && spill < world.tide().maxLevel() - 0.5f) {
        basinX = x;
        basinZ = z;
        basinSpill = spill;
        break;
      }
    }
    if (basinX >= 0) {
      break;
    }
  }
  REQUIRE(basinX >= 0);

  for (std::uint64_t t = 0; t < 5000; ++t) {
    world.tick();
    if (world.waterLevel() >= basinSpill + 0.25f) {
      break;
    }
  }
  for (std::uint64_t t = 0; t < 5000; ++t) {
    world.tick();
    if (world.waterLevel() < basinSpill - 0.25f) {
      break;
    }
  }
  REQUIRE(world.waterLevel() < basinSpill);

  const float wx = static_cast<float>(basinX) * cellSize - half;
  const float wz = static_cast<float>(basinZ) * cellSize - half;
  REQUIRE(world.isImpoundedAt(wx, wz, cellSize));

  for (std::uint64_t t = 0; t < 200; ++t) {
    world.tick();
    if (world.waterLevelDelta() < -0.005f) {
      break;
    }
  }
  REQUIRE(world.waterLevelDelta() < 0.0f);

  const evolab::AdvectionVelocity velocity =
      evolab::shoreAdvection(world, wx, wz, cellSize, half);
  REQUIRE_FALSE(velocity.active);
}
