#include "sim/EnergonString.hpp"

#include "sim/CloacaSignal.hpp"

#include <algorithm>
#include <cmath>

namespace evolab {

void energonBlobInitPoint(EnergonBlob& blob) {
  blob.headX = blob.x;
  blob.headZ = blob.z;
  blob.tailX = blob.x;
  blob.tailZ = blob.z;
}

void energonBlobLayoutSegment(EnergonBlob& blob, float cellSize, float headingRadians) {
  if (blob.remaining <= 1) {
    energonBlobInitPoint(blob);
    return;
  }

  const float length =
      std::max(cellSize * 0.18f, static_cast<float>(blob.remaining) * cellSize * kEnergonSegmentUnitFactor);
  const float halfLen = length * 0.5f;
  const float hx = std::cos(headingRadians) * halfLen;
  const float hz = std::sin(headingRadians) * halfLen;
  blob.headX = blob.x + hx;
  blob.headZ = blob.z + hz;
  blob.tailX = blob.x - hx;
  blob.tailZ = blob.z - hz;
}

void energonBlobSyncCenter(EnergonBlob& blob) {
  blob.x = (blob.headX + blob.tailX) * 0.5f;
  blob.z = (blob.headZ + blob.tailZ) * 0.5f;
}

void energonTranslateBlob(EnergonBlob& blob, float dx, float dz) {
  blob.headX += dx;
  blob.headZ += dz;
  blob.tailX += dx;
  blob.tailZ += dz;
  energonBlobSyncCenter(blob);
}

float energonAnchorWorldX(const EnergonBlob& blob, float anchorT) {
  return blob.tailX + anchorT * (blob.headX - blob.tailX);
}

float energonAnchorWorldZ(const EnergonBlob& blob, float anchorT) {
  return blob.tailZ + anchorT * (blob.headZ - blob.tailZ);
}

std::uint8_t energonByteAt(const EnergonBlob& blob, int index) {
  if (index < 0 || index >= blob.remaining) {
    return 0;
  }
  return static_cast<std::uint8_t>((blob.data >> (8 * index)) & 0xFFu);
}

std::uint64_t energonPackRawBytes(const std::uint8_t* bytes, int count) {
  std::uint64_t packed = 0;
  for (int i = 0; i < count; ++i) {
    packed |= static_cast<std::uint64_t>(bytes[i]) << (8 * i);
  }
  return packed;
}

float energonWetTtlScaleForBlob(const EnergonBlob& blob) {
  switch (blob.origin) {
    case EnergonOrigin::Fragment:
    case EnergonOrigin::Waste:
      return kEnergonFragmentTtlScale;
    case EnergonOrigin::Cloaca:
      switch (cloacaBandFromBlob(blob)) {
        case CloacaBand::Distress:
          return kEnergonTtlDistressScale;
        case CloacaBand::Baseline:
          return kEnergonTtlBaselineScale;
        case CloacaBand::Mate:
          return kEnergonTtlMateScale;
        default:
          return kEnergonTtlBaselineScale;
      }
    case EnergonOrigin::Signal:
      return kEnergonTtlBaselineScale;
    case EnergonOrigin::Sunfall:
    default:
      return 1.0f;
  }
}

float energonWetTtlSeconds(const EnergonBlob& blob, const EnergonConfig& config, float ttlScale) {
  return config.ttlWetSeconds * energonWetTtlScaleForBlob(blob) * ttlScale;
}

float energonDryTtlSeconds(const EnergonBlob& blob, const EnergonConfig& config, float ttlScale) {
  if (config.ttlWetSeconds <= 0.0f) {
    return 0.0f;
  }
  const float wetTtl = energonWetTtlSeconds(blob, config, ttlScale);
  return wetTtl * (config.ttlDrySeconds / config.ttlWetSeconds);
}

void energonAssignGroundedTtl(EnergonBlob& blob, const EnergonConfig& config, bool onWet,
                              float ttlScale) {
  blob.ttl = onWet ? energonWetTtlSeconds(blob, config, ttlScale)
                   : energonDryTtlSeconds(blob, config, ttlScale);
}

EnergonBlob makeCornucopiaBlob(float x, float z, std::uint8_t byte) {
  EnergonBlob blob;
  blob.data = byte;
  for (int i = 1; i < kEnergonMaxBytesPerBlob; ++i) {
    blob.data |= static_cast<std::uint64_t>(byte) << (8 * i);
  }
  blob.remaining = static_cast<std::uint16_t>(kEnergonMaxBytesPerBlob);
  blob.initialBytes = static_cast<std::uint8_t>(kEnergonMaxBytesPerBlob);
  blob.origin = EnergonOrigin::Sunfall;
  blob.x = x;
  blob.z = z;
  blob.y = 0.0f;
  blob.grounded = true;
  blob.onWet = true;
  blob.cornucopia = true;
  energonBlobInitPoint(blob);
  return blob;
}

std::uint64_t energonPackBytes(const EnergonBlob& blob, int startIndex, int count) {
  std::uint64_t packed = 0;
  for (int i = 0; i < count; ++i) {
    packed |= static_cast<std::uint64_t>(energonByteAt(blob, startIndex + i)) << (8 * i);
  }
  return packed;
}

float energonPointSegmentDistanceSq(float px, float pz, const EnergonBlob& blob, float& tOut) {
  const float ax = blob.tailX;
  const float az = blob.tailZ;
  const float bx = blob.headX;
  const float bz = blob.headZ;
  const float abx = bx - ax;
  const float abz = bz - az;
  const float apx = px - ax;
  const float apz = pz - az;
  const float abLenSq = abx * abx + abz * abz;

  float t = 0.0f;
  if (abLenSq > 1.0e-6f) {
    t = (apx * abx + apz * abz) / abLenSq;
    t = std::clamp(t, 0.0f, 1.0f);
  }

  const float cx = ax + t * abx;
  const float cz = az + t * abz;
  const float dx = px - cx;
  const float dz = pz - cz;
  tOut = t;
  return dx * dx + dz * dz;
}

int energonByteIndexAtProjection(const EnergonBlob& blob, float t) {
  if (blob.remaining <= 1) {
    return 0;
  }
  const int last = static_cast<int>(blob.remaining) - 1;
  return std::clamp(static_cast<int>(std::lround(t * static_cast<float>(last))), 0, last);
}

void energonShrinkTailGeometry(EnergonBlob& blob) {
  if (blob.remaining <= 1) {
    energonBlobInitPoint(blob);
    return;
  }
  const float step = 1.0f / static_cast<float>(blob.remaining);
  blob.tailX += (blob.headX - blob.tailX) * step;
  blob.tailZ += (blob.headZ - blob.tailZ) * step;
  energonBlobSyncCenter(blob);
}

void energonShrinkHeadGeometry(EnergonBlob& blob) {
  if (blob.remaining <= 1) {
    energonBlobInitPoint(blob);
    return;
  }
  const float step = 1.0f / static_cast<float>(blob.remaining);
  blob.headX -= (blob.headX - blob.tailX) * step;
  blob.headZ -= (blob.headZ - blob.tailZ) * step;
  energonBlobSyncCenter(blob);
}

}  // namespace evolab
