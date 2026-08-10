#pragma once

namespace evolab {

class BarrenWorld;

// Discrete water-column zone at a world XZ sample (depth thresholds in sim height units).
enum class WaterBand {
  Dry,
  Benthic,    // bed-dominated shallow wet
  Shallow,    // under-surface shelf
  Pelagic,    // mid column
  OpenDeep,   // deep open water (free surface still at top)
};

// Where a Nom prefers to sit vertically when wet (phenotype can override later).
enum class NomHabitat {
  Surface,  // default swimmers — ride the tide at the free surface
  Benthic,  // bottom-dwellers — snap to seabed
  Shallow,  // under-surface mid-shelf
  Pelagic,  // mid-water column
};

struct WaterColumn {
  bool wet = false;
  float depth = 0.0f;       // sim height units (same as BarrenWorld::depthAt)
  float terrainY = 0.0f;    // scaled world Y (seabed / land)
  float surfaceY = 0.0f;    // scaled world Y (local free surface)
  WaterBand band = WaterBand::Dry;
};

// Depth band thresholds (sim height units; comparable to TideAdvection dry margins).
inline constexpr float kWaterBandBenthicMaxDepth = 2.0f;
inline constexpr float kWaterBandShallowMaxDepth = 6.0f;
inline constexpr float kWaterBandPelagicMaxDepth = 18.0f;

inline constexpr float kNomSurfaceClearance = 0.12f;
inline constexpr float kNomBenthicClearance = 0.08f;
inline constexpr float kNomDryClearance = 0.05f;
inline constexpr float kEnergonSurfaceClearance = 0.05f;

inline constexpr float kShallowPlacementBlend = 0.35f;
inline constexpr float kPelagicPlacementBlend = 0.65f;

WaterBand classifyWaterBand(float depth);
WaterColumn sampleWaterColumn(const BarrenWorld& world, float wx, float wz, float cellSize,
                              float heightScale);
float placementY(const WaterColumn& column, NomHabitat habitat);

}  // namespace evolab
