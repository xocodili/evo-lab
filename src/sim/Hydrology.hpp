#pragma once



#include "sim/Heightmap.hpp"



#include <vector>



namespace evolab {



struct FlowVector {

  float dx = 0.0f;

  float dz = 0.0f;

  bool valid = false;

};



// Minimum global tide level for hydraulic connection from each cell to a map edge.

// Minimax elevation along the best path to the boundary (single-pass Dijkstra).

std::vector<float> computeSpillHeights(const Heightmap& map);



// D8 steepest-descent drainage directions (unit XZ, terrain-only, precomputed per seed).

std::vector<FlowVector> computeFlowDirections(const Heightmap& map, const std::vector<float>& spillHeights);



bool shouldImpoundBasin(float globalLevel, float spillHeight);



float localWaterSurface(float globalLevel, float spillHeight, bool impounded);



float localWaterDepth(float globalLevel, float spillHeight, bool impounded, float terrainHeight);



}  // namespace evolab

