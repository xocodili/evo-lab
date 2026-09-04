#include "sim/BarrenWorld.hpp"
#include "sim/CellConstants.hpp"
#include "sim/CellPopulation.hpp"
#include "sim/Energon.hpp"
#include "sim/EnergonString.hpp"
#include "game/OrganismInspector.hpp"
#include "sim/Organism.hpp"
#include "sim/TideAdvection.hpp"
#include "sim/WorldConstants.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>

namespace {

evolab::EnergonBlob makeWetFoodBlob(float x, float z, std::uint8_t bytes, std::uint32_t id,
                                    float halfSpan = 0.6f) {
  evolab::EnergonBlob blob;
  blob.id = id;
  blob.initialBytes = bytes;
  blob.remaining = bytes;
  const int packed = std::min(static_cast<int>(bytes), evolab::kEnergonMaxBytesPerBlob);
  for (int i = 0; i < packed; ++i) {
    blob.bytes[i] = static_cast<std::uint8_t>(0xA0u + static_cast<std::uint8_t>(i));
  }
  blob.x = x;
  blob.z = z;
  blob.y = 1.0f;
  blob.grounded = true;
  blob.onWet = true;
  blob.ttl = 40.0f;
  if (bytes <= 1) {
    evolab::energonBlobInitPoint(blob);
  } else {
    blob.tailX = x - halfSpan;
    blob.tailZ = z;
    blob.headX = x + halfSpan;
    blob.headZ = z;
  }
  return blob;
}

}  // namespace

TEST_CASE("star skeleton couples nodes links and mouth neurons", "[skeleton]") {
  evolab::Organism organism =
      evolab::makeStarMouthOrganism(1, 0.0f, 0.0f, 1.0f, 100, 0, 3, evolab::kWorldCellSize * 0.45f);

  REQUIRE(organism.nodes.size() == 4);
  REQUIRE(organism.links.size() == 3);
  REQUIRE(organism.mouthCount() == 3);
  REQUIRE(organism.links[0].parentNodeId == organism.rootNodeId);
  REQUIRE(organism.links[0].childNodeId != organism.rootNodeId);
}

TEST_CASE("kinematic forward pass places rim mouths on bone spokes", "[skeleton]") {
  evolab::BarrenWorld world(42, 32);
  evolab::Organism organism =
      evolab::makeStarMouthOrganism(1, 0.0f, 0.0f, 1.0f, 100, 0, 4, 1.0f);
  organism.updateKinematics(world, evolab::kWorldCellSize, evolab::kTerrainHeightScale);

  const evolab::SkeletonNode* root = organism.findNode(organism.rootNodeId);
  REQUIRE(root != nullptr);

  int rimCount = 0;
  for (const evolab::SkeletonLink& link : organism.links) {
    const evolab::SkeletonNode* child = organism.findNode(link.childNodeId);
    REQUIRE(child != nullptr);
    const float dx = child->worldX - root->worldX;
    const float dz = child->worldZ - root->worldZ;
    const float dist = std::sqrt(dx * dx + dz * dz);
    REQUIRE(dist == Catch::Approx(link.restLength).margin(0.05f));
    ++rimCount;
  }
  REQUIRE(rimCount == 4);
}

TEST_CASE("skeleton heading rotates all spokes rigidly", "[skeleton]") {
  evolab::BarrenWorld world(42, 32);
  evolab::Organism organism =
      evolab::makeStarMouthOrganism(1, 0.0f, 0.0f, 1.0f, 100, 0, 1, 1.0f);

  organism.heading = 0.0f;
  organism.updateKinematics(world, evolab::kWorldCellSize, evolab::kTerrainHeightScale);
  const evolab::SkeletonNode* mouthAtZero = nullptr;
  for (const evolab::SkeletonNode& node : organism.nodes) {
    if (node.neuron == evolab::NeuronType::Mouth) {
      mouthAtZero = &node;
    }
  }
  REQUIRE(mouthAtZero != nullptr);
  REQUIRE(mouthAtZero->worldX == Catch::Approx(0.0f).margin(0.05f));
  REQUIRE(mouthAtZero->worldZ == Catch::Approx(1.0f).margin(0.05f));

  organism.heading = 1.5707963f;
  organism.updateKinematics(world, evolab::kWorldCellSize, evolab::kTerrainHeightScale);
  REQUIRE(mouthAtZero->worldX == Catch::Approx(1.0f).margin(0.05f));
  REQUIRE(mouthAtZero->worldZ == Catch::Approx(0.0f).margin(0.05f));
}

