#include "sim/EnergonInformation.hpp"

#include "sim/EnergonString.hpp"

#include <algorithm>
#include <cmath>
#include <random>

namespace evolab {

float energonInformationValue(std::uint8_t byte) {
  return static_cast<float>(byte) / 255.0f;
}

float energonBlobInformationMass(const EnergonBlob& blob) {
  float mass = 0.0f;
  for (int i = 0; i < blob.remaining; ++i) {
    mass += energonInformationValue(energonByteAt(blob, i));
  }
  return mass;
}

void energonPaletteRgb(std::uint8_t byte, float& r, float& g, float& b) {
  // Hue 240° (cool blue-violet) → 0° (warm red); fixed saturation/value for legibility.
  const float t = static_cast<float>(byte) / 255.0f;
  const float hue = (1.0f - t) * 240.0f;
  const float s = 0.82f;
  const float v = 0.55f + 0.40f * t;
  const float c = v * s;
  const float hPrime = hue / 60.0f;
  const float x = c * (1.0f - std::abs(std::fmod(hPrime, 2.0f) - 1.0f));
  const float m = v - c;

  float rp = 0.0f;
  float gp = 0.0f;
  float bp = 0.0f;
  if (hue < 60.0f) {
    rp = c;
    gp = x;
  } else if (hue < 120.0f) {
    rp = x;
    gp = c;
  } else if (hue < 180.0f) {
    gp = c;
    bp = x;
  } else if (hue < 240.0f) {
    gp = x;
    bp = c;
  } else if (hue < 300.0f) {
    rp = x;
    bp = c;
  } else {
    rp = c;
    bp = x;
  }
  r = rp + m;
  g = gp + m;
  b = bp + m;
}

std::uint8_t energonDecayByte(std::uint8_t byte, int steps) {
  if (steps <= 0) {
    return byte;
  }
  const int cooled = static_cast<int>(byte) - steps;
  return static_cast<std::uint8_t>(std::max(0, cooled));
}

std::uint8_t energonHopCoolByte(std::uint8_t byte, float eta) {
  if (byte == 0 || eta >= 1.0f - 1.0e-5f) {
    return byte;
  }
  const int steps = std::max(
      1, static_cast<int>(std::lround((1.0f - eta) * static_cast<float>(kEnergonHopCoolSpan))));
  return energonDecayByte(byte, steps);
}

std::uint8_t energonRandomSunfallByte(std::mt19937_64& rng) {
  std::uniform_int_distribution<int> dist(0, 255);
  return static_cast<std::uint8_t>(dist(rng));
}

void energonSetByteAt(EnergonBlob& blob, int index, std::uint8_t value) {
  if (index < 0 || index >= blob.remaining) {
    return;
  }
  blob.bytes[index] = value;
}

void energonCompactZeroBytes(EnergonBlob& blob) {
  std::uint8_t kept[kEnergonMaxBytesPerBlob]{};
  int keptCount = 0;
  for (int i = 0; i < blob.remaining; ++i) {
    const std::uint8_t byte = energonByteAt(blob, i);
    if (byte > 0) {
      kept[keptCount++] = byte;
    }
  }
  blob.remaining = static_cast<std::uint16_t>(keptCount);
  energonBlobAssignBytes(blob, kept, keptCount);
  if (keptCount <= 1) {
    energonBlobInitPoint(blob);
  }
}

void energonEntropyDecayBlob(EnergonBlob& blob, int steps) {
  if (steps <= 0 || blob.remaining == 0 || blob.cornucopia) {
    return;
  }
  for (int i = 0; i < blob.remaining; ++i) {
    const std::uint8_t cooled = energonDecayByte(energonByteAt(blob, i), steps);
    energonSetByteAt(blob, i, cooled);
  }
  energonCompactZeroBytes(blob);
}

}  // namespace evolab
