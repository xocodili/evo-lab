#include "sim/Heightmap.hpp"

#include "sim/Noise.hpp"

#include <algorithm>
#include <cmath>
#include <numeric>

namespace evolab {

float Heightmap::at(int x, int z) const {
  if (resolution <= 0 || samples.empty()) {
    return 0.0f;
  }
  x = std::max(0, std::min(x, resolution - 1));
  z = std::max(0, std::min(z, resolution - 1));
  return samples[static_cast<std::size_t>(z * resolution + x)];
}

float Heightmap::atNormalized(float u, float v) const {
  if (resolution <= 0) {
    return 0.0f;
  }
  u = std::max(0.0f, std::min(u, 1.0f));
  v = std::max(0.0f, std::min(v, 1.0f));
  const int x = static_cast<int>(u * static_cast<float>(resolution - 1));
  const int z = static_cast<int>(v * static_cast<float>(resolution - 1));
  return at(x, z);
}

int Heightmap::countAbove(float level) const {
  return static_cast<int>(std::count_if(samples.begin(), samples.end(),
                                         [level](float h) { return h > level; }));
}

int Heightmap::countBelow(float level) const {
  return static_cast<int>(std::count_if(samples.begin(), samples.end(),
                                         [level](float h) { return h < level; }));
}

float Heightmap::variance() const {
  if (samples.empty()) {
    return 0.0f;
  }
  const float mean =
      std::accumulate(samples.begin(), samples.end(), 0.0f) /
      static_cast<float>(samples.size());
  float acc = 0.0f;
  for (float s : samples) {
    const float d = s - mean;
    acc += d * d;
  }
  return acc / static_cast<float>(samples.size());
}

Heightmap generateHeightmap(std::uint64_t seed, int resolution) {
  Heightmap map;
  map.resolution = resolution;
  map.minHeight = -40.0f;
  map.maxHeight = 40.0f;
  map.seaLevel = 0.0f;
  map.samples.resize(static_cast<std::size_t>(resolution * resolution));

  Noise noise(seed);
  const float scale = 0.04f;

  for (int z = 0; z < resolution; ++z) {
    for (int x = 0; x < resolution; ++x) {
      const float nx = static_cast<float>(x) * scale;
      const float nz = static_cast<float>(z) * scale;
      const float n = noise.fbm(nx, nz, 5, 2.0f, 0.5f);
      const float ridge = 1.0f - std::fabs(n * 2.0f - 1.0f);
      const float h = map.minHeight + (map.maxHeight - map.minHeight) * (0.65f * n + 0.35f * ridge);
      map.samples[static_cast<std::size_t>(z * resolution + x)] = h;
    }
  }

  return map;
}

}  // namespace evolab
