#include "sim/BarrenWorld.hpp"
#include "sim/CellConstants.hpp"
#include "sim/CellPopulation.hpp"
#include "sim/EnergonString.hpp"
#include "sim/Organism.hpp"
#include "sim/WorldConstants.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>

namespace {

evolab::EnergonBlob makeWetFoodBlob(float x, float z, std::uint8_t bytes, std::uint32_t id) {
  evolab::EnergonBlob blob;
  blob.id = id;
  blob.initialBytes = bytes;
  blob.remaining = bytes;
  blob.data = static_cast<std::uint64_t>(0xB0);
  blob.x = x;
  blob.z = z;
  blob.y = 1.0f;
  blob.grounded = true;
  blob.onWet = true;
  blob.ttl = 40.0f;
  evolab::energonBlobInitPoint(blob);
  return blob;
}

}  // namespace

TEST_CASE("two mouth factory places second mouth off origin before kinematics", "[twomouth]") {
  evolab::Organism organism =
      evolab::makeTwoMouthOrganism(1, 10.0f, 20.0f, 1.0f, 100, 0, 1.5f);
  const evolab::SkeletonNode* m2 = organism.findNode(2);
  REQUIRE(m2 != nullptr);
  REQUIRE(m2->worldX == Catch::Approx(10.0f).margin(0.01f));
  REQUIRE(m2->worldZ == Catch::Approx(21.5f).margin(0.01f));
}

TEST_CASE("two mouth organism has dumbbell bone and reciprocal neural axons", "[twomouth]") {
  evolab::Organism organism =
      evolab::makeTwoMouthOrganism(1, 0.0f, 0.0f, 1.0f, 100, 0, evolab::kWorldCellSize);

  REQUIRE(organism.nodes.size() == 2);
  REQUIRE(organism.mouthCount() == 2);
  REQUIRE(organism.links.size() == 1);
  REQUIRE(organism.neuralAxons.size() == 2);
  REQUIRE(organism.links[0].energyEta == 0.0f);
  REQUIRE(organism.findNeuralAxon(1, 2) != nullptr);
  REQUIRE(organism.findNeuralAxon(2, 1) != nullptr);
}

TEST_CASE("two mouth kinematics places mouths at bone length apart", "[twomouth]") {
  evolab::BarrenWorld world(1, 32);
  evolab::Organism organism =
      evolab::makeTwoMouthOrganism(1, 0.0f, 0.0f, 1.0f, 100, 0, 1.0f);
  organism.updateKinematics(world, evolab::kWorldCellSize, evolab::kTerrainHeightScale);

  const evolab::SkeletonNode* m1 = organism.findNode(1);
  const evolab::SkeletonNode* m2 = organism.findNode(2);
  REQUIRE(m1 != nullptr);
  REQUIRE(m2 != nullptr);
  const float dx = m2->worldX - m1->worldX;
  const float dz = m2->worldZ - m1->worldZ;
  const float dist = std::sqrt(dx * dx + dz * dz);
  REQUIRE(dist == Catch::Approx(1.0f).margin(0.05f));
}

TEST_CASE("mouth bite emits byte signal on outbound neural axon", "[twomouth]") {
  evolab::BarrenWorld world(1, 32);
  evolab::EnergonField field(1, {});
  evolab::Organism organism =
      evolab::makeTwoMouthOrganism(1, 0.0f, 0.0f, 1.0f, 100, 0, evolab::kWorldCellSize);
  organism.updateKinematics(world, evolab::kWorldCellSize, evolab::kTerrainHeightScale);

  evolab::SkeletonNode* m1 = organism.findNode(1);
  REQUIRE(m1 != nullptr);
  field.injectBlob(makeWetFoodBlob(m1->worldX, m1->worldZ, 3, 1));

  organism.feed(field, evolab::kWorldCellSize);
  organism.signal(42);

  const evolab::NeuralAxon* axon12 = organism.findNeuralAxon(1, 2);
  REQUIRE(axon12 != nullptr);
  REQUIRE(axon12->lastSentByte == 0xB0);
  REQUIRE(axon12->lastReceived.valid);
  REQUIRE(axon12->lastReceived.byte == 0xB0);
}

TEST_CASE("neural axon transfers store bytes when trust feed allows", "[twomouth]") {
  evolab::Organism organism =
      evolab::makeTwoMouthOrganism(1, 0.0f, 0.0f, 1.0f, 100, 0, evolab::kWorldCellSize, 320, 320);
  evolab::SkeletonNode* m1 = organism.findNode(1);
  evolab::SkeletonNode* m2 = organism.findNode(2);
  REQUIRE(m1 != nullptr);
  REQUIRE(m2 != nullptr);

  for (int i = 0; i < 10; ++i) {
    m1->store.push_back(static_cast<std::uint8_t>(0xA0 + i));
  }

  evolab::EnergonField field(1, {});
  organism.transferEnergy(field, evolab::kWorldCellSize);

  REQUIRE(m1->store.size() < 10);
  REQUIRE(!m2->store.empty());
}

TEST_CASE("population seeds twin mouth organisms with two axons each", "[twomouth]") {
  evolab::BarrenWorld world(3, 64);
  evolab::CellPopulation population;
  population.seedTwoMouthOrganisms(world, evolab::kWorldCellSize, evolab::kTerrainHeightScale, 5,
                                   99);

  const evolab::CellPopulationStats stats = population.stats();
  REQUIRE(stats.mouthOrganisms == 5);
  REQUIRE(stats.mouthNeurons == 10);
  REQUIRE(stats.neuralAxons == 10);
  REQUIRE(!population.organisms().empty());
  REQUIRE(population.organisms().front().neuralAxons.size() == 2);
}

TEST_CASE("spawn initializes axon trust near 100 percent with jitter", "[twomouth]") {
  evolab::BarrenWorld world(7, 64);
  evolab::CellPopulation population;
  population.seedTwoMouthOrganisms(world, evolab::kWorldCellSize, evolab::kTerrainHeightScale, 8,
                                   4242);

  for (const evolab::Organism& organism : population.organisms()) {
    for (const evolab::NeuralAxon& axon : organism.neuralAxons) {
      const float feed = evolab::axonTrustScale(axon.trustFeed);
      const float believe = evolab::axonTrustScale(axon.trustBelieve);
      REQUIRE(feed == Catch::Approx(1.0f).margin(0.04f));
      REQUIRE(believe == Catch::Approx(1.0f).margin(0.04f));
    }
  }
}

TEST_CASE("zero trust axons are pruned structurally", "[twomouth]") {
  evolab::Organism organism =
      evolab::makeTwoMouthOrganism(1, 0.0f, 0.0f, 1.0f, 100, 0, evolab::kWorldCellSize);
  REQUIRE(organism.neuralAxons.size() == 2);

  organism.neuralAxons[0].trustBelieve = 0;
  organism.neuralAxons[0].trustFeed = 0;
  organism.pruneNeuralAxons();

  REQUIRE(organism.neuralAxons.size() == 1);
  organism.neuralAxons[0].trustBelieve = 50;
  organism.neuralAxons[0].trustFeed = 0;
  organism.pruneNeuralAxons();
  REQUIRE(organism.neuralAxons.size() == 1);
}
