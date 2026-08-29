#include "sim/BarrenWorld.hpp"
#include "sim/CampTopology.hpp"
#include "sim/CellConstants.hpp"
#include "sim/Energon.hpp"
#include "sim/EnergonString.hpp"
#include "sim/NeuronTick.hpp"
#include "sim/Organism.hpp"
#include "sim/Tide.hpp"
#include "sim/WorldConstants.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <cstdint>

namespace {

float worldHalfExtent(const evolab::BarrenWorld& world, float cellSize) {
  const int res = world.heightmap().resolution;
  if (res <= 1 || cellSize <= 0.0f) {
    return 0.0f;
  }
  return static_cast<float>(res - 1) * cellSize * 0.5f;
}

bool isDeepWet(const evolab::BarrenWorld& world, float wx, float wz, float cellSize,
               float minDepth) {
  return world.depthAtWorld(wx, wz, cellSize) >= minDepth;
}

bool findWetSite(const evolab::BarrenWorld& world, float cellSize, float minDepth, float& wx,
                 float& wz) {
  const float halfExtent = worldHalfExtent(world, cellSize);
  const float step = cellSize * 2.0f;
  for (float x = -halfExtent; x <= halfExtent; x += step) {
    for (float z = -halfExtent; z <= halfExtent; z += step) {
      if (isDeepWet(world, x, z, cellSize, minDepth)) {
        wx = x;
        wz = z;
        return true;
      }
    }
  }
  return false;
}

evolab::EnergonConfig nurseryFieldConfig() {
  evolab::EnergonConfig config;
  config.spawnRateMax = 0.0f;
  config.populationScaledRain = false;
  config.maxBlobs = 32;
  return config;
}

void tickNurseryOrganism(evolab::Organism& organism, evolab::BarrenWorld& world,
                         evolab::EnergonField& energon, float sunIntensity) {
  const float cellSize = evolab::kWorldCellSize;
  const float heightScale = evolab::kTerrainHeightScale;
  const float halfExtent = worldHalfExtent(world, cellSize);
  energon.prepareSpatialQueries(cellSize, halfExtent);
  organism.perceive(world, energon, cellSize, halfExtent, {organism}, world.tickCount(),
                    sunIntensity);
  organism.feed(energon, cellSize, world.tickCount());
  organism.runDigestAndComputer(energon, world.tickCount());
  const evolab::OrganismTickContext ctx{world, energon, cellSize, heightScale, halfExtent,
                                        world.tickCount()};
  evolab::runOrganismPreAdvectHooks(organism, ctx);
  if (!organism.disableNurseryLocomotion) {
    organism.advectRoot(world, energon, cellSize, heightScale, halfExtent);
  }
  organism.metabolise(world, cellSize, heightScale);
  organism.tickNeuronViability(energon);
  organism.tickAxonTransitBasal();
  energon.purgeDepletedBlobs();
  organism.transferEnergy(energon, cellSize, world.tickCount());
  organism.signal(energon, world.tickCount());
  organism.pruneNeuralAxons();
  world.tick();
  energon.tick(world, sunIntensity, cellSize, heightScale);
}

int countNeuronType(const evolab::Organism& organism, evolab::NeuronType type) {
  int count = 0;
  for (const evolab::SkeletonNode& node : organism.nodes) {
    if (node.alive && node.neuron == type) {
      ++count;
    }
  }
  return count;
}

}  // namespace

TEST_CASE("nursery random camp mutants tick without code faults", "[nursery][mutant]") {
  evolab::TideConfig tideConfig;
  tideConfig.amplitude = 0.0f;
  constexpr int kMutantSeeds = 32;
  constexpr int kNurseryTicks = 1024;
  constexpr float kMinDepth = 0.35f;

  int multiPerceptorCases = 0;

  for (int seed = 0; seed < kMutantSeeds; ++seed) {
    evolab::BarrenWorld world(31, 32, evolab::Tide(tideConfig));
    evolab::EnergonField energon(static_cast<std::uint32_t>(1000 + seed), nurseryFieldConfig());

    float wx = 0.0f;
    float wz = 0.0f;
    REQUIRE(findWetSite(world, evolab::kWorldCellSize, kMinDepth, wx, wz));

    evolab::Organism mutant = evolab::makeRandomCampMutant(
        1, wx, wz, 1.0f, evolab::kTicksPerStemCellDay, 0, evolab::kWorldCellSize,
        static_cast<std::uint64_t>(seed));
    mutant.alive = true;
    mutant.disableTideAdvection = true;
    mutant.disableTerrainThreatScan = true;
    mutant.disableNurseryLocomotion = true;
    mutant.updateKinematics(world, evolab::kWorldCellSize, evolab::kTerrainHeightScale);

    const int perceptorCount = countNeuronType(mutant, evolab::NeuronType::Perceptor);
    if (perceptorCount >= 2) {
      ++multiPerceptorCases;
    }

    energon.injectBlob(evolab::makeCornucopiaBlob(wx, wz, static_cast<std::uint8_t>(seed)));
    REQUIRE(energon.activeCount() == 1);

    const std::string genotype = evolab::campGenotypeLabel(mutant);
    const std::size_t initialAxons = mutant.neuralAxons.size();
    INFO("seed=" << seed << " genotype=" << genotype << " nodes=" << mutant.nodes.size()
                 << " axons=" << initialAxons << " perceptors=" << perceptorCount
                 << " campPhases=" << evolab::organismUsesCampNeuronPhases(mutant)
                 << " canonical=" << evolab::organismHasCampTopology(mutant));

    REQUIRE(mutant.nodes.size() >= 4);
    REQUIRE(initialAxons >= 6);

    for (int tick = 0; tick < kNurseryTicks; ++tick) {
      tickNurseryOrganism(mutant, world, energon, 1.0f);
    }

    INFO("alive=" << mutant.alive << " hubBytes=" << mutant.bodyStorage.size()
                  << " axonsRemaining=" << mutant.neuralAxons.size());
    if (perceptorCount >= 2) {
      INFO("multi-eye abomination seed=" << seed << " genotype=" << genotype
                                         << " perceptors=" << perceptorCount
                                         << " alive=" << mutant.alive);
    }
  }

  INFO("multiPerceptorCases=" << multiPerceptorCases);
  REQUIRE(multiPerceptorCases >= 1);
}
