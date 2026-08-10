#pragma once

#include "sim/Energon.hpp"

namespace evolab {

// Byte 0 sits at the tail end; byte (remaining-1) at the head end of the segment.
inline constexpr float kEnergonSegmentUnitFactor = 0.22f;

void energonBlobInitPoint(EnergonBlob& blob);
void energonBlobLayoutSegment(EnergonBlob& blob, float cellSize, float headingRadians);
void energonBlobSyncCenter(EnergonBlob& blob);

std::uint8_t energonByteAt(const EnergonBlob& blob, int index);
std::uint64_t energonPackBytes(const EnergonBlob& blob, int startIndex, int count);

// Squared distance from point to segment; tOut is projection in [0, 1] tail→head.
float energonPointSegmentDistanceSq(float px, float pz, const EnergonBlob& blob, float& tOut);

int energonByteIndexAtProjection(const EnergonBlob& blob, float t);

void energonShrinkTailGeometry(EnergonBlob& blob);
void energonShrinkHeadGeometry(EnergonBlob& blob);

}  // namespace evolab