TEST_CASE("heading turns toward nearby wet food", "[skeleton]") {
  evolab::BarrenWorld world(1, 32);
  evolab::EnergonField field(1, {});
  evolab::Organism organism =
      evolab::makeStarMouthOrganism(1, 0.0f, 0.0f, 1.0f, 100, 0, 1, 1.0f);
  organism.heading = 0.0f;

  evolab::EnergonBlob blob = makeWetFoodBlob(3.0f, 0.0f, 2, 1, 0.4f);
  field.injectBlob(blob);

  evolab::AdvectionVelocity still{};
  still.active = false;
  for (int i = 0; i < 40; ++i) {
    organism.advectRoot(world, field, evolab::kWorldCellSize, evolab::kTerrainHeightScale, 32.0f);
  }

  REQUIRE(organism.heading == Catch::Approx(1.5707963f).margin(0.25f));
}

TEST_CASE("energon bite removes low byte and decrements remaining", "[mouth]") {
  evolab::EnergonField field(1, {});
  evolab::EnergonBlob blob = makeWetFoodBlob(0.0f, 0.0f, 40, 10);
  field.injectBlob(blob);

  const evolab::EnergonBiteResult first = field.biteOneByte(10);
  REQUIRE(first.tookByte);
  REQUIRE(first.byte == 0xA0);
  REQUIRE(first.byteCount == evolab::kChompFieldBytes);
  REQUIRE(field.blobs().front().remaining == 40 - evolab::kChompFieldBytes);
}

TEST_CASE("mouth node bite credits local store on skeleton", "[mouth]") {
  evolab::BarrenWorld world(42, 32);
  evolab::EnergonConfig config;
  config.spawnRateMax = 0.0f;
  evolab::EnergonField field(1, config);

  evolab::Organism organism =
      evolab::makeStarMouthOrganism(1, 0.0f, 0.0f, 1.0f, 100, 0, 1, evolab::kWorldCellSize * 0.45f);
  organism.updateKinematics(world, evolab::kWorldCellSize, evolab::kTerrainHeightScale);

  evolab::SkeletonNode* mouth = nullptr;
  for (evolab::SkeletonNode& node : organism.nodes) {
    if (node.neuron == evolab::NeuronType::Mouth) {
      mouth = &node;
    }
  }
  REQUIRE(mouth != nullptr);
  field.injectBlob(makeWetFoodBlob(mouth->worldX, mouth->worldZ, evolab::kChompFieldBytes, 1, 0.2f));

  organism.feed(field, evolab::kWorldCellSize);
  REQUIRE(mouth->store.size() == evolab::kChompNetYieldBytes);
}

TEST_CASE("axon link transfers rim store toward root body", "[skeleton]") {
  evolab::BarrenWorld world(1, 32);
  evolab::EnergonField field(1, {});
  evolab::Organism organism =
      evolab::makeStarMouthOrganism(1, 0.0f, 0.0f, 1.0f, 10, 0, 1, 1.0f);
  organism.updateKinematics(world, evolab::kWorldCellSize, evolab::kTerrainHeightScale);

  for (evolab::SkeletonNode& node : organism.nodes) {
    if (node.neuron == evolab::NeuronType::Mouth) {
      node.store.push_back(1);
      node.store.push_back(1);
    }
  }

  const std::size_t bodyBefore = organism.findNode(organism.rootNodeId)->store.size();
  organism.transferEnergy(field, evolab::kWorldCellSize);
  REQUIRE(organism.findNode(organism.rootNodeId)->store.size() > bodyBefore);
}

TEST_CASE("middle bite snips one energon string into two fragments", "[mouth]") {
  evolab::EnergonField field(1, {});
  field.injectBlob(makeWetFoodBlob(0.0f, 0.0f, 65, 7, 3.0f));

  const evolab::EnergonBiteResult bite = field.biteAt(7, 0.0f, 0.0f);
  REQUIRE(bite.tookByte);
  REQUIRE(bite.snipped);
  REQUIRE(field.activeCount() == 2);
}

TEST_CASE("architecture label describes kinetic skeleton", "[skeleton]") {
  evolab::Organism organism =
      evolab::makeStarMouthOrganism(2, 0.0f, 0.0f, 1.0f, 10, 0, 2, 1.0f);
  const std::string label = evolab::game::formatOrganismArchitectureLabel(organism);
  REQUIRE(label.find("kinetic mouth") != std::string::npos);
  REQUIRE(label.find("Links: 2") != std::string::npos);
}

TEST_CASE("population stats count kinetic mouth organisms", "[skeleton]") {
  evolab::BarrenWorld world(3, 64);
  evolab::CellPopulation population;
  population.seedMouthOrganisms(world, evolab::kWorldCellSize, evolab::kTerrainHeightScale, 4, 2, 2);
  const evolab::CellPopulationStats stats = population.stats();
  REQUIRE(stats.mouthOrganisms == 4);
  REQUIRE(stats.mouthNeurons == 8);
  REQUIRE(stats.skeletonLinks == 8);
}
