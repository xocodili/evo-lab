#pragma once



#include "sim/Heightmap.hpp"

#include "sim/Hydrology.hpp"

#include "sim/Tide.hpp"



#include <cstdint>

#include <vector>



namespace evolab {



struct WetnessStats {

  int wetCells = 0;

  int dryCells = 0;

};



class BarrenWorld {

public:

  BarrenWorld(std::uint64_t seed, int resolution, Tide tide = {});



  void regenerate(std::uint64_t seed);

  void tick();



  std::uint64_t tickCount() const { return tick_; }

  std::uint64_t seed() const { return seed_; }

  const Heightmap& heightmap() const { return heightmap_; }

  const Tide& tide() const { return tide_; }



  float waterLevel() const;

  float waterLevelDelta() const;

  float effectiveWaterLevelAt(float worldX, float worldZ, float cellSize) const;

  float spillHeightAt(int x, int z) const;

  float spillHeightAtWorld(float worldX, float worldZ, float cellSize) const;

  bool isHydraulicallyConnected(int x, int z) const;

  bool isHydraulicallyConnectedAt(float worldX, float worldZ, float cellSize) const;

  bool isImpoundedAt(float worldX, float worldZ, float cellSize) const;

  FlowVector flowDirectionAt(int x, int z) const;

  FlowVector flowDirectionAtWorld(float worldX, float worldZ, float cellSize) const;

  float heightAt(int x, int z) const;

  float heightAtWorld(float worldX, float worldZ, float cellSize) const;

  float depthAtWorld(float worldX, float worldZ, float cellSize) const;

  bool isWetWorld(float worldX, float worldZ, float cellSize) const;

  float depthAt(int x, int z) const;

  bool isWet(int x, int z) const;



  WetnessStats wetnessStats() const;

  float heightChecksum() const;



private:

  void rebuildHydrology();

  int cellIndex(int x, int z) const;

  void worldToCell(float worldX, float worldZ, float cellSize, int& x, int& z) const;

  float waterSurfaceAt(int x, int z) const;



  std::uint64_t seed_ = 0;

  int resolution_ = 0;

  std::uint64_t tick_ = 0;

  Heightmap heightmap_;

  Tide tide_;

  std::vector<float> spillHeights_;

  std::vector<FlowVector> flowDirections_;

  std::vector<uint8_t> impounded_;

};



}  // namespace evolab

