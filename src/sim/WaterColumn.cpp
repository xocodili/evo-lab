#include "sim/WaterColumn.hpp"

#include "sim/BarrenWorld.hpp"

namespace evolab {

WaterBand classifyWaterBand(float depth) {
  if (depth <= 0.0f) {
    return WaterBand::Dry;
  }
  if (depth <= kWaterBandBenthicMaxDepth) {
    return WaterBand::Benthic;
  }
  if (depth <= kWaterBandShallowMaxDepth) {
    return WaterBand::Shallow;
  }
  if (depth <= kWaterBandPelagicMaxDepth) {
    return WaterBand::Pelagic;
  }
  return WaterBand::OpenDeep;
}

WaterColumn sampleWaterColumn(const BarrenWorld& world, float wx, float wz, float cellSize,
                              float heightScale) {
  WaterColumn column;
  const float terrainHeight = world.heightAtWorld(wx, wz, cellSize);
  column.terrainY = terrainHeight * heightScale;
  const float waterLevel = world.effectiveWaterLevelAt(wx, wz, cellSize);
  column.surfaceY = waterLevel * heightScale;
  column.depth = world.depthAtWorld(wx, wz, cellSize);
  column.wet = column.depth > 0.0f;
  column.band = classifyWaterBand(column.depth);
  return column;
}

float placementY(const WaterColumn& column, NomHabitat habitat) {
  if (!column.wet) {
    return column.terrainY + kNomDryClearance;
  }

  switch (habitat) {
    case NomHabitat::Benthic:
      return column.terrainY + kNomBenthicClearance;
    case NomHabitat::Shallow:
      return column.terrainY +
             (column.surfaceY - column.terrainY) * kShallowPlacementBlend + kNomBenthicClearance;
    case NomHabitat::Pelagic:
      return column.terrainY + (column.surfaceY - column.terrainY) * kPelagicPlacementBlend;
    case NomHabitat::Surface:
    default:
      return column.surfaceY + kNomSurfaceClearance;
  }
}

}  // namespace evolab
