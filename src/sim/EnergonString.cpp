#include "sim/CellConstants.hpp"
#include "sim/EnergonString.hpp"

#include "sim/CloacaSignal.hpp"
#include "sim/EnergonInformation.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>

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
  return blob.bytes[index];
}

void energonCopyBytesFromBlob(const EnergonBlob& blob, int startIndex, std::uint8_t* dest,
                              int count) {
  if (dest == nullptr || count <= 0 || startIndex < 0) {
    return;
  }
  const int available = static_cast<int>(blob.remaining) - startIndex;
  count = std::min(count, available);
  if (count <= 0) {
    return;
  }
  std::memcpy(dest, blob.bytes + startIndex, static_cast<std::size_t>(count));
}

void energonCopyBytesToBlob(EnergonBlob& blob, int startIndex, const std::uint8_t* src, int count) {
  if (src == nullptr || count <= 0 || startIndex < 0) {
    return;
  }
  count = std::min(count, kEnergonMaxBytesPerBlob - startIndex);
  if (count <= 0) {
    return;
  }
  std::memcpy(blob.bytes + startIndex, src, static_cast<std::size_t>(count));
}

void energonBlobAssignBytes(EnergonBlob& blob, const std::uint8_t* src, int count) {
  count = std::clamp(count, 0, kEnergonMaxBytesPerBlob);
  if (count > 0 && src != nullptr) {
    std::memcpy(blob.bytes, src, static_cast<std::size_t>(count));
  }
  blob.remaining = static_cast<std::uint16_t>(count);
}

void energonBlobAssignLegacyPack(EnergonBlob& blob, std::uint64_t packed, int count) {
  count = std::clamp(count, 0, kEnergonMaxBytesPerBlob);
  for (int i = 0; i < count; ++i) {
    blob.bytes[i] = static_cast<std::uint8_t>((packed >> (8 * i)) & 0xFFu);
  }
  blob.remaining = static_cast<std::uint16_t>(count);
}

std::uint64_t energonPackRawBytes(const std::uint8_t* bytes, int count) {
  std::uint64_t packed = 0;
  count = std::min(count, 8);
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
  return wetTtl * (config.ttlDrySeconds / config.ttlWetSeconds) / kEnergonDryDecayMultiplier;
}

void energonAssignGroundedTtl(EnergonBlob& blob, const EnergonConfig& config, bool onWet,
                              float ttlScale) {
  blob.ttl = onWet ? energonWetTtlSeconds(blob, config, ttlScale)
                   : energonDryTtlSeconds(blob, config, ttlScale);
}

EnergonBlob makeCornucopiaBlob(float x, float z, std::uint8_t byte) {
  if (byte == 0) {
    byte = kEnergonPaletteMate;
  }
  EnergonBlob blob;
  for (int i = 0; i < kEnergonMaxBytesPerBlob; ++i) {
    blob.bytes[i] = byte;
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

EnergonBlob makeWetSunfallBlob(float x, float z, int byteCount, std::uint8_t fillByte, float ttl) {
  const int clamped = std::clamp(byteCount, 1, kEnergonMaxBytesPerBlob);
  EnergonBlob blob;
  for (int i = 0; i < clamped; ++i) {
    blob.bytes[i] = fillByte;
  }
  blob.remaining = static_cast<std::uint16_t>(clamped);
  blob.initialBytes = static_cast<std::uint8_t>(clamped);
  blob.origin = EnergonOrigin::Sunfall;
  blob.x = x;
  blob.z = z;
  blob.y = 0.0f;
  blob.grounded = true;
  blob.onWet = true;
  blob.ttl = ttl;
  energonBlobInitPoint(blob);
  return blob;
}

std::uint64_t energonPackBytes(const EnergonBlob& blob, int startIndex, int count) {
  std::uint8_t temp[kEnergonMaxBytesPerBlob]{};
  energonCopyBytesFromBlob(blob, startIndex, temp, count);
  return energonPackRawBytes(temp, count);
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
  if (abLenSq <= 1.0e-8f) {
    tOut = 0.0f;
    return apx * apx + apz * apz;
  }
  tOut = std::clamp((apx * abx + apz * abz) / abLenSq, 0.0f, 1.0f);
  const float cx = ax + tOut * abx;
  const float cz = az + tOut * abz;
  const float dx = px - cx;
  const float dz = pz - cz;
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
  const float last = static_cast<float>(std::max(1, static_cast<int>(blob.remaining)));
  const float t = 1.0f / last;
  blob.tailX = blob.tailX + t * (blob.headX - blob.tailX);
  blob.tailZ = blob.tailZ + t * (blob.headZ - blob.tailZ);
  energonBlobSyncCenter(blob);
}

void energonShrinkHeadGeometry(EnergonBlob& blob) {
  if (blob.remaining <= 1) {
    energonBlobInitPoint(blob);
    return;
  }
  const float last = static_cast<float>(std::max(1, static_cast<int>(blob.remaining)));
  const float t = 1.0f / last;
  blob.headX = blob.headX + t * (blob.tailX - blob.headX);
  blob.headZ = blob.headZ + t * (blob.tailZ - blob.headZ);
  energonBlobSyncCenter(blob);
}

}  // namespace evolab
