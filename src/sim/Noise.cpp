#include "sim/Noise.hpp"

#include <cmath>

namespace evolab {

namespace {

std::uint64_t splitMix64(std::uint64_t& state) {
  std::uint64_t z = (state += 0x9E3779B97F4A7C15ULL);
  z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
  z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
  return z ^ (z >> 31);
}

}  // namespace

Noise::Noise(std::uint64_t seed) : seed_(seed) {}

float Noise::lattice(int ix, int iy) const {
  std::uint64_t state = seed_;
  state ^= static_cast<std::uint64_t>(ix) * 0xD2B74407B1CE6E93ULL;
  state ^= static_cast<std::uint64_t>(iy) * 0x94A7C9870984AD17ULL;
  const std::uint64_t h = splitMix64(state);
  return static_cast<float>(h & 0xFFFFFF) / static_cast<float>(0xFFFFFF);
}

float Noise::lerp(float a, float b, float t) { return a + (b - a) * t; }

float Noise::smooth(float t) { return t * t * (3.0f - 2.0f * t); }

float Noise::sample(float x, float y) const {
  const int x0 = static_cast<int>(std::floor(x));
  const int y0 = static_cast<int>(std::floor(y));
  const int x1 = x0 + 1;
  const int y1 = y0 + 1;
  const float tx = smooth(x - static_cast<float>(x0));
  const float ty = smooth(y - static_cast<float>(y0));

  const float n00 = lattice(x0, y0);
  const float n10 = lattice(x1, y0);
  const float n01 = lattice(x0, y1);
  const float n11 = lattice(x1, y1);

  const float nx0 = lerp(n00, n10, tx);
  const float nx1 = lerp(n01, n11, tx);
  return lerp(nx0, nx1, ty);
}

float Noise::fbm(float x, float y, int octaves, float lacunarity, float gain) const {
  float sum = 0.0f;
  float amp = 1.0f;
  float freq = 1.0f;
  float norm = 0.0f;
  for (int i = 0; i < octaves; ++i) {
    sum += sample(x * freq, y * freq) * amp;
    norm += amp;
    amp *= gain;
    freq *= lacunarity;
  }
  return norm > 0.0f ? sum / norm : 0.0f;
}

}  // namespace evolab
