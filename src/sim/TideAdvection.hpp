#pragma once

namespace evolab {

class BarrenWorld;

struct TideAdvectionConfig {
  float dryMarginDepth = 1.6f;
  // Ebb: blend downhill drainage with depth convergence into pools.
  float poolFlowBlend = 0.35f;
  // Flood: blend toward interior near map rim so edge cells re-enter the basin.
  float boundaryInflowWeight = 0.72f;
  float speedScale = 4.2f;
  float maxStepPerTick = 0.52f;
};

struct AdvectionVelocity {
  float vx = 0.0f;
  float vz = 0.0f;
  bool active = false;
};

// Tidal drift along precomputed downhill drainage (inverted on flood). Only active when
// hydraulically connected to the map edge; impounded basins do not advect.
AdvectionVelocity shoreAdvection(const BarrenWorld& world, float worldX, float worldZ, float cellSize,
                                 float worldHalfExtent,
                                 const TideAdvectionConfig& config = {});

void applyShoreAdvection(float& worldX, float& worldZ, const AdvectionVelocity& velocity,
                         float worldHalfExtent, float boundaryMargin = 0.0f);

}  // namespace evolab
