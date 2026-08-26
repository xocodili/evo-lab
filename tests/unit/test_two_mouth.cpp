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

void fillMouthStoreToPressure(evolab::SkeletonNode& mouth, std::uint8_t byte) {
  while (mouth.store.size() < evolab::kMouthStoreSoftPressureBytes) {
    mouth.store.push_back(byte);
  }
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

TEST_CASE("mouth bite stores byte locally without axon fire", "[twomouth]") {
  evolab::BarrenWorld world(1, 32);
  evolab::EnergonField field(1, {});
  evolab::Organism organism =
      evolab::makeTwoMouthOrganism(1, 0.0f, 0.0f, 1.0f, 100, 0, evolab::kWorldCellSize);
  organism.updateKinematics(world, evolab::kWorldCellSize, evolab::kTerrainHeightScale);

  evolab::SkeletonNode* m1 = organism.findNode(1);
  evolab::SkeletonNode* m2 = organism.findNode(2);
  REQUIRE(m1 != nullptr);
  REQUIRE(m2 != nullptr);
  field.injectBlob(makeWetFoodBlob(m1->worldX, m1->worldZ, 3, 1));

  organism.feed(field, evolab::kWorldCellSize);
  REQUIRE(m1->store.size() == 1);
  REQUIRE(m1->store.front() == 0xB0);

  organism.signal(field, 42);
  REQUIRE(m2->store.empty());
  const evolab::NeuralAxon* axon12 = organism.findNeuralAxon(1, 2);
  REQUIRE(axon12 != nullptr);
  REQUIRE_FALSE(axon12->lastReceived.valid);
}

TEST_CASE("mouth forwards signal tag and feed burst under store pressure", "[twomouth]") {
  evolab::BarrenWorld world(1, 32);
  evolab::EnergonField field(1, {});
  evolab::Organism organism =
      evolab::makeTwoMouthOrganism(1, 0.0f, 0.0f, 1.0f, 100, 0, evolab::kWorldCellSize);
  organism.updateKinematics(world, evolab::kWorldCellSize, evolab::kTerrainHeightScale);

  evolab::SkeletonNode* m1 = organism.findNode(1);
  evolab::SkeletonNode* m2 = organism.findNode(2);
  REQUIRE(m1 != nullptr);
  REQUIRE(m2 != nullptr);

  fillMouthStoreToPressure(*m1, 0xB0);
  organism.signal(field, 42);

  const evolab::NeuralAxon* axon12 = organism.findNeuralAxon(1, 2);
  REQUIRE(axon12 != nullptr);
  REQUIRE(axon12->lastReceived.valid);
  REQUIRE(axon12->lastReceived.byte == evolab::kMouthSignalTagShipping);
  REQUIRE(axon12->lastSentByte == evolab::kMouthSignalTagShipping);
  REQUIRE(!m2->store.empty());
  REQUIRE(m1->store.size() < evolab::kMouthStoreSoftPressureBytes);
}

TEST_CASE("neural axon feed bandwidth scales with trust feed", "[twomouth]") {
  evolab::Organism organism =
      evolab::makeTwoMouthOrganism(1, 0.0f, 0.0f, 1.0f, 100, 0, evolab::kWorldCellSize, 256, 256);
  evolab::NeuralAxon* axon = organism.findNeuralAxon(1, 2);
  REQUIRE(axon != nullptr);

  const int fullBandwidth = evolab::axonFeedBandwidth(*axon);
  REQUIRE(fullBandwidth > 1);
  REQUIRE(fullBandwidth <= static_cast<int>(evolab::kAxonChannelCapacity));

  axon->trustFeed = evolab::kTrustMin;
  const int lowBandwidth = evolab::axonFeedBandwidth(*axon);
  REQUIRE(lowBandwidth < fullBandwidth);
}

TEST_CASE("mouth signal forwards store bytes when trust feed allows", "[twomouth]") {
  evolab::Organism organism =
      evolab::makeTwoMouthOrganism(1, 0.0f, 0.0f, 1.0f, 100, 0, evolab::kWorldCellSize, 320, 320);
  evolab::SkeletonNode* m1 = organism.findNode(1);
  evolab::SkeletonNode* m2 = organism.findNode(2);
  REQUIRE(m1 != nullptr);
  REQUIRE(m2 != nullptr);

  fillMouthStoreToPressure(*m1, 0xA0);
  for (int i = 0; i < 6; ++i) {
    m1->store.push_back(static_cast<std::uint8_t>(0xA0 + i));
  }

  evolab::EnergonField field(1, {});
  organism.signal(field, 99);

  REQUIRE(m1->store.size() < 14);
  REQUIRE(!m2->store.empty());
}

TEST_CASE("full tick loop forwards between mouths while eating", "[twomouth][integration]") {
  evolab::BarrenWorld world(1, 32);
  evolab::EnergonConfig config;
  config.spawnRateMax = 0.0f;
  evolab::EnergonField field(1, config);
  evolab::Organism organism =
      evolab::makeTwoMouthOrganism(1, 0.0f, 0.0f, 1.0f, evolab::kTicksPerStemCellDay, 0,
                                   evolab::kWorldCellSize);
  organism.updateKinematics(world, evolab::kWorldCellSize, evolab::kTerrainHeightScale);

  evolab::SkeletonNode* m1 = organism.findNode(1);
  REQUIRE(m1 != nullptr);
  field.injectBlob(makeWetFoodBlob(m1->worldX, m1->worldZ, 40, 1));

  bool sawPartnerReceive = false;
  for (int tick = 0; tick < 20; ++tick) {
    organism.metabolise(world, evolab::kWorldCellSize, evolab::kTerrainHeightScale);
    organism.feed(field, evolab::kWorldCellSize);
    organism.signal(field, static_cast<std::uint64_t>(tick));

    const evolab::SkeletonNode* m2 = organism.findNode(2);
    const evolab::NeuralAxon* axon12 = organism.findNeuralAxon(1, 2);
    if (m2 != nullptr && m2->store.size() > 0) {
      sawPartnerReceive = true;
    }
    if (axon12 != nullptr && axon12->lastReceived.valid) {
      sawPartnerReceive = true;
    }
  }

  REQUIRE(sawPartnerReceive);
}

TEST_CASE("received overflow spits to world at receiving mouth", "[twomouth]") {
  evolab::Organism organism =
      evolab::makeTwoMouthOrganism(1, 0.0f, 0.0f, 1.0f, 100, 0, evolab::kWorldCellSize);
  evolab::SkeletonNode* m1 = organism.findNode(1);
  evolab::SkeletonNode* m2 = organism.findNode(2);
  REQUIRE(m1 != nullptr);
  REQUIRE(m2 != nullptr);

  m2->store.resize(evolab::kMouthLocalStoreMaxBytes, 0x11);
  fillMouthStoreToPressure(*m1, 0x22);

  evolab::EnergonField field(1, {});
  const int blobsBefore = field.activeCount();
  organism.signal(field, 42);
  REQUIRE(field.activeCount() > blobsBefore);
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

  const float minFeedScale =
      static_cast<float>(evolab::kTrustMin) / static_cast<float>(evolab::kTrustBaseline);
  for (const evolab::Organism& organism : population.organisms()) {
    for (const evolab::NeuralAxon& axon : organism.neuralAxons) {
      const float feed = evolab::axonTrustScale(axon.trustFeed);
      const float believe = evolab::axonTrustScale(axon.trustBelieve);
      REQUIRE(feed == Catch::Approx(minFeedScale).margin(0.04f));
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
