#include "sim/BarrenWorld.hpp"



#include "sim/Hydrology.hpp"

#include "sim/WorldConstants.hpp"



#include <algorithm>

#include <cmath>



namespace evolab {



BarrenWorld::BarrenWorld(std::uint64_t seed, int resolution, Tide tide)

    : seed_(seed), resolution_(resolution), tide_(std::move(tide)) {

  heightmap_ = generateHeightmap(seed_, resolution_);

  rebuildHydrology();

}



void BarrenWorld::regenerate(std::uint64_t seed) {

  seed_ = seed;

  heightmap_ = generateHeightmap(seed_, resolution_);

  tick_ = 0;

  rebuildHydrology();

}



void BarrenWorld::rebuildHydrology() {

  spillHeights_ = computeSpillHeights(heightmap_);

  flowDirections_ = computeFlowDirections(heightmap_, spillHeights_);

  impounded_.assign(spillHeights_.size(), 0);

}



int BarrenWorld::cellIndex(int x, int z) const {

  return z * resolution_ + x;

}



void BarrenWorld::worldToCell(float worldX, float worldZ, float cellSize, int& x, int& z) const {

  if (resolution_ <= 1 || cellSize <= 0.0f) {

    x = 0;

    z = 0;

    return;

  }

  const float half = static_cast<float>(resolution_ - 1) * cellSize * 0.5f;

  const float gx = (worldX + half) / cellSize;

  const float gz = (worldZ + half) / cellSize;

  x = static_cast<int>(std::floor(gx));

  z = static_cast<int>(std::floor(gz));

  x = std::max(0, std::min(x, resolution_ - 1));

  z = std::max(0, std::min(z, resolution_ - 1));

}



void BarrenWorld::tick() {

  ++tick_;

  const float globalLevel = waterLevel();

  for (int z = 0; z < resolution_; ++z) {

    for (int x = 0; x < resolution_; ++x) {

      const int i = cellIndex(x, z);

      if (shouldImpoundBasin(globalLevel, spillHeights_[static_cast<std::size_t>(i)])) {

        impounded_[static_cast<std::size_t>(i)] = 1;

      }

    }

  }

}



float BarrenWorld::waterLevel() const { return tide_.waterLevel(tick_); }



float BarrenWorld::waterLevelDelta() const { return tide_.waterLevelDelta(tick_); }



bool BarrenWorld::isHydraulicallyConnected(int x, int z) const {

  if (spillHeights_.empty()) {

    return true;

  }

  return waterLevel() >= spillHeights_[static_cast<std::size_t>(cellIndex(x, z))];

}



bool BarrenWorld::isHydraulicallyConnectedAt(float worldX, float worldZ, float cellSize) const {

  int x = 0;

  int z = 0;

  worldToCell(worldX, worldZ, cellSize, x, z);

  return isHydraulicallyConnected(x, z);

}



bool BarrenWorld::isImpoundedAt(float worldX, float worldZ, float cellSize) const {

  int x = 0;

  int z = 0;

  worldToCell(worldX, worldZ, cellSize, x, z);

  const int i = cellIndex(x, z);

  return impounded_[static_cast<std::size_t>(i)] != 0 && !isHydraulicallyConnected(x, z);

}



FlowVector BarrenWorld::flowDirectionAt(int x, int z) const {

  if (flowDirections_.empty()) {

    return {};

  }

  return flowDirections_[static_cast<std::size_t>(cellIndex(x, z))];

}



FlowVector BarrenWorld::flowDirectionAtWorld(float worldX, float worldZ, float cellSize) const {

  int x = 0;

  int z = 0;

  worldToCell(worldX, worldZ, cellSize, x, z);

  return flowDirectionAt(x, z);

}



float BarrenWorld::waterSurfaceAt(int x, int z) const {

  if (spillHeights_.empty()) {

    return waterLevel();

  }

  const int i = cellIndex(x, z);

  const float spill = spillHeights_[static_cast<std::size_t>(i)];

  const float globalLevel = waterLevel();

  return localWaterSurface(globalLevel, spill, impounded_[static_cast<std::size_t>(i)] != 0);

}



float BarrenWorld::effectiveWaterLevelAt(float worldX, float worldZ, float cellSize) const {

  int x = 0;

  int z = 0;

  worldToCell(worldX, worldZ, cellSize, x, z);

  return waterSurfaceAt(x, z);

}



float BarrenWorld::spillHeightAt(int x, int z) const {

  if (spillHeights_.empty()) {

    return 0.0f;

  }

  return spillHeights_[static_cast<std::size_t>(cellIndex(x, z))];

}



float BarrenWorld::spillHeightAtWorld(float worldX, float worldZ, float cellSize) const {

  int x = 0;

  int z = 0;

  worldToCell(worldX, worldZ, cellSize, x, z);

  return spillHeightAt(x, z);

}



float BarrenWorld::heightAt(int x, int z) const { return heightmap_.at(x, z); }



float BarrenWorld::heightAtWorld(float worldX, float worldZ, float cellSize) const {

  int x = 0;

  int z = 0;

  worldToCell(worldX, worldZ, cellSize, x, z);

  return heightmap_.at(x, z);

}



bool BarrenWorld::isWetWorld(float worldX, float worldZ, float cellSize) const {

  return depthAtWorld(worldX, worldZ, cellSize) > 0.0f;

}



float BarrenWorld::depthAtWorld(float worldX, float worldZ, float cellSize) const {

  int x = 0;

  int z = 0;

  worldToCell(worldX, worldZ, cellSize, x, z);

  return depthAt(x, z);

}



float BarrenWorld::depthAt(int x, int z) const {

  const float terrainHeight = heightAt(x, z);

  const float globalLevel = waterLevel();

  if (spillHeights_.empty()) {

    const float depth = globalLevel - terrainHeight;

    return depth > 0.0f ? depth : 0.0f;

  }



  const int i = cellIndex(x, z);

  const float spill = spillHeights_[static_cast<std::size_t>(i)];

  return localWaterDepth(globalLevel, spill, impounded_[static_cast<std::size_t>(i)] != 0,

                         terrainHeight);

}



bool BarrenWorld::isWet(int x, int z) const { return depthAt(x, z) > 0.0f; }



WetnessStats BarrenWorld::wetnessStats() const {

  WetnessStats stats;

  for (int z = 0; z < resolution_; ++z) {

    for (int x = 0; x < resolution_; ++x) {

      if (isWet(x, z)) {

        ++stats.wetCells;

      } else {

        ++stats.dryCells;

      }

    }

  }

  return stats;

}



float BarrenWorld::heightChecksum() const {

  float sum = 0.0f;

  for (float h : heightmap_.samples) {

    sum += h;

  }

  return sum;

}



}  // namespace evolab

