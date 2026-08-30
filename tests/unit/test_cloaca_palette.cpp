#include "sim/CellConstants.hpp"
#include "sim/CloacaSignal.hpp"
#include "sim/NeuronFuel.hpp"
#include "sim/Organism.hpp"
#include "sim/WorldConstants.hpp"

#include <catch2/catch_test_macros.hpp>

#include <random>

TEST_CASE("cloaca palette bands are heritable with ordered jitter", "[cloaca][information]") {
  evolab::Organism parent =
      evolab::makeCampNomOrganism(1, 0.0f, 0.0f, 1.0f, evolab::kTicksPerStemCellDay, 0,
                                  evolab::kWorldCellSize);
  std::mt19937 rng(42);
  parent.finalizeSpawn(rng);

  evolab::Organism child = parent;
  child.id = 2;
  evolab::jitterCloacaPaletteBytes(child, rng);

  REQUIRE(child.cloacaDistressByte > 0);
  REQUIRE(child.cloacaBaselineByte > child.cloacaDistressByte);
  REQUIRE(child.cloacaMateByte > child.cloacaBaselineByte);
  REQUIRE(child.cloacaDistressByte >= evolab::kCloacaPaletteMinTierGap);
  REQUIRE(evolab::cloacaBandFromTag(child.cloacaDistressByte) == evolab::CloacaBand::Distress);
  REQUIRE(evolab::cloacaBandFromTag(child.cloacaBaselineByte) == evolab::CloacaBand::Baseline);
  REQUIRE(evolab::cloacaBandFromTag(child.cloacaMateByte) == evolab::CloacaBand::Mate);
}

TEST_CASE("organism vents its own jittered cloaca palette byte", "[cloaca][information]") {
  evolab::EnergonField field(1, {});
  evolab::Organism organism =
      evolab::makeCampNomOrganism(1, 0.0f, 0.0f, 1.0f, evolab::kTicksPerStemCellDay * 2, 0, 1.0f);
  std::mt19937 rng(7);
  organism.finalizeSpawn(rng);
  organism.cloacaDistressByte = 40;
  organism.cloacaBaselineByte = 130;
  organism.cloacaMateByte = 230;

  evolab::SkeletonNode* computer = organism.findNode(3);
  REQUIRE(computer != nullptr);
  for (int i = 0; i < 200; ++i) {
    evolab::hubStorePush(organism, 1);
  }

  REQUIRE(evolab::expelCloacaVent(organism, field, *computer, evolab::CloacaBand::Distress));
  REQUIRE(field.activeCount() == 1);
  REQUIRE(field.blobs()[0].origin == evolab::EnergonOrigin::Cloaca);
  REQUIRE(static_cast<std::uint8_t>(field.blobs()[0].data & 0xFFu) == 40);
}
