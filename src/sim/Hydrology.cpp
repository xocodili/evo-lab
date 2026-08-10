#include "sim/Hydrology.hpp"



#include <algorithm>

#include <cmath>

#include <queue>

#include <utility>

#include <vector>



namespace evolab {



namespace {



bool normalizeFlow(float& dx, float& dz) {

  const float len = std::sqrt(dx * dx + dz * dz);

  if (len <= 1.0e-5f) {

    dx = 0.0f;

    dz = 0.0f;

    return false;

  }

  dx /= len;

  dz /= len;

  return true;

}



}  // namespace



std::vector<float> computeSpillHeights(const Heightmap& map) {

  const int res = map.resolution;

  if (res <= 0 || map.samples.empty()) {

    return {};

  }



  const std::size_t count = static_cast<std::size_t>(res * res);

  const float kInf = 1.0e9f;

  std::vector<float> spill(count, kInf);



  using Node = std::pair<float, int>;

  std::priority_queue<Node, std::vector<Node>, std::greater<Node>> open;



  auto index = [res](int x, int z) { return z * res + x; };



  for (int z = 0; z < res; ++z) {

    for (int x = 0; x < res; ++x) {

      if (x != 0 && x != res - 1 && z != 0 && z != res - 1) {

        continue;

      }

      const int i = index(x, z);

      const float h = map.at(x, z);

      if (h < spill[static_cast<std::size_t>(i)]) {

        spill[static_cast<std::size_t>(i)] = h;

        open.push({h, i});

      }

    }

  }



  constexpr int kDx[] = {1, -1, 0, 0};

  constexpr int kDz[] = {0, 0, 1, -1};



  while (!open.empty()) {

    const Node node = open.top();

    open.pop();

    const float cost = node.first;

    const int i = node.second;

    if (cost > spill[static_cast<std::size_t>(i)]) {

      continue;

    }



    const int x = i % res;

    const int z = i / res;

    for (int d = 0; d < 4; ++d) {

      const int nx = x + kDx[d];

      const int nz = z + kDz[d];

      if (nx < 0 || nx >= res || nz < 0 || nz >= res) {

        continue;

      }

      const int ni = index(nx, nz);

      const float candidate = std::max(cost, map.at(nx, nz));

      if (candidate < spill[static_cast<std::size_t>(ni)]) {

        spill[static_cast<std::size_t>(ni)] = candidate;

        open.push({candidate, ni});

      }

    }

  }



  return spill;

}



std::vector<FlowVector> computeFlowDirections(const Heightmap& map,

                                              const std::vector<float>& spillHeights) {

  const int res = map.resolution;

  if (res <= 0 || map.samples.empty() ||

      spillHeights.size() != static_cast<std::size_t>(res * res)) {

    return {};

  }



  auto index = [res](int x, int z) { return static_cast<std::size_t>(z * res + x); };



  constexpr int kDx8[] = {1, 1, 0, -1, -1, -1, 0, 1};

  constexpr int kDz8[] = {0, 1, 1, 1, 0, -1, -1, -1};

  constexpr float kDist8[] = {1.0f, 1.4142135f, 1.0f, 1.4142135f,

                              1.0f, 1.4142135f, 1.0f, 1.4142135f};



  std::vector<FlowVector> flow(static_cast<std::size_t>(res * res));

  const float virtualOcean = map.minHeight - 10.0f;



  for (int z = 0; z < res; ++z) {

    for (int x = 0; x < res; ++x) {

      const float height = map.at(x, z);

      const float cellSpill = spillHeights[index(x, z)];



      auto heightAtNeighbor = [&](int nx, int nz) -> float {

        if (nx < 0 || nx >= res || nz < 0 || nz >= res) {

          return virtualOcean;

        }

        return map.at(nx, nz);

      };



      int bestX = x;

      int bestZ = z;

      float bestHeight = height;

      bool foundLower = false;



      for (int d = 0; d < 8; ++d) {

        const int nx = x + kDx8[d];

        const int nz = z + kDz8[d];

        const float neighborH = heightAtNeighbor(nx, nz);

        if (neighborH + 1.0e-4f < bestHeight) {

          bestHeight = neighborH;

          bestX = nx;

          bestZ = nz;

          foundLower = true;

        }

      }



      if (!foundLower) {

        float bestSpill = cellSpill;

        for (int d = 0; d < 8; ++d) {

          const int nx = x + kDx8[d];

          const int nz = z + kDz8[d];

          if (nx < 0 || nx >= res || nz < 0 || nz >= res) {

            continue;

          }

          const float neighborSpill = spillHeights[index(nx, nz)];

          if (neighborSpill + 1.0e-4f < bestSpill) {

            bestSpill = neighborSpill;

            bestX = nx;

            bestZ = nz;

            foundLower = true;

          }

        }

      }



      if (!foundLower || (bestX == x && bestZ == z)) {

        continue;

      }



      float dx = 0.0f;

      float dz = 0.0f;

      if (bestX < 0) {

        dx = -1.0f;

      } else if (bestX >= res) {

        dx = 1.0f;

      } else {

        dx = static_cast<float>(bestX - x);

      }

      if (bestZ < 0) {

        dz = -1.0f;

      } else if (bestZ >= res) {

        dz = 1.0f;

      } else {

        dz = static_cast<float>(bestZ - z);

      }



      if (bestX >= 0 && bestX < res && bestZ >= 0 && bestZ < res) {

        for (int d = 0; d < 8; ++d) {

          if (x + kDx8[d] == bestX && z + kDz8[d] == bestZ) {

            dx *= kDist8[d];

            dz *= kDist8[d];

            break;

          }

        }

      }



      FlowVector& out = flow[index(x, z)];

      out.dx = dx;

      out.dz = dz;

      out.valid = normalizeFlow(out.dx, out.dz);

    }

  }



  return flow;

}



bool shouldImpoundBasin(float globalLevel, float spillHeight) {

  return globalLevel >= spillHeight;

}



float localWaterSurface(float globalLevel, float spillHeight, bool impounded) {

  if (globalLevel >= spillHeight) {

    return globalLevel;

  }

  if (impounded) {

    return spillHeight;

  }

  return globalLevel;

}



float localWaterDepth(float globalLevel, float spillHeight, bool impounded, float terrainHeight) {

  if (globalLevel >= spillHeight) {

    const float depth = globalLevel - terrainHeight;

    return depth > 0.0f ? depth : 0.0f;

  }

  if (impounded) {

    const float depth = spillHeight - terrainHeight;

    return depth > 0.0f ? depth : 0.0f;

  }

  return 0.0f;

}



}  // namespace evolab

