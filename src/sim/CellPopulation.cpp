#include "sim/CellPopulation.hpp"

#include "sim/BarrenWorld.hpp"
#include "sim/CellConstants.hpp"
#include "sim/Chaos.hpp"
#include "sim/NeuronTick.hpp"
#include "sim/OrganismHgt.hpp"
#include "sim/OrganismFeedbagOracle.hpp"
#include "sim/OrganismParthenogenesis.hpp"
#include "sim/Organism.hpp"
#include "sim/TideAdvection.hpp"
#include "sim/WaterColumn.hpp"
#include "sim/WorldConstants.hpp"

#include <algorithm>
#include <cmath>
#include <random>
#include <unordered_map>
#include <vector>

namespace evolab {

namespace {

class DisjointSet {
public:
  explicit DisjointSet(int n) : parent_(static_cast<std::size_t>(n)), rank_(static_cast<std::size_t>(n), 0) {
    for (int i = 0; i < n; ++i) {
      parent_[static_cast<std::size_t>(i)] = i;
    }
  }

  int find(int x) {
    if (parent_[static_cast<std::size_t>(x)] != x) {
      parent_[static_cast<std::size_t>(x)] = find(parent_[static_cast<std::size_t>(x)]);
    }
    return parent_[static_cast<std::size_t>(x)];
  }

