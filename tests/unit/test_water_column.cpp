#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "sim/BarrenWorld.hpp"
#include "sim/Tide.hpp"
#include "sim/WaterColumn.hpp"
#include "sim/WorldConstants.hpp"

using Catch::Matchers::WithinAbs;

namespace {

evolab::BarrenWorld makeWorld(int resolution = 64) {
  evolab::Tide tide;
  evolab::TideConfig config = tide.config();
  config.meanLevel = 12.0f;
  config.amplitude = 4.0f;
  config.periodTicks = 3600.0f;
  tide.setConfig(config);
  return evolab::BarrenWorld(42, resolution, tide);
}

bool findWetSample(const evolab::BarrenWorld& world, float cellSize, float heightScale, float& wx,
                   float& wz, evolab::WaterBand minBand) {
  const int res = world.heightmap().resolution;
  const float half = static_cast<float>(res - 1) * cellSize * 0.5f;
  for (int z = 0; z < res; ++z) {
    for (int x = 0; x < res; ++x) {
      if (!world.isWet(x, z)) {
        continue;
      }
      const float sampleX = static_cast<float>(x) * cellSize - half;
      const float sampleZ = static_cast<float>(z) * cellSize - half;
      const evolab::WaterColumn column =
          evolab::sampleWaterColumn(world, sampleX, sampleZ, cellSize, heightScale);
      if (static_cast<int>(column.band) >= static_cast<int>(minBand)) {
        wx = sampleX;
        wz = sampleZ;
        return true;
      }
    }
  }
  return false;
}

}  // namespace

TEST_CASE("water band classification respects depth thresholds", "[water][column]") {
  REQUIRE(evolab::classifyWaterBand(0.0f) == evolab::WaterBand::Dry);
  REQUIRE(evolab::classifyWaterBand(0.5f) == evolab::WaterBand::Benthic);
  REQUIRE(evolab::classifyWaterBand(evolab::kWaterBandBenthicMaxDepth) ==
          evolab::WaterBand::Benthic);
  REQUIRE(evolab::classifyWaterBand(evolab::kWaterBandBenthicMaxDepth + 0.01f) ==
          evolab::WaterBand::Shallow);
  REQUIRE(evolab::classifyWaterBand(evolab::kWaterBandShallowMaxDepth + 0.01f) ==
          evolab::WaterBand::Pelagic);
  REQUIRE(evolab::classifyWaterBand(evolab::kWaterBandPelagicMaxDepth + 0.01f) ==
          evolab::WaterBand::OpenDeep);
}

TEST_CASE("surface noms ride the free surface not the seabed", "[water][column]") {
  const evolab::BarrenWorld world = makeWorld();
  const float cellSize = evolab::kWorldCellSize;
  const float heightScale = evolab::kTerrainHeightScale;

  float wx = 0.0f;
  float wz = 0.0f;
  REQUIRE(findWetSample(world, cellSize, heightScale, wx, wz, evolab::WaterBand::Shallow));

  const evolab::WaterColumn column =
      evolab::sampleWaterColumn(world, wx, wz, cellSize, heightScale);
  REQUIRE(column.wet);
  REQUIRE(column.band != evolab::WaterBand::Dry);
  REQUIRE(column.surfaceY > column.terrainY);

  const float y = evolab::placementY(column, evolab::NomHabitat::Surface);
  REQUIRE_THAT(y, WithinAbs(column.surfaceY + evolab::kNomSurfaceClearance, 0.001f));
  REQUIRE(y > column.terrainY + 0.05f);
}

TEST_CASE("benthic noms snap to seabed when wet", "[water][column]") {
  const evolab::BarrenWorld world = makeWorld();
  const float cellSize = evolab::kWorldCellSize;
  const float heightScale = evolab::kTerrainHeightScale;

  float wx = 0.0f;
  float wz = 0.0f;
  REQUIRE(findWetSample(world, cellSize, heightScale, wx, wz, evolab::WaterBand::Benthic));

  const evolab::WaterColumn column =
      evolab::sampleWaterColumn(world, wx, wz, cellSize, heightScale);
  const float y = evolab::placementY(column, evolab::NomHabitat::Benthic);
  REQUIRE_THAT(y, WithinAbs(column.terrainY + evolab::kNomBenthicClearance, 0.001f));
  REQUIRE(y < column.surfaceY);
}

TEST_CASE("surface placement tracks tide height", "[water][column]") {
  evolab::BarrenWorld world = makeWorld();
  const float cellSize = evolab::kWorldCellSize;
  const float heightScale = evolab::kTerrainHeightScale;

  float wx = 0.0f;
  float wz = 0.0f;
  REQUIRE(findWetSample(world, cellSize, heightScale, wx, wz, evolab::WaterBand::OpenDeep));

  const evolab::WaterColumn low =
      evolab::sampleWaterColumn(world, wx, wz, cellSize, heightScale);
  const float yLow = evolab::placementY(low, evolab::NomHabitat::Surface);

  const int quarterPeriod =
      static_cast<int>(world.tide().config().periodTicks * 0.25f);
  for (int i = 0; i < quarterPeriod; ++i) {
    world.tick();
  }

  const evolab::WaterColumn high =
      evolab::sampleWaterColumn(world, wx, wz, cellSize, heightScale);
  const float yHigh = evolab::placementY(high, evolab::NomHabitat::Surface);

  REQUIRE(yHigh > yLow);
  REQUIRE_THAT(yHigh - yLow, WithinAbs(high.surfaceY - low.surfaceY, 0.001f));
}

TEST_CASE("grounded wet energon uses surface clearance", "[water][column][energon]") {
  const evolab::BarrenWorld world = makeWorld();
  const float cellSize = evolab::kWorldCellSize;
  const float heightScale = evolab::kTerrainHeightScale;

  float wx = 0.0f;
  float wz = 0.0f;
  REQUIRE(findWetSample(world, cellSize, heightScale, wx, wz, evolab::WaterBand::Pelagic));

  const evolab::WaterColumn column =
      evolab::sampleWaterColumn(world, wx, wz, cellSize, heightScale);
  const float landingY = column.surfaceY + evolab::kEnergonSurfaceClearance;
  REQUIRE_THAT(landingY, WithinAbs(column.surfaceY + evolab::kEnergonSurfaceClearance, 0.001f));
  REQUIRE(landingY > column.terrainY);
}
