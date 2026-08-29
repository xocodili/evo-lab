#include "sim/BarrenWorld.hpp"
#include "sim/CampTopology.hpp"
#include "sim/CellConstants.hpp"
#include "sim/CellPopulation.hpp"
#include "sim/Energon.hpp"
#include "sim/NeuralAxon.hpp"
#include "sim/Organism.hpp"
#include "sim/OrganismFeedbagOracle.hpp"
#include "sim/OrganismParthenogenesis.hpp"
#include "sim/WorldConstants.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cmath>

namespace {

bool findWetSite(const evolab::BarrenWorld& world, float cellSize, float& wx, float& wz) {
  const int res = world.heightmap().resolution;
  const float half = static_cast<float>(res - 1) * cellSize * 0.5f;
  for (float x = -half; x <= half; x += cellSize * 0.5f) {
    for (float z = -half; z <= half; z += cellSize * 0.5f) {
      if (world.isWetWorld(x, z, cellSize)) {
        wx = x;
        wz = z;
        return true;
      }
    }
  }
  return false;
}

evolab::Organism makeWealthyParent(float wx, float wz, std::uint64_t simTick) {
  evolab::Organism parent = evolab::makeCampNomOrganism(
      1, wx, wz, 1.0f, evolab::kStemCellStorageMaxBytes, 0, evolab::kWorldCellSize);
  parent.alive = true;
  parent.createdAtTick = 0;
  parent.bodyStorage.assign(evolab::estimateParthenogenesisRequiredHubBytes() + 50'000, 1);
  parent.heading = 0.0f;
  for (evolab::SkeletonNode& node : parent.nodes) {
    node.worldX = wx;
    node.worldZ = wz;
  }
  return parent;
}

bool axonParamsDiffer(const evolab::Organism& parent, const evolab::Organism& child) {
  const evolab::NeuralAxon* p = parent.findNeuralAxon(evolab::kCampPerceptorId, evolab::kCampMouthId);
  const evolab::NeuralAxon* c = child.findNeuralAxon(evolab::kCampPerceptorId, evolab::kCampMouthId);
  REQUIRE(p != nullptr);
  REQUIRE(c != nullptr);
  return p->etaSignal != c->etaSignal || p->trustFeed != c->trustFeed ||
         p->trustBelieveByConfidence != c->trustBelieveByConfidence;
}

bool withinBinomialTolerance(int successes, int trials, float expectedRate, float sigmaBand) {
  if (expectedRate >= 1.0f) {
    return successes == trials;
  }
  if (expectedRate <= 0.0f) {
    return successes == 0;
  }
  const float mean = static_cast<float>(trials) * expectedRate;
  const float stddev =
      std::sqrt(static_cast<float>(trials) * expectedRate * (1.0f - expectedRate));
  return std::abs(static_cast<float>(successes) - mean) <= sigmaBand * stddev;
}

evolab::ParthenogenesisResult runBirthAttempt(evolab::Organism& parent,
                                            const evolab::BarrenWorld& world, float cellSize,
                                            std::uint64_t simTick, std::uint32_t& nextId,
                                            const evolab::ParthenogenesisPassOptions& options) {
  return evolab::attemptParthenogenesis(parent, world, cellSize, evolab::kTerrainHeightScale,
                                        simTick, nextId, options);
}

bool findDrySite(const evolab::BarrenWorld& world, float cellSize, float& wx, float& wz) {
  const int res = world.heightmap().resolution;
  const float half = static_cast<float>(res - 1) * cellSize * 0.5f;
  for (float x = -half; x <= half; x += cellSize * 0.5f) {
    for (float z = -half; z <= half; z += cellSize * 0.5f) {
      if (!world.isWetWorld(x, z, cellSize)) {
        wx = x;
        wz = z;
        return true;
      }
    }
  }
  return false;
}

bool hasAllDevelopmentalAxons(const evolab::Organism& organism) {
  for (const auto& edge : evolab::kCampDevelopmentalAxons) {
    if (organism.findNeuralAxon(edge.first, edge.second) == nullptr) {
      return false;
    }
  }
  return true;
}

std::size_t totalOrganismFuel(const evolab::Organism& organism) {
  std::size_t total = organism.bodyStorage.size();
  for (const evolab::SkeletonNode& node : organism.nodes) {
    total += node.store.size();
  }
  return total;
}

const evolab::Organism* findNewbornChild(const std::vector<evolab::Organism>& population,
                                         std::uint32_t parentId, std::uint64_t minCreatedTick) {
  for (const evolab::Organism& organism : population) {
    if (organism.id == parentId) {
      continue;
    }
    if (organism.createdAtTick >= minCreatedTick && evolab::campGenotypeValid(organism)) {
      return &organism;
    }
  }
  return nullptr;
}

}  // namespace

TEST_CASE("wealthy aged parent spawns faithful camp child", "[parthenogenesis][birth_rub]") {
  evolab::BarrenWorld world(42, 64);
  float wx = 0.0f;
  float wz = 0.0f;
  REQUIRE(findWetSite(world, evolab::kWorldCellSize, wx, wz));

  evolab::Organism parent = makeWealthyParent(wx, wz, 1000);
  const std::size_t hubBefore = parent.bodyStorage.size();
  std::uint32_t nextId = 2;

  evolab::ParthenogenesisPassOptions options;
  options.structuralRateOverride = 0.0f;
  options.skipEligibilityChecks = true;

  const evolab::ParthenogenesisResult result =
      runBirthAttempt(parent, world, evolab::kWorldCellSize, 1000, nextId, options);

  REQUIRE(result.spawned);
  REQUIRE(result.child.alive);
  REQUIRE(result.child.isCampNom());
  REQUIRE(result.bytesSpent == evolab::estimateParthenogenesisCostCamp());
  REQUIRE(parent.bodyStorage.size() + result.bytesSpent == hubBefore);
  REQUIRE(parent.lastParthenogenesisSpawned);
  REQUIRE(parent.offspringSpawnedCount == 1);
  REQUIRE(axonParamsDiffer(parent, result.child));
}

TEST_CASE("young parent cannot spawn", "[parthenogenesis]") {
  evolab::BarrenWorld world(42, 64);
  float wx = 0.0f;
  float wz = 0.0f;
  REQUIRE(findWetSite(world, evolab::kWorldCellSize, wx, wz));

  evolab::Organism parent = makeWealthyParent(wx, wz, 100);
  parent.createdAtTick = 900;
  std::uint32_t nextId = 2;

  const evolab::ParthenogenesisResult result =
      runBirthAttempt(parent, world, evolab::kWorldCellSize, 1000, nextId, {});

  REQUIRE_FALSE(result.spawned);
  REQUIRE(result.bytesSpent == 0);
}

TEST_CASE("insolvent parent aborts without spawn", "[parthenogenesis]") {
  evolab::BarrenWorld world(42, 64);
  float wx = 0.0f;
  float wz = 0.0f;
  REQUIRE(findWetSite(world, evolab::kWorldCellSize, wx, wz));

  evolab::Organism parent = makeWealthyParent(wx, wz, 1000);
  parent.bodyStorage.assign(evolab::kParthenogenesisInitCost + 100, 1);
  std::uint32_t nextId = 2;

  evolab::ParthenogenesisPassOptions options;
  options.skipEligibilityChecks = true;

  const evolab::ParthenogenesisResult result =
      runBirthAttempt(parent, world, evolab::kWorldCellSize, 1000, nextId, options);

  REQUIRE_FALSE(result.spawned);
  REQUIRE(result.aborted);
  REQUIRE(result.bytesSpent > 0);
  REQUIRE(result.bytesSpent < evolab::estimateParthenogenesisCostCamp());
}

TEST_CASE("parthenogenesis structural rate calibration", "[parthenogenesis][birth_rub]") {
  evolab::BarrenWorld world(42, 64);
  float wx = 0.0f;
  float wz = 0.0f;
  REQUIRE(findWetSite(world, evolab::kWorldCellSize, wx, wz));

  SECTION("0% structural rate preserves developmental axon count") {
    evolab::Organism parent = makeWealthyParent(wx, wz, 2000);
    std::uint32_t nextId = 100;
    evolab::ParthenogenesisPassOptions options;
    options.structuralRateOverride = 0.0f;
    options.skipEligibilityChecks = true;
    const evolab::ParthenogenesisResult result =
        runBirthAttempt(parent, world, evolab::kWorldCellSize, 2000, nextId, options);
    REQUIRE(result.spawned);
    REQUIRE(result.child.neuralAxons.size() == parent.neuralAxons.size());
  }

  SECTION("100% structural rate still spawns viable camp children in rub trial") {
    constexpr int kTrials = 16;
    int successes = 0;
    for (int trial = 0; trial < kTrials; ++trial) {
      evolab::Organism parent = makeWealthyParent(wx, wz, static_cast<std::uint64_t>(3000 + trial));
      std::uint32_t nextId = static_cast<std::uint32_t>(500 + trial);
      evolab::ParthenogenesisPassOptions options;
      options.structuralRateOverride = 1.0f;
      options.skipEligibilityChecks = true;
      const evolab::ParthenogenesisResult result = runBirthAttempt(
          parent, world, evolab::kWorldCellSize, static_cast<std::uint64_t>(3000 + trial), nextId,
          options);
      if (result.spawned && evolab::campGenotypeValid(result.child)) {
        ++successes;
      }
    }
    REQUIRE(successes == kTrials);
  }

  SECTION("100% structural rate often yields non-PMCA freak genotypes") {
    constexpr int kTrials = 32;
    int freakSpawns = 0;
    for (int trial = 0; trial < kTrials; ++trial) {
      evolab::Organism parent = makeWealthyParent(wx, wz, static_cast<std::uint64_t>(3500 + trial));
      std::uint32_t nextId = static_cast<std::uint32_t>(600 + trial);
      evolab::ParthenogenesisPassOptions options;
      options.structuralRateOverride = 1.0f;
      options.skipEligibilityChecks = true;
      const evolab::ParthenogenesisResult result = runBirthAttempt(
          parent, world, evolab::kWorldCellSize, static_cast<std::uint64_t>(3500 + trial), nextId,
          options);
      if (result.spawned && evolab::campGenotypeValid(result.child) && !result.child.isCampNom()) {
        ++freakSpawns;
      }
    }
    REQUIRE(freakSpawns >= 12);
  }

  SECTION("default structural rate yields occasional freak genotypes") {
    constexpr int kTrials = 128;
    int freakSpawns = 0;
    int canonicalSpawns = 0;
    for (int trial = 0; trial < kTrials; ++trial) {
      evolab::Organism parent = makeWealthyParent(wx, wz, static_cast<std::uint64_t>(4000 + trial));
      std::uint32_t nextId = static_cast<std::uint32_t>(700 + trial);
      evolab::ParthenogenesisPassOptions options;
      options.skipEligibilityChecks = true;
      const evolab::ParthenogenesisResult result = runBirthAttempt(
          parent, world, evolab::kWorldCellSize, static_cast<std::uint64_t>(4000 + trial), nextId,
          options);
      if (!result.spawned || !evolab::campGenotypeValid(result.child)) {
        continue;
      }
      if (result.child.isCampNom()) {
        ++canonicalSpawns;
      } else {
        ++freakSpawns;
      }
    }
    REQUIRE(canonicalSpawns + freakSpawns == kTrials);
    REQUIRE(freakSpawns >= 3);
  }
}

TEST_CASE("population tick can increase camp nom count via parthenogenesis",
          "[parthenogenesis]") {
  evolab::BarrenWorld world(42, 64);
  float wx = 0.0f;
  float wz = 0.0f;
  REQUIRE(findWetSite(world, evolab::kWorldCellSize, wx, wz));

  std::vector<evolab::Organism> population;
  population.push_back(makeWealthyParent(wx, wz, 5000));
  std::uint32_t nextId = 2;

  evolab::ParthenogenesisPassOptions options;
  options.structuralRateOverride = 0.0f;
  options.skipEligibilityChecks = true;

  tickParthenogenesisPass(population, world, evolab::kWorldCellSize, evolab::kTerrainHeightScale,
                          5000, nextId, options);

  REQUIRE(population.size() == 2);
  REQUIRE(population[0].offspringSpawnedCount == 1);
  REQUIRE(population[1].isCampNom());
}

TEST_CASE("feedbag reproduction oracle spawns by tick 150", "[parthenogenesis][feedbag]") {
  evolab::BarrenWorld world(42, 64);
  evolab::EnergonField energon(42, {});
  evolab::CellPopulation population;
  population.seedNoms(world, evolab::kWorldCellSize, evolab::kTerrainHeightScale, 40, 42);
  const std::size_t seededCount = population.organisms().size();
  REQUIRE(seededCount >= 1);

  population.installFeedbagReproductionOracle(world, evolab::kWorldCellSize,
                                            evolab::kTerrainHeightScale, 0);

  const evolab::Organism* oracle = nullptr;
  for (const evolab::Organism& organism : population.organisms()) {
    if (organism.feedbagOracle) {
      oracle = &organism;
      break;
    }
  }
  REQUIRE(oracle != nullptr);
  const std::uint32_t oracleId = oracle->id;

  for (int tick = 0; tick < 150; ++tick) {
    world.tick();
    population.tick(world, energon, evolab::kWorldCellSize, evolab::kTerrainHeightScale, 0.5f);
  }

  const evolab::Organism* parent = population.findById(oracleId);
  REQUIRE(parent != nullptr);
  REQUIRE(parent->alive);
  REQUIRE(evolab::campGenotypeValid(*parent));
  REQUIRE(parent->offspringSpawnedCount == 1);
  REQUIRE(parent->parthenogenesisCelebrationStartTick != 0);
  REQUIRE(population.organisms().size() == seededCount + 1);
}

TEST_CASE("eligible wealthy parent spawns with production eligibility checks",
          "[parthenogenesis][integration]") {
  evolab::BarrenWorld world(42, 64);
  float wx = 0.0f;
  float wz = 0.0f;
  REQUIRE(findWetSite(world, evolab::kWorldCellSize, wx, wz));

  evolab::Organism parent = makeWealthyParent(wx, wz, 0);
  parent.createdAtTick = 0;
  std::uint32_t nextId = 2;

  REQUIRE(evolab::eligibleForParthenogenesis(parent, world, evolab::kWorldCellSize, 1000));

  const evolab::ParthenogenesisResult result =
      runBirthAttempt(parent, world, evolab::kWorldCellSize, 1000, nextId, {});

  REQUIRE(result.spawned);
  REQUIRE(result.childId == 2);
  REQUIRE(nextId == 3);
  REQUIRE(evolab::organismHasCampTopology(result.child));
}

TEST_CASE("dry land parent fails production eligibility", "[parthenogenesis]") {
  evolab::BarrenWorld world(42, 64);
  float wx = 0.0f;
  float wz = 0.0f;
  REQUIRE(findDrySite(world, evolab::kWorldCellSize, wx, wz));

  evolab::Organism parent = makeWealthyParent(wx, wz, 0);
  for (evolab::SkeletonNode& node : parent.nodes) {
    node.worldX = wx;
    node.worldZ = wz;
  }
  std::uint32_t nextId = 2;

  REQUIRE_FALSE(
      evolab::eligibleForParthenogenesis(parent, world, evolab::kWorldCellSize, 1000));

  const evolab::ParthenogenesisResult result =
      runBirthAttempt(parent, world, evolab::kWorldCellSize, 1000, nextId, {});

  REQUIRE_FALSE(result.spawned);
}

TEST_CASE("basal arrears block production eligibility", "[parthenogenesis]") {
  evolab::BarrenWorld world(42, 64);
  float wx = 0.0f;
  float wz = 0.0f;
  REQUIRE(findWetSite(world, evolab::kWorldCellSize, wx, wz));

  evolab::Organism parent = makeWealthyParent(wx, wz, 0);
  parent.nodes.front().basalArrearsTicks =
      static_cast<std::uint16_t>(evolab::kNeuronBasalGraceTicks);
  std::uint32_t nextId = 2;

  REQUIRE_FALSE(
      evolab::eligibleForParthenogenesis(parent, world, evolab::kWorldCellSize, 1000));

  const evolab::ParthenogenesisResult result =
      runBirthAttempt(parent, world, evolab::kWorldCellSize, 1000, nextId, {});

  REQUIRE_FALSE(result.spawned);
}

TEST_CASE("refractory prevents second spawn within cooldown", "[parthenogenesis]") {
  evolab::BarrenWorld world(42, 64);
  float wx = 0.0f;
  float wz = 0.0f;
  REQUIRE(findWetSite(world, evolab::kWorldCellSize, wx, wz));

  evolab::Organism parent = makeWealthyParent(wx, wz, 0);
  parent.createdAtTick = 0;
  std::uint32_t nextId = 2;

  evolab::ParthenogenesisPassOptions options;
  options.structuralRateOverride = 0.0f;
  options.skipEligibilityChecks = true;

  const evolab::ParthenogenesisResult first =
      runBirthAttempt(parent, world, evolab::kWorldCellSize, 5000, nextId, options);
  REQUIRE(first.spawned);
  REQUIRE(parent.offspringSpawnedCount == 1);
  REQUIRE(parent.lastParthenogenesisSuccessTick == 5000);

  parent.bodyStorage.assign(evolab::estimateParthenogenesisRequiredHubBytes() + 50'000, 1);

  const evolab::ParthenogenesisResult second =
      runBirthAttempt(parent, world, evolab::kWorldCellSize,
                      5000 + evolab::kParthenogenesisRefractoryTicks - 1, nextId, {});

  REQUIRE_FALSE(second.spawned);
  REQUIRE(parent.offspringSpawnedCount == 1);
}

TEST_CASE("spawned child is a viable camp lifeform", "[parthenogenesis][birth_rub]") {
  evolab::BarrenWorld world(42, 64);
  float wx = 0.0f;
  float wz = 0.0f;
  REQUIRE(findWetSite(world, evolab::kWorldCellSize, wx, wz));

  evolab::Organism parent = makeWealthyParent(wx, wz, 0);
  parent.heading = 1.2f;
  std::uint32_t nextId = 77;
  constexpr std::uint64_t kBirthTick = 9000;

  evolab::ParthenogenesisPassOptions options;
  options.structuralRateOverride = 0.0f;
  options.skipEligibilityChecks = true;

  const evolab::ParthenogenesisResult result =
      runBirthAttempt(parent, world, evolab::kWorldCellSize, kBirthTick, nextId, options);

  REQUIRE(result.spawned);
  REQUIRE(result.childId == 77);
  REQUIRE(nextId == 78);

  const evolab::Organism& child = result.child;
  REQUIRE(child.alive);
  REQUIRE(child.isCampNom());
  REQUIRE(evolab::campGenotypeValid(child));
  REQUIRE(evolab::organismHasCampTopology(child));
  REQUIRE(hasAllDevelopmentalAxons(child));
  REQUIRE_FALSE(child.feedbagOracle);
  REQUIRE(child.createdAtTick == kBirthTick);
  REQUIRE(child.id == 77);
  REQUIRE(totalOrganismFuel(child) == evolab::kParthenogenesisChildEndowmentBytes);
  REQUIRE(child.nodes.size() == 4);
  REQUIRE(child.mouthCount() == 1);
  REQUIRE(child.perceptorCount() == 1);
  REQUIRE(child.actuatorCount() == 1);
  REQUIRE(std::abs(child.rootWorldX() - wx) + std::abs(child.rootWorldZ() - wz) > 0.01f);

  REQUIRE(parent.parthenogenesisCelebrationStartTick == kBirthTick);
  REQUIRE(parent.parthenogenesisBirthHeading == parent.heading);
  REQUIRE(parent.lastParthenogenesisSpawned);
}

TEST_CASE("sixty nom seed oracle yields child organism id sixty one",
          "[parthenogenesis][feedbag][integration]") {
  evolab::BarrenWorld world(42, 64);
  evolab::EnergonField energon(42, {});
  evolab::CellPopulation population;
  population.seedNoms(world, evolab::kWorldCellSize, evolab::kTerrainHeightScale, 60, 42);
  REQUIRE(population.organisms().size() == 60);

  population.installFeedbagReproductionOracle(world, evolab::kWorldCellSize,
                                            evolab::kTerrainHeightScale, 0);

  int oracleCount = 0;
  for (const evolab::Organism& organism : population.organisms()) {
    if (organism.feedbagOracle) {
      ++oracleCount;
    }
  }
  REQUIRE(oracleCount == 1);

  for (int tick = 0; tick < 150; ++tick) {
    world.tick();
    population.tick(world, energon, evolab::kWorldCellSize, evolab::kTerrainHeightScale, 0.5f);
  }

  const evolab::Organism* child = population.findById(61);
  REQUIRE(child != nullptr);
  REQUIRE(child->alive);
  REQUIRE(evolab::campGenotypeValid(*child));
  REQUIRE_FALSE(child->feedbagOracle);
  REQUIRE(hasAllDevelopmentalAxons(*child));
  REQUIRE(population.organisms().size() == 61);
}

TEST_CASE("newborn survives full population tick loop after oracle birth",
          "[parthenogenesis][integration]") {
  evolab::BarrenWorld world(42, 64);
  evolab::EnergonField energon(42, {});
  evolab::CellPopulation population;
  population.seedNoms(world, evolab::kWorldCellSize, evolab::kTerrainHeightScale, 20, 42);
  population.installFeedbagReproductionOracle(world, evolab::kWorldCellSize,
                                              evolab::kTerrainHeightScale, 0);

  for (int tick = 0; tick < 150; ++tick) {
    world.tick();
    population.tick(world, energon, evolab::kWorldCellSize, evolab::kTerrainHeightScale, 0.5f);
  }

  const evolab::Organism* child = findNewbornChild(population.organisms(), 1, 1);
  REQUIRE(child != nullptr);
  const std::uint32_t childId = child->id;

  for (int tick = 0; tick < 400; ++tick) {
    world.tick();
    population.tick(world, energon, evolab::kWorldCellSize, evolab::kTerrainHeightScale, 0.75f);
  }

  const evolab::Organism* survivor = population.findById(childId);
  REQUIRE(survivor != nullptr);
  REQUIRE(survivor->alive);
  REQUIRE(evolab::campGenotypeValid(*survivor));
  REQUIRE(evolab::organismHasCampTopology(*survivor));
  REQUIRE(totalOrganismFuel(*survivor) > 0);
}