  void unite(int a, int b) {
    a = find(a);
    b = find(b);
    if (a == b) {
      return;
    }
    if (rank_[static_cast<std::size_t>(a)] < rank_[static_cast<std::size_t>(b)]) {
      std::swap(a, b);
    }
    parent_[static_cast<std::size_t>(b)] = a;
    if (rank_[static_cast<std::size_t>(a)] == rank_[static_cast<std::size_t>(b)]) {
      ++rank_[static_cast<std::size_t>(a)];
    }
  }

private:
  std::vector<int> parent_;
  std::vector<int> rank_;
};

float worldHalfExtent(const BarrenWorld& world, float cellSize) {
  const int res = world.heightmap().resolution;
  if (res <= 1 || cellSize <= 0.0f) {
    return 0.0f;
  }
  return static_cast<float>(res - 1) * cellSize * 0.5f;
}

bool trySampleWetSpawnSite(const BarrenWorld& world, float cellSize, float heightScale, float half,
                           std::mt19937& rng, float& wx, float& wz, float& wy) {
  std::uniform_real_distribution<float> posDist(-half, half);
  wx = posDist(rng);
  wz = posDist(rng);
  if (!world.isWetWorld(wx, wz, cellSize)) {
    return false;
  }

  const WaterColumn column = sampleWaterColumn(world, wx, wz, cellSize, heightScale);
  wy = column.surfaceY + chaosJitterFloat(kSpawnSurfaceYOffset, rng);
  return true;
}

}  // namespace

int countOrganisms(const std::vector<Organism>& organisms) {
  std::vector<int> liveIndices;
  liveIndices.reserve(organisms.size());
  std::unordered_map<std::uint32_t, int> idToIndex;
  idToIndex.reserve(organisms.size());

  for (int i = 0; i < static_cast<int>(organisms.size()); ++i) {
    if (!organisms[static_cast<std::size_t>(i)].alive) {
      continue;
    }
    idToIndex[organisms[static_cast<std::size_t>(i)].id] = static_cast<int>(liveIndices.size());
    liveIndices.push_back(i);
  }

  if (liveIndices.empty()) {
    return 0;
  }

  DisjointSet dsu(static_cast<int>(liveIndices.size()));
  for (int liveIdx = 0; liveIdx < static_cast<int>(liveIndices.size()); ++liveIdx) {
    const Organism& organism =
        organisms[static_cast<std::size_t>(liveIndices[static_cast<std::size_t>(liveIdx)])];
    for (const ColonyAxon& axon : organism.colonyAxons) {
      const auto it = idToIndex.find(axon.targetOrganismId);
      if (it == idToIndex.end()) {
        continue;
      }
      dsu.unite(liveIdx, it->second);
    }
  }

  int components = 0;
  for (int i = 0; i < static_cast<int>(liveIndices.size()); ++i) {
    if (dsu.find(i) == i) {
      ++components;
    }
  }
  return components;
}

void CellPopulation::seedOnWetTerrain(
    const BarrenWorld& world, float cellSize, float heightScale, int count,
    std::uint64_t seedSalt, bool clearFirst, int maxAttemptsPerOrganism,
    const std::function<Organism(float, float, float, std::mt19937&)>& build,
    const std::function<void(Organism&, std::mt19937&)>& afterFinalize) {
  if (clearFirst) {
    clear();
  }
  if (count <= 0) {
    return;
  }

  const int res = world.heightmap().resolution;
  const float half = static_cast<float>(res - 1) * cellSize * 0.5f;
  if (half <= 0.0f) {
    return;
  }

  std::mt19937 rng = chaosSpawnRng(world.seed(), seedSalt);
  const int targetCount = clearFirst ? count : static_cast<int>(organisms_.size()) + count;
  int attempts = 0;
  const int maxAttempts = count * maxAttemptsPerOrganism;

  while (static_cast<int>(organisms_.size()) < targetCount && attempts < maxAttempts) {
    ++attempts;
    float wx = 0.0f;
    float wz = 0.0f;
    float wy = 0.0f;
    if (!trySampleWetSpawnSite(world, cellSize, heightScale, half, rng, wx, wz, wy)) {
      continue;
    }

    Organism organism = build(wx, wz, wy, rng);
    organism.finalizeSpawn(rng);
    if (afterFinalize) {
      afterFinalize(organism, rng);
    }
    organisms_.push_back(std::move(organism));
  }
}

void CellPopulation::clear() {
  organisms_.clear();
  nextId_ = 1;
}

void CellPopulation::seedStemCells(const BarrenWorld& world, float cellSize, float heightScale,
                                   int count, std::uint64_t seed) {
  (void)seed;
  seedOnWetTerrain(
      world, cellSize, heightScale, count, kChaosSaltStemCell, true, 40,
      [this, &world](float wx, float wz, float wy, std::mt19937& rng) {
        return makeUndifferentiatedOrganism(nextId_++, wx, wz, wy, chaosInitialStorage(rng),
                                            world.tickCount());
      });
}

void CellPopulation::seedMouthOrganisms(const BarrenWorld& world, float cellSize, float heightScale,
                                        int count, std::uint64_t seed, int mouthsPerOrganism) {
  (void)seed;
  if (mouthsPerOrganism <= 0) {
    return;
  }

  seedOnWetTerrain(
      world, cellSize, heightScale, count, kChaosSaltStarMouth, false, 100,
      [this, &world, cellSize, mouthsPerOrganism](float wx, float wz, float wy,
                                                  std::mt19937& rng) {
        Organism organism = makeStarMouthOrganism(
            nextId_++, wx, wz, wy, chaosInitialStorage(rng), world.tickCount(), mouthsPerOrganism,
            nominalBoneLength(cellSize));
        organism.heading = chaosSpawnHeading(rng);
        return organism;
      },
      [&world, cellSize, heightScale](Organism& organism, std::mt19937& rng) {
        (void)rng;
        organism.updateKinematics(world, cellSize, heightScale);
        organism.landAdjacent =
            organismLandAdjacent(world, organism.rootWorldX(), organism.rootWorldZ(), cellSize);
      });
}

void CellPopulation::seedActuatorOrganisms(const BarrenWorld& world, float cellSize,
                                           float heightScale, int count, std::uint64_t seed) {
  (void)seed;
  seedOnWetTerrain(
      world, cellSize, heightScale, count, kChaosSaltActuator, true, 100,
      [this, &world, cellSize](float wx, float wz, float wy, std::mt19937& rng) {
        Organism organism =
            makeActuatorOrganism(nextId_++, wx, wz, wy, chaosInitialStorage(rng), world.tickCount());
        organism.heading = chaosSpawnHeading(rng);
        return organism;
      },
      [&world, cellSize, heightScale](Organism& organism, std::mt19937& rng) {
        (void)rng;
        organism.updateKinematics(world, cellSize, heightScale);
        organism.landAdjacent =
            organismLandAdjacent(world, organism.rootWorldX(), organism.rootWorldZ(), cellSize);
      });
}

void CellPopulation::seedNoms(const BarrenWorld& world, float cellSize, float heightScale, int count,
                              std::uint64_t seed) {
  (void)seed;
  seedOnWetTerrain(
      world, cellSize, heightScale, count, kChaosSaltNom, true, 100,
      [this, &world, cellSize](float wx, float wz, float wy, std::mt19937& rng) {
        Organism organism = makeCampNomOrganism(nextId_++, wx, wz, wy, chaosInitialStorage(rng),
                                            world.tickCount(), nominalBoneLength(cellSize));
        organism.heading = chaosSpawnHeading(rng);
        return organism;
      },
      [&world, cellSize, heightScale](Organism& organism, std::mt19937& rng) {
        (void)rng;
        organism.updateKinematics(world, cellSize, heightScale);
        organism.landAdjacent =
            organismLandAdjacent(world, organism.rootWorldX(), organism.rootWorldZ(), cellSize);
      });
}

void CellPopulation::installFeedbagReproductionOracle(const BarrenWorld& world, float cellSize,
                                                      float heightScale,
                                                      std::uint64_t simTick) {
  (void)world;
  (void)cellSize;
  (void)heightScale;
  for (Organism& organism : organisms_) {
    if (!organism.alive || !organism.isCampNom() || organism.feedbagOracle) {
      continue;
    }
    ::evolab::installFeedbagReproductionOracle(organism, simTick);
    return;
  }
}

void CellPopulation::tick(const BarrenWorld& world, EnergonField& energon, float cellSize,
                          float heightScale, float sunIntensity) {
  const float halfExtent = worldHalfExtent(world, cellSize);
  energon.prepareSpatialQueries(cellSize, halfExtent);
  for (Organism& organism : organisms_) {
    if (organism.feedbagOracle) {
      continue;
    }
    organism.perceive(world, energon, cellSize, halfExtent, organisms_, world.tickCount(),
                      sunIntensity);
  }
  for (Organism& organism : organisms_) {
    tickFeedbagOracleHooks(organism, energon, cellSize);
  }
  for (Organism& organism : organisms_) {
    organism.feed(energon, cellSize, world.tickCount());
  }
  for (Organism& organism : organisms_) {
    organism.runDigestAndComputer(energon, world.tickCount());
  }
  const OrganismTickContext tickCtx{world,     energon,     cellSize,
                                    heightScale, halfExtent, world.tickCount()};
  for (Organism& organism : organisms_) {
    runOrganismPreAdvectHooks(organism, tickCtx);
  }
  for (Organism& organism : organisms_) {
    if (!organism.feedbagOracle) {
      organism.advectRoot(world, energon, cellSize, heightScale, halfExtent);
    }
  }
  for (Organism& organism : organisms_) {
    organism.metabolise(world, cellSize, heightScale);
  }
  for (Organism& organism : organisms_) {
    organism.tickNeuronViability(energon);
  }
  for (Organism& organism : organisms_) {
    organism.tickAxonTransitBasal();
  }
  energon.purgeDepletedBlobs();
  for (Organism& organism : organisms_) {
    organism.transferEnergy(energon, cellSize, world.tickCount());
  }
  tickHgtDockPass(organisms_, cellSize, world.tickCount());
  for (Organism& organism : organisms_) {
    organism.signal(energon, world.tickCount());
  }
  for (Organism& organism : organisms_) {
    organism.pruneNeuralAxons();
  }
  tickParthenogenesisPass(organisms_, world, cellSize, heightScale, world.tickCount(), nextId_);
  for (Organism& organism : organisms_) {
    organism.transferColony();
  }

  organisms_.erase(std::remove_if(organisms_.begin(), organisms_.end(),
                                  [](const Organism& organism) { return !organism.alive; }),
                   organisms_.end());
}

CellPopulationStats CellPopulation::stats() const {
  CellPopulationStats out;
  out.liveCells = static_cast<int>(organisms_.size());
  out.organisms = countOrganisms(organisms_);
  for (const Organism& organism : organisms_) {
    if (organism.isCampNom()) {
      ++out.campNomOrganisms;
      out.mouthNeurons += organism.mouthCount();
      out.skeletonLinks += static_cast<int>(organism.links.size());
      out.neuralAxons += static_cast<int>(organism.neuralAxons.size());
    } else if (organism.hasPerceptorNeurons()) {
      ++out.degradedNomOrganisms;
      out.skeletonLinks += static_cast<int>(organism.links.size());
      out.neuralAxons += static_cast<int>(organism.neuralAxons.size());
    } else if (organism.hasActuatorNeurons()) {
      ++out.actuatorOrganisms;
    } else if (!organism.hasMouthNeurons() && !organism.hasActuatorNeurons() &&
               !organism.hasPerceptorNeurons()) {
      ++out.stemCells;
    } else {
      ++out.mouthOrganisms;
      out.mouthNeurons += organism.mouthCount();
      out.skeletonLinks += static_cast<int>(organism.links.size());
      out.neuralAxons += static_cast<int>(organism.neuralAxons.size());
    }
  }
  return out;
}

const Organism* CellPopulation::findById(std::uint32_t id) const {
  for (const Organism& organism : organisms_) {
    if (organism.id == id) {
      return &organism;
    }
  }
  return nullptr;
}

}  // namespace evolab
