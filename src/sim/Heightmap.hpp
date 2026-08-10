#pragma once

#include <cstdint>
#include <vector>

namespace evolab {

struct Heightmap {
  int resolution = 0;
  float minHeight = 0.0f;
  float maxHeight = 0.0f;
  float seaLevel = 0.0f;
  std::vector<float> samples;

  float at(int x, int z) const;
  float atNormalized(float u, float v) const;

  int countAbove(float level) const;
  int countBelow(float level) const;
  float variance() const;
};

Heightmap generateHeightmap(std::uint64_t seed, int resolution);

}  // namespace evolab
