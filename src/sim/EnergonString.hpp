#pragma once

#include "sim/Energon.hpp"

namespace evolab {

// Byte 0 sits at the tail end; byte (remaining-1) at the head end of the segment.
inline constexpr float kEnergonSegmentUnitFactor = 0.22f;
inline constexpr int kEnergonMaxBytesPerBlob = 8;

// Wet TTL multipliers (relative to EnergonConfig::ttlWetSeconds). Blue decays fastest, red slowest.
inline constexpr float kEnergonTtlDistressScale = 0.22f;
inline constexpr float kEnergonTtlBaselineScale = 0.45f;
inline constexpr float kEnergonTtlMateScale = 1.0f;
inline constexpr float kEnergonFragmentTtlScale = 0.35f;
// Eviction rank only while airborne — TTL does not decay until the blob lands.
inline constexpr float kEnergonAirborneTtlSeconds = 120.0f;

void energonBlobInitPoint(EnergonBlob& blob);
void energonBlobLayoutSegment(EnergonBlob& blob, float cellSize, float headingRadians);
void energonBlobSyncCenter(EnergonBlob& blob);

void energonTranslateBlob(EnergonBlob& blob, float dx, float dz);

float energonAnchorWorldX(const EnergonBlob& blob, float anchorT);

float energonAnchorWorldZ(const EnergonBlob& blob, float anchorT);

std::uint8_t energonByteAt(const EnergonBlob& blob, int index);
std::uint64_t energonPackBytes(const EnergonBlob& blob, int startIndex, int count);
std::uint64_t energonPackRawBytes(const std::uint8_t* bytes, int count);

// Grounded TTL from origin / cloaca band (blue < green < red) and optional caller scale.
float energonWetTtlScaleForBlob(const EnergonBlob& blob);
float energonWetTtlSeconds(const EnergonBlob& blob, const EnergonConfig& config,
                           float ttlScale = 1.0f);
float energonDryTtlSeconds(const EnergonBlob& blob, const EnergonConfig& config,
                           float ttlScale = 1.0f);
void energonAssignGroundedTtl(EnergonBlob& blob, const EnergonConfig& config, bool onWet,
                              float ttlScale = 1.0f);

EnergonBlob makeCornucopiaBlob(float x, float z, std::uint8_t byte = 0x42);

// Squared distance from point to segment; tOut is projection in [0, 1] tail→head.
float energonPointSegmentDistanceSq(float px, float pz, const EnergonBlob& blob, float& tOut);

int energonByteIndexAtProjection(const EnergonBlob& blob, float t);

void energonShrinkTailGeometry(EnergonBlob& blob);
void energonShrinkHeadGeometry(EnergonBlob& blob);

}  // namespace evolab
