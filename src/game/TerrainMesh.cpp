#include "game/TerrainMesh.hpp"

#include <algorithm>
#include <cmath>

namespace evolab::game {

namespace {

void heightColor(float height, float waterLevel, float& r, float& g, float& b) {
  if (height < waterLevel) {
    const float depth = waterLevel - height;
    const float t = std::min(depth / 20.0f, 1.0f);
    r = 0.05f + 0.1f * (1.0f - t);
    g = 0.25f + 0.35f * (1.0f - t);
    b = 0.45f + 0.4f * (1.0f - t);
  } else {
    const float above = height - waterLevel;
    const float t = std::min(above / 25.0f, 1.0f);
    r = 0.35f + 0.25f * t;
    g = 0.28f + 0.2f * t;
    b = 0.12f + 0.08f * (1.0f - t);
  }
}

}  // namespace

TerrainMesh buildTerrainMesh(const Heightmap& map, float cellSize) {
  TerrainMesh mesh;
  const int res = map.resolution;
  if (res <= 1) {
    return mesh;
  }

  const float half = static_cast<float>(res - 1) * cellSize * 0.5f;
  mesh.minX = -half;
  mesh.maxX = half;
  mesh.minZ = -half;
  mesh.maxZ = half;

  mesh.vertices.resize(static_cast<std::size_t>(res * res));
  for (int z = 0; z < res; ++z) {
    for (int x = 0; x < res; ++x) {
      const float h = map.at(x, z);
      const float wx = static_cast<float>(x) * cellSize - half;
      const float wz = static_cast<float>(z) * cellSize - half;
      auto& v = mesh.vertices[static_cast<std::size_t>(z * res + x)];
      v.x = wx;
      v.y = h * kTerrainHeightScale;
      v.z = wz;
      heightColor(h, map.seaLevel, v.r, v.g, v.b);
    }
  }

  mesh.indices.reserve(static_cast<std::size_t>((res - 1) * (res - 1) * 6));
  for (int z = 0; z < res - 1; ++z) {
    for (int x = 0; x < res - 1; ++x) {
      const std::uint32_t i0 = static_cast<std::uint32_t>(z * res + x);
      const std::uint32_t i1 = i0 + 1;
      const std::uint32_t i2 = i0 + static_cast<std::uint32_t>(res);
      const std::uint32_t i3 = i2 + 1;
      mesh.indices.push_back(i0);
      mesh.indices.push_back(i2);
      mesh.indices.push_back(i1);
      mesh.indices.push_back(i1);
      mesh.indices.push_back(i2);
      mesh.indices.push_back(i3);
    }
  }

  return mesh;
}

void updateTerrainColors(TerrainMesh& mesh, const Heightmap& map, float waterLevel, int resolution) {
  for (int z = 0; z < resolution; ++z) {
    for (int x = 0; x < resolution; ++x) {
      auto& v = mesh.vertices[static_cast<std::size_t>(z * resolution + x)];
      heightColor(map.at(x, z), waterLevel, v.r, v.g, v.b);
    }
  }
}

void updateTerrainColors(TerrainMesh& mesh, const BarrenWorld& world, float cellSize) {
  const int resolution = world.heightmap().resolution;
  for (int z = 0; z < resolution; ++z) {
    for (int x = 0; x < resolution; ++x) {
      auto& v = mesh.vertices[static_cast<std::size_t>(z * resolution + x)];
      const float localWater = world.effectiveWaterLevelAt(v.x, v.z, cellSize);
      heightColor(world.heightmap().at(x, z), localWater, v.r, v.g, v.b);
    }
  }
}

}  // namespace evolab::game
