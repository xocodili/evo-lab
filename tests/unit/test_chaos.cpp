#include "sim/BarrenWorld.hpp"
#include "sim/CellConstants.hpp"
#include "sim/CellPopulation.hpp"
#include "sim/Chaos.hpp"
#include "sim/NeuralAxon.hpp"
#include "sim/WorldConstants.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <random>

TEST_CASE("chaos jitter rate matches design notes", "[chaos]") {
  REQUIRE(evolab::kChaosJitterRate == Catch::Approx(0.03f));
  REQUIRE(evolab::kEpsilonRandomChild == Catch::Approx(0.03f));
  REQUIRE(evolab::kMisalignmentRate == Catch::Approx(0.03f));
}

TEST_CASE("chaos jitter multiplier stays within plus minus three percent", "[chaos]") {
  std::mt19937 rng(12345);
  for (int i = 0; i < 200; ++i) {
    const float mult = evolab::chaosJitterMultiplier(rng);
    REQUIRE(mult >= 0.97f);
    REQUIRE(mult <= 1.03f);
  }
}

TEST_CASE("developmental axon trust initializes near baseline", "[chaos]") {
  std::mt19937 rng(99);
  evolab::NeuralAxon axon;
  evolab::initializeDevelopmentalAxonTrust(axon, rng);

  const float feedScale = evolab::axonTrustScale(axon.trustFeed);
  const float believeScale = evolab::axonTrustScale(axon.trustBelieve);
  const float minFeedScale =
      static_cast<float>(evolab::kTrustMin) / static_cast<float>(evolab::kTrustBaseline);
  REQUIRE(feedScale == Catch::Approx(minFeedScale).margin(0.04f));
  REQUIRE(believeScale == Catch::Approx(1.0f).margin(0.04f));
  REQUIRE(axon.trustFeed >= evolab::kTrustMin);
  REQUIRE(axon.trustFeed <= evolab::kTrustMax);
  REQUIRE(axon.etaSignal == Catch::Approx(evolab::kDefaultNeuralAxonEta).margin(0.04f));
}

TEST_CASE("axon marked for pruning only when both trusts are zero", "[chaos]") {
  evolab::NeuralAxon axon;
  axon.trustBelieve = 100;
  axon.trustFeed = 0;
  REQUIRE_FALSE(evolab::axonMarkedForPruning(axon));
  axon.trustBelieve = 0;
  REQUIRE(evolab::axonMarkedForPruning(axon));
}

TEST_CASE("chaos initial storage stays in one to three day band", "[chaos]") {
  std::mt19937 rng(4242);
  for (int i = 0; i < 100; ++i) {
    const std::uint32_t bytes = evolab::chaosInitialStorage(rng);
    REQUIRE(bytes >= evolab::kTicksPerStemCellDay);
    REQUIRE(bytes <= evolab::kStemCellStorageMaxBytes);
  }
}

TEST_CASE("bone length receives a single chaos jitter at spawn", "[chaos]") {
  evolab::BarrenWorld world(11, 64);
  evolab::CellPopulation population;
  population.seedTwoMouthOrganisms(world, evolab::kWorldCellSize, evolab::kTerrainHeightScale, 12,
                                   77);

  const float nominal = evolab::nominalBoneLength(evolab::kWorldCellSize);
  for (const evolab::Organism& organism : population.organisms()) {
    REQUIRE(organism.links.size() == 1);
    const float ratio = organism.links[0].restLength / nominal;
    REQUIRE(ratio >= 0.97f);
    REQUIRE(ratio <= 1.03f);
  }
}
