#pragma once

#include "sim/Energon.hpp"
#include "sim/Organism.hpp"

#include <cstdint>
#include <functional>
#include <random>
#include <vector>

namespace evolab {

class BarrenWorld;

struct CellPopulationStats {
  int liveCells = 0;
  int organisms = 0;
  int stemCells = 0;
  int mouthOrganisms = 0;
  int mouthNeurons = 0;
  int actuatorOrganisms = 0;
  int skeletonLinks = 0;
  int neuralAxons = 0;
};

class CellPopulation {
public:
  void clear();
  void seedStemCells(const BarrenWorld& world, float cellSize, float heightScale, int count,
                     std::uint64_t seed);
  void seedMouthOrganisms(const BarrenWorld& world, float cellSize, float heightScale, int count,
                          std::uint64_t seed, int mouthsPerOrganism = 1);
  void seedTwoMouthOrganisms(const BarrenWorld& world, float cellSize, float heightScale, int count,
                             std::uint64_t seed);
  void seedActuatorOrganisms(const BarrenWorld& world, float cellSize, float heightScale, int count,
                             std::uint64_t seed);

  void tick(const BarrenWorld& world, EnergonField& energon, float cellSize, float heightScale);

  const std::vector<Organism>& organisms() const { return organisms_; }
  CellPopulationStats stats() const;

  const Organism* findById(std::uint32_t id) const;

private:
  void seedOnWetTerrain(const BarrenWorld& world, float cellSize, float heightScale, int count,
                        std::uint64_t seedSalt, bool clearFirst, int maxAttemptsPerOrganism,
                        const std::function<Organism(float, float, float, std::mt19937&)>& build,
                        const std::function<void(Organism&, std::mt19937&)>& afterFinalize = {});

  std::vector<Organism> organisms_;
  std::uint32_t nextId_ = 1;
};

int countOrganisms(const std::vector<Organism>& organisms);

}  // namespace evolab
