#include "sim/EnergonConveyance.hpp"
#include "sim/EnergonInformation.hpp"
#include "sim/NeuronFuel.hpp"
#include "sim/NeuralAxon.hpp"
#include "sim/Organism.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

TEST_CASE("palette information increases with warm byte ordinal", "[energon][information]") {
  REQUIRE(evolab::energonInformationValue(0) == Catch::Approx(0.0f));
  REQUIRE(evolab::energonInformationValue(255) == Catch::Approx(1.0f));
  REQUIRE(evolab::energonInformationValue(32) <
          evolab::energonInformationValue(evolab::kEnergonPaletteMate));
}

TEST_CASE("entropy decay cools bytes down the palette", "[energon][information]") {
  REQUIRE(evolab::energonDecayByte(40, 5) == 35);
  REQUIRE(evolab::energonDecayByte(3, 5) == 0);
}

TEST_CASE("axon hop cooling lowers byte value without dropping count at eta 1", "[energon][information]") {
  const std::uint8_t warm = 200;
  REQUIRE(evolab::energonHopCoolByte(warm, 1.0f) == warm);
  REQUIRE(evolab::energonHopCoolByte(warm, 0.5f) < warm);
  REQUIRE(evolab::energonHopCoolByte(warm, 0.5f) > 0);
}

TEST_CASE("conveyance hop applies thermal cooling to delivered bytes", "[energon][information]") {
  evolab::EnergonField field(1, {});
  evolab::Organism organism =
      evolab::makeCampNomOrganism(1, 0.0f, 0.0f, 1.0f, 100, 0, 1.0f);
  organism.alive = true;

  evolab::SkeletonNode* mouth = organism.findNode(2);
  evolab::SkeletonNode* actuator = organism.findNode(4);
  REQUIRE(mouth != nullptr);
  REQUIRE(actuator != nullptr);

  evolab::NeuralAxon* mToA = organism.findNeuralAxon(2, 4);
  REQUIRE(mToA != nullptr);
  mToA->trustFeed = evolab::kTrustBaseline;
  mToA->etaEnergy = 0.88f;
  mToA->etaSignal = 1.0f;

  constexpr std::uint8_t kWarmByte = 220;
  for (int i = 0; i < static_cast<int>(evolab::kMouthConveyReserveBytes + 4); ++i) {
    evolab::neuronStorePush(organism, *mouth, kWarmByte);
  }
  actuator->store.clear();

  evolab::conveyMouthDownstream(organism, field, 1);
  REQUIRE_FALSE(actuator->store.empty());
  REQUIRE(actuator->store.back() < kWarmByte);
  REQUIRE(actuator->store.back() > 0);
}
