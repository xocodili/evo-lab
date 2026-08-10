#pragma once

#include <cstdint>

namespace evolab {

// Deterministic 2D value noise in [0, 1].
class Noise {
public:
  explicit Noise(std::uint64_t seed);

  float sample(float x, float y) const;
  float fbm(float x, float y, int octaves, float lacunarity, float gain) const;

private:
  std::uint64_t seed_;

  float lattice(int ix, int iy) const;
  static float lerp(float a, float b, float t);
  static float smooth(float t);
};

}  // namespace evolab
