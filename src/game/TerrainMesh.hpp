#pragma once

#include "sim/BarrenWorld.hpp"
#include "sim/Heightmap.hpp"

#include "sim/WorldConstants.hpp"

#include <cstdint>
#include <vector>

namespace evolab::game {

struct TerrainVertex {
  float x, y, z;
  float r, g, b;
};

struct TerrainMesh {
  std::vector<TerrainVertex> vertices;
  std::vector<std::uint32_t> indices;
  float minX = 0.0f;
  float maxX = 0.0f;
  float minZ = 0.0f;
  float maxZ = 0.0f;
};

TerrainMesh buildTerrainMesh(const Heightmap& map, float cellSize = 1.0f);
void updateTerrainColors(TerrainMesh& mesh, const Heightmap& map, float waterLevel, int resolution);
void updateTerrainColors(TerrainMesh& mesh, const BarrenWorld& world, float cellSize);

}  // namespace evolab::game
